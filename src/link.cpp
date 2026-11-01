// Getakteter Halbduplex-Link ueber D+/D- (Protokoll siehe link.h).
//
// Beide Seiten arbeiten im Timer-Interrupt: der Host stoesst alle PERIOD_US
// einen Austausch an, der Gast schaut alle POLL_US nach, ob angeklopft wurde.
// Die Spiellogik muss dafuer nichts tun und darf beliebig lange zeichnen oder
// warten - der Link laeuft nebenher weiter.
//
// Fehlercodes (Stats::lastFail):
//   Host:  1 CLK schon hoch (Gegenseite ist auch Host)
//          2 Gast gab DATA nicht frei       3 Gast antwortete nicht
//          4 falsches Sync-Byte             5 unsinnige Laenge
//          6 CRC der Antwort falsch         7 DATA schon vor dem Anklopfen hoch
//          8 DATA blieb nach dem Freigeben hoch
//   Gast: 12 Host senkte CLK nicht         13 Taktflanke fehlt beim Empfang
//         14 falsches Sync-Byte            15 unsinnige Laenge
//         16 CRC falsch                    17 Taktflanke fehlt beim Senden

#include "link.h"

#include "pico/stdlib.h"
#include "hardware/structs/usb.h"
#include "hardware/regs/usb.h"
#include "hardware/resets.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include <string.h>

// Alles, was Flanken erzeugt oder zaehlt, laeuft aus dem RAM: ein Cache-Fehlgriff
// im XIP-Flash kostet bei 50 MHz Systemtakt mehrere Mikrosekunden und wuerde
// eine Taktphase verschieben. "no_inline", sonst bettet GCC sie in die
// (im Flash liegenden) Timer-Callbacks ein und das Attribut ist wirkungslos.
#define RAM(f) __no_inline_not_in_flash_func(f)

namespace usblink {
namespace {

constexpr uint8_t SYNC = 0xA5;
constexpr int32_t PERIOD_US = 15000;   // Host: Pause zwischen zwei Austauschen
constexpr int32_t POLL_US = 250;       // Gast: so oft wird nach CLK geschaut
constexpr uint32_t ATTN_US = 800;      // Host wartet so lange auf "bereit"
constexpr uint32_t READY_US = 1500;    // Gast haelt "bereit" hoechstens so lange
constexpr uint32_t STEP_US = 2000;     // einzelne Handshake-Schritte
constexpr uint32_t EDGE_US = 300;      // Gast wartet auf die naechste Taktflanke
constexpr uint32_t TURN_NS = 5000;     // Gast: Pause vor dem Richtungswechsel
constexpr uint32_t HALF_DEFAULT = 2000, HALF_MAX = 5000;

// CLK = D- (DM), DATA = D+ (DP)
constexpr uint32_t CLK_TX = USB_USBPHY_DIRECT_TX_DM_BITS;
constexpr uint32_t CLK_OE = USB_USBPHY_DIRECT_TX_DM_OE_BITS;
constexpr uint32_t CLK_RX = USB_USBPHY_DIRECT_RX_DM_BITS;
constexpr uint32_t DATA_TX = USB_USBPHY_DIRECT_TX_DP_BITS;
constexpr uint32_t DATA_OE = USB_USBPHY_DIRECT_TX_DP_OE_BITS;
constexpr uint32_t DATA_RX = USB_USBPHY_DIRECT_RX_DP_BITS;

// Ruhe: nichts getrieben, beide Leitungen ueber die Pulldowns auf low.
constexpr uint32_t PHY_IDLE = USB_USBPHY_DIRECT_DP_PULLDN_EN_BITS | USB_USBPHY_DIRECT_DM_PULLDN_EN_BITS;
// Alles, was wir dem USB-Controller abnehmen. TX_PD/RX_PD bleiben 0 (Sender und
// Empfaenger an), TX_DIFFMODE 0 (einzeln treiben), Pullups aus, Pulldowns an.
constexpr uint32_t PHY_OVERRIDE =
    USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OE_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OE_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_TX_DIFFMODE_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_TX_PD_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_RX_PD_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_DP_PULLDN_EN_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_DM_PULLDN_EN_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_EN_OVERRIDE_EN_BITS |
    USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_OVERRIDE_EN_BITS;

bool active, hostRole;
repeating_timer timer;
uint32_t shadow;
uint32_t halfNs = HALF_DEFAULT, halfCycles, turnCycles;

uint8_t txBuf[M2S_MAX];
int txLen;
uint8_t rxBuf[M2S_MAX];
int rxLen;
volatile uint32_t rxCnt;
volatile uint32_t lastOkMs;
Stats st;
uint32_t window, windowBad;          // Host: Fehlerquote fuer das Abbremsen

// Rahmen-Puffer der Bitschleifen (statisch, damit sie nicht auf dem Stapel liegen)
uint8_t frame[2 + M2S_MAX + 2];

__force_inline void phy(uint32_t v) { shadow = v; usb_hw->phy_direct = v; }
__force_inline bool clkIn() { return (usb_hw->phy_direct & CLK_RX) != 0; }
__force_inline bool dataIn() { return (usb_hw->phy_direct & DATA_RX) != 0; }
__force_inline void clkSet(bool on) { phy(on ? (shadow | CLK_TX) : (shadow & ~CLK_TX)); }
__force_inline void clkDrive() { phy((shadow | CLK_OE) & ~CLK_TX); }
__force_inline void clkRelease() { phy(shadow & ~(CLK_OE | CLK_TX)); }
__force_inline void dataDrive(bool on) {
    uint32_t v = shadow | DATA_OE;
    phy(on ? (v | DATA_TX) : (v & ~DATA_TX));
}
__force_inline void dataRelease() { phy(shadow & ~(DATA_OE | DATA_TX)); }
__force_inline void halfWait() { busy_wait_at_least_cycles(halfCycles); }

void fail(uint8_t code) { st.lastFail = code; }

bool RAM(waitData)(bool level, uint32_t us) {
    uint32_t t0 = time_us_32();
    while (dataIn() != level)
        if (time_us_32() - t0 > us) return false;
    return true;
}

// Auf einen CLK-Pegel warten, so knapp wie moeglich: ein Registerlesen je
// Durchlauf (CLK und DATA stecken im selben Register - die Momentaufnahme wird
// zurueckgegeben), die Uhr nur alle 64 Durchlaeufe befragen.
bool RAM(waitClk)(bool level, uint32_t us, uint32_t* sample) {
    uint32_t guard = 0, t0 = time_us_32();
    for (;;) {
        uint32_t v = usb_hw->phy_direct;
        if (((v & CLK_RX) != 0) == level) { *sample = v; return true; }
        if (!(++guard & 63) && time_us_32() - t0 > us) return false;
    }
}

uint16_t RAM(crc16)(const uint8_t* p, int n) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

// ---- Host (Master) ------------------------------------------------------
void RAM(mSendByte)(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        dataDrive((b >> i) & 1);
        halfWait();
        clkSet(true);
        halfWait();
        clkSet(false);
        halfWait();                    // DATA stabil halten, bis der Gast gelesen hat
    }
}

// Der Gast legt sein Bit an, sobald CLK low ist; er braucht dafuer etwas Zeit,
// deshalb drei halbe Bitzeiten Vorlauf.
uint8_t RAM(mRecvByte)() {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        halfWait(); halfWait(); halfWait();
        clkSet(true);
        halfWait();
        b = (uint8_t)((b << 1) | (dataIn() ? 1 : 0));
        clkSet(false);
    }
    return b;
}

// Zeitkritischer Teil; CLK ist von uns getrieben und hoch, der Gast hat DATA
// schon auf "bereit". Liefert die Laenge der Antwort oder < 0.
// Den Rahmen vorher fertig bauen: sobald CLK faellt, wartet der Gast nur
// EDGE_US auf die erste Flanke - fuer die CRC ist dann keine Zeit mehr.
int RAM(buildFrame)(const uint8_t* d, int n) {
    frame[0] = SYNC;
    frame[1] = (uint8_t)n;
    memcpy(frame + 2, d, n);
    uint16_t crc = crc16(frame, 2 + n);
    frame[2 + n] = (uint8_t)crc;
    frame[3 + n] = (uint8_t)(crc >> 8);
    return 4 + n;
}

int RAM(masterExchange)(int frameLen, uint8_t* rx) {
    uint32_t irq = save_and_disable_interrupts();
    int rc = -1;
    clkSet(false);
    if (!waitData(false, STEP_US)) { fail(2); goto timeout; }
    for (int i = 0; i < frameLen; i++) mSendByte(frame[i]);

    // Richtungswechsel: erst abwarten, bis die Leitung wirklich unten ist. Sonst
    // liest man den Rest des EIGENEN letzten 1-Bits (die Leitung entlaedt sich
    // nur ueber den Pulldown) als "Gast sendet schon".
    dataRelease();
    if (!waitData(false, STEP_US)) { fail(8); goto timeout; }
    if (!waitData(true, STEP_US)) { fail(3); goto timeout; }   // erstes Bit von 0xA5 ist 1
    {
        frame[0] = mRecvByte();
        if (frame[0] != SYNC) { fail(4); goto bad; }
        int len = mRecvByte();
        if (len > S2M_MAX) { fail(5); goto bad; }
        frame[1] = (uint8_t)len;
        for (int i = 0; i < len; i++) frame[2 + i] = mRecvByte();
        uint16_t crc = mRecvByte();
        crc |= (uint16_t)mRecvByte() << 8;
        if (crc != crc16(frame, 2 + len)) { fail(6); goto bad; }
        memcpy(rx, frame + 2, len);
        rc = len;
    }
    clkRelease();
    restore_interrupts(irq);
    return rc;
bad:
    rc = -2;
timeout:
    clkSet(false);
    clkRelease();
    dataRelease();
    restore_interrupts(irq);
    return rc;
}

void noteResult(int rc) {
    if (rc >= 0) {
        st.ok++;
        st.lastFail = 0;
        lastOkMs = to_ms_since_boot(get_absolute_time());
        rxCnt++;
    } else if (rc == -2) st.bad++;
    else st.timeout++;
    // Host: haeufen sich Fehler mitten im Austausch, langsamer takten.
    if (hostRole) {
        window++;
        if (rc < 0) windowBad++;
        if (window >= 32) {
            if (windowBad >= 4 && halfNs < HALF_MAX) setHalfBitNs(halfNs + 500);
            window = windowBad = 0;
        }
    }
}

bool hostTick(repeating_timer*) {
    uint32_t irq = save_and_disable_interrupts();
    if (clkIn()) {                         // jemand anderes treibt CLK
        restore_interrupts(irq);
        st.conflict++; fail(1);
        return true;
    }
    // DATA schon hoch, BEVOR wir anklopfen: der Gast haengt noch in einem
    // alten Austausch. Sein altes "bereit" duerfen wir nicht als Antwort lesen.
    if (dataIn()) {
        restore_interrupts(irq);
        fail(7);
        return true;
    }
    restore_interrupts(irq);
    int frameLen = buildFrame(txBuf, txLen);
    irq = save_and_disable_interrupts();
    clkDrive();
    clkSet(true);
    restore_interrupts(irq);

    uint32_t t0 = time_us_32();
    while (!dataIn()) {
        if (time_us_32() - t0 > ATTN_US) {
            irq = save_and_disable_interrupts();
            clkSet(false);
            clkRelease();
            restore_interrupts(irq);
            st.noPeer++;
            return true;
        }
    }
    static uint8_t rx[S2M_MAX];
    int n = masterExchange(frameLen, rx);
    if (n >= 0) { memcpy(rxBuf, rx, n); rxLen = n; }
    noteResult(n);
    return true;
}

// ---- Gast (Slave) -------------------------------------------------------
uint8_t RAM(sRecvByte)(bool* ok) {
    uint8_t b = 0;
    uint32_t v;
    for (int i = 0; i < 8; i++) {
        if (!waitClk(true, EDGE_US, &v)) { *ok = false; return 0; }
        b = (uint8_t)((b << 1) | ((v & DATA_RX) ? 1 : 0));   // DATA aus derselben Momentaufnahme
        if (!waitClk(false, EDGE_US, &v)) { *ok = false; return 0; }
    }
    return b;
}

bool RAM(sSendByte)(uint8_t b) {
    uint32_t v;
    for (int i = 7; i >= 0; i--) {
        dataDrive((b >> i) & 1);
        if (!waitClk(true, EDGE_US, &v)) return false;
        if (!waitClk(false, EDGE_US, &v)) return false;
    }
    return true;
}

int RAM(slaveExchange)(uint8_t* rx) {
    uint32_t irq = save_and_disable_interrupts();
    uint32_t v;
    int rc = -1, len = 0;
    bool ok = true;
    if (!clkIn()) { restore_interrupts(irq); return -4; }

    dataDrive(true);                                   // bereit
    if (!waitClk(false, READY_US, &v)) { fail(12); goto out; }
    dataRelease();

    frame[0] = sRecvByte(&ok);
    if (!ok) { fail(13); goto out; }
    if (frame[0] != SYNC) { fail(14); rc = -2; goto out; }
    frame[1] = sRecvByte(&ok);
    if (!ok) { fail(13); goto out; }
    len = frame[1];
    if (len > M2S_MAX) { fail(15); rc = -2; goto out; }
    for (int i = 0; i < len && ok; i++) frame[2 + i] = sRecvByte(&ok);
    if (ok) {
        uint16_t crc = sRecvByte(&ok);
        crc |= (uint16_t)sRecvByte(&ok) << 8;
        if (ok && crc != crc16(frame, 2 + len)) { fail(16); rc = -2; goto out; }
    }
    if (!ok) { fail(13); goto out; }
    memcpy(rx, frame + 2, len);

    busy_wait_at_least_cycles(turnCycles);
    {
        int n = buildFrame(txBuf, txLen);      // Antwort ist kurz, die CRC schnell
        for (int i = 0; i < n; i++)
            if (!sSendByte(frame[i])) { fail(17); goto out; }
    }
    rc = len;
out:
    dataRelease();
    restore_interrupts(irq);
    return rc;
}

bool guestPoll(repeating_timer*) {
    if (!clkIn()) return true;
    static uint8_t rx[M2S_MAX];
    int n = slaveExchange(rx);
    if (n == -4) return true;
    if (n >= 0) { memcpy(rxBuf, rx, n); rxLen = n; }
    noteResult(n);
    return true;
}

void phyTakeover() {
    // USB-Block aus dem Reset holen; die Serial-Engine bleibt untaetig.
    reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    phy(PHY_IDLE);
    usb_hw->phy_direct_override = PHY_OVERRIDE;
}

}  // namespace

void start(bool host) {
    if (active) stop();
    phyTakeover();
    hostRole = host;
    txLen = rxLen = 0;
    rxCnt = 0;
    lastOkMs = to_ms_since_boot(get_absolute_time()) - 100000;
    memset(&st, 0, sizeof st);
    window = windowBad = 0;
    setHalfBitNs(halfNs);
    turnCycles = (uint32_t)((uint64_t)clock_get_hz(clk_sys) * TURN_NS / 1000000000u);
    active = true;
    if (host) add_repeating_timer_us(PERIOD_US, hostTick, nullptr, &timer);
    else add_repeating_timer_us(POLL_US, guestPoll, nullptr, &timer);
}

void stop() {
    if (!active) return;
    cancel_repeating_timer(&timer);
    // Leitungen nicht dem (untaetigen) USB-Controller zurueckgeben - dann
    // schweben sie. Ueber die Pulldowns bleiben sie definiert low, und die
    // Gegenseite sieht kein Phantom-Anklopfen.
    phy(PHY_IDLE);
    active = false;
}

bool running() { return active; }
bool isHost() { return hostRole; }

void setTx(const uint8_t* d, int n) {
    int cap = hostRole ? M2S_MAX : S2M_MAX;
    if (n > cap) n = cap;
    uint32_t irq = save_and_disable_interrupts();
    memcpy(txBuf, d, n);
    txLen = n;
    restore_interrupts(irq);
}

int getRx(uint8_t* d, int cap) {
    uint32_t irq = save_and_disable_interrupts();
    int n = rxLen < cap ? rxLen : cap;
    memcpy(d, rxBuf, n);
    restore_interrupts(irq);
    return n;
}

uint32_t rxCount() { return rxCnt; }

uint32_t msSinceOk() { return to_ms_since_boot(get_absolute_time()) - lastOkMs; }

void setHalfBitNs(uint32_t ns) {
    halfNs = ns;
    uint32_t c = (uint32_t)((uint64_t)clock_get_hz(clk_sys) * ns / 1000000000u);
    halfCycles = c ? c : 1;
}

uint32_t halfBitNs() { return halfNs; }

Stats stats() {
    Stats s = st;
    s.lines = active ? (uint8_t)((clkIn() ? 2 : 0) | (dataIn() ? 1 : 0)) : 0;
    return s;
}

void resetStats() {
    uint32_t irq = save_and_disable_interrupts();
    memset(&st, 0, sizeof st);
    restore_interrupts(irq);
}

}  // namespace usblink
