// Merken (W'n'R) - quer. Der Spieler am Zug sieht die bisherige Folge,
// drueckt sie nach und haengt eine Richtung an; dann ist der andere dran.
// Wer sich vertut, verliert. Die Folge sieht und hoert nur, wer dran ist.

#include <stdio.h>
#include "play.h"

namespace wnr {
namespace {

constexpr int MAX_SEQ = 100;
enum { UP, DOWN, LEFT, RIGHT, NONE = 0xFF };
enum { PH_INTRO, PH_WATCH, PH_REPLAY, PH_EXTEND, PH_P1_WINS, PH_P2_WINS, PH_DRAW };
constexpr uint32_t FLASH_ON = 400, FLASH_OFF = 200, STEP = FLASH_ON + FLASH_OFF, PRE_DELAY = 600;
constexpr uint32_t TICK_MS = 33;
const uint16_t TONE[4] = { 880, 587, 740, 1100 };
// Feldmitten: oben, unten, links, rechts (Raute um die Bildmitte)

uint8_t seq[MAX_SEQ], seqLen, progress, phase, active, match;
uint8_t watchFlash = NONE, localFlash = NONE;
uint32_t localFlashUntil, phaseStart, inputBlockUntil;

bool iAmActive() { return (active == 1) == play::host(); }
bool ended() { return phase >= PH_P1_WINS; }

void initMatch() {
    seqLen = progress = 0;
    phase = PH_INTRO;
    active = 1;
    watchFlash = localFlash = NONE;
    localFlashUntil = inputBlockUntil = 0;
}

void flash(uint8_t dir) {
    localFlash = dir;
    localFlashUntil = hw::ms() + 280;
    hw::beep(TONE[dir], 180);
}

void extendSignal() {
    hw::beep(1200, 80);
    hw::sleepMs(100);
    hw::beep(1600, 120);
}

// ---- Zeichnen (Aufbau wie W'n'R der PicoPac-Sammlung) ----
// 19x19-Pfeil nach oben aus dem Original, je Zeile ein Bitmuster (Bit 18 = links)
const uint32_t ARROW[19] = {
    0x00200, 0x00700, 0x00F80, 0x01FC0, 0x03FE0, 0x07FF0, 0x0FFF8, 0x1FFFC,
    0x00F80, 0x00F80, 0x00F80, 0x00F80, 0x00F80, 0x00F80, 0x00F80, 0x00F80,
    0x00F80, 0x00F80, 0x00F80,
};
// linke obere Ecke der Pfeile: oben, unten, links, rechts
const int AX[4] = { 55, 55, 35, 74 }, AY[4] = { 3, 43, 23, 23 };

bool arrowBit(int r, int c) { return (ARROW[r] >> (18 - c)) & 1; }

void drawBigArrow(uint8_t dir) {
    for (int i = 0; i < 19; i++)
        for (int j = 0; j < 19; j++) {
            bool on;
            switch (dir) {                      // Original um die Richtung gedreht
                case UP:   on = arrowBit(i, j); break;
                case DOWN: on = arrowBit(18 - i, j); break;
                case LEFT: on = arrowBit(j, i); break;
                default:   on = arrowBit(18 - j, i); break;
            }
            if (on) gfx::pixel(AX[dir] + j, AY[dir] + i, gfx::ON);
        }
}

void vtext(int x, int y, const char* s) {          // senkrecht, Buchstabe fuer Buchstabe
    char c[2] = { 0, 0 };
    for (; *s; s++, y += 10) { c[0] = *s; gfx::text(x, y, c); }
}

uint32_t ledOffAt;
void flashLed(hw::Led l, uint32_t ms) { hw::led(l, true); ledOffAt = hw::ms() + ms; }

void draw() {
    gfx::clear();
    if (ledOffAt && hw::ms() >= ledOffAt) {
        hw::led(hw::LED_GREEN, false); hw::led(hw::LED_RED, false); ledOffAt = 0;
    }
    if (ended()) {
        int me = play::host() ? 1 : 2;
        int winner = phase == PH_P1_WINS ? 1 : phase == PH_P2_WINS ? 2 : 3;
        char b[16];
        snprintf(b, sizeof b, "LENGTH %u", seqLen);
        gfx::messageBox(winner == 3 ? "DRAW" : winner == me ? "YOU WIN!" : "YOU LOSE", b, "P U S H !", true);
        return;
    }
    bool mine = iAmActive() && phase != PH_INTRO;
    bool input = mine && (phase == PH_REPLAY || phase == PH_EXTEND);
    uint8_t lit = NONE;
    if (mine) {
        if (hw::ms() > localFlashUntil) localFlash = NONE;
        lit = phase == PH_WATCH ? watchFlash : localFlash;
    }
    gfx::circle(64, 32, 31, gfx::ON);
    // innerer Kreis: gefuellt, wenn ich druecken soll; beim Anhaengen blinkend
    bool disc = input && !(phase == PH_EXTEND && lit == NONE && (hw::ms() / 250) & 1);
    if (disc) gfx::fillCircle(64, 32, 8, gfx::ON);
    else gfx::circle(64, 32, 8, gfx::ON);
    char b[4];
    snprintf(b, sizeof b, "%u", seqLen);
    gfx::textCenter(64, 29, b, disc ? gfx::OFF : gfx::ON);
    if (lit != NONE) drawBigArrow(lit);
    if (!mine) vtext(15, 14, "WAIT");
    else if (phase == PH_WATCH) vtext(15, 9, "WATCH");
    else if (phase == PH_REPLAY) vtext(107, 2, "REPEAT");
    else if (phase == PH_EXTEND) vtext(107, 17, "ADD");
}

// ---- Host ----
void startWatch() {
    phase = PH_WATCH;
    watchFlash = localFlash = NONE;
    phaseStart = hw::ms();
}

void startExtend() {
    phase = PH_EXTEND;
    watchFlash = localFlash = NONE;
    inputBlockUntil = hw::ms() + 600;
    if (iAmActive()) extendSignal();
}

void startReplay() {
    phase = PH_REPLAY;
    progress = 0;
    watchFlash = localFlash = NONE;
    inputBlockUntil = hw::ms() + 500;
}

void tickWatch() {
    uint32_t el = hw::ms() - phaseStart;
    if (el < PRE_DELAY) { watchFlash = NONE; return; }
    uint32_t t = el - PRE_DELAY, step = t / STEP;
    if (step >= seqLen) { startReplay(); return; }
    uint8_t prev = watchFlash;
    watchFlash = t % STEP < FLASH_ON ? seq[step] : NONE;
    if (watchFlash != prev && watchFlash != NONE && iAmActive()) hw::beep(TONE[watchFlash], 180);
}

int pack(uint8_t* b) {
    b[0] = match; b[1] = phase; b[2] = seqLen; b[3] = progress; b[4] = active; b[5] = watchFlash;
    return 6;
}

void sendNow() {
    uint8_t b[6];
    play::send(b, pack(b));
}

void handle(uint8_t dir) {
    if (phase == PH_REPLAY) {
        if (dir == seq[progress]) {
            flash(dir);
            progress++;
            inputBlockUntil = hw::ms() + 250;
            if (progress >= seqLen) {
                if (iAmActive()) flashLed(hw::LED_GREEN, 80);
                sendNow(); draw(); play::present();
                hw::sleepMs(350);
                startExtend();
            }
        } else {
            hw::beep(160, 400);
            phase = active == 1 ? PH_P2_WINS : PH_P1_WINS;
            flashLed(iAmActive() ? hw::LED_RED : hw::LED_GREEN, 400);
        }
    } else if (phase == PH_EXTEND) {
        seq[seqLen++] = dir;
        flash(dir);
        sendNow(); draw(); play::present();
        hw::sleepMs(380);
        if (seqLen >= MAX_SEQ) { phase = PH_DRAW; hw::beep(1500, 200); return; }
        active = active == 1 ? 2 : 1;
        startWatch();
    }
}

uint8_t firstDir(uint8_t e) {
    for (int k = 0; k < 4; k++) if (e & (1u << k)) return (uint8_t)k;
    return NONE;
}

}  // namespace

void host() {
    initMatch();
    match = 0;
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t lt = hw::ms(), endAt = 0;
    bool started = false;
    while (play::running()) {
        uint32_t now = hw::ms();
        if (play::live() && now - lt >= TICK_MS) {
            lt = now;
            uint8_t k1 = e1.take(play::pad(1)), k2 = e2.take(play::pad(2));
            if (!started) { started = true; startExtend(); }       // P1 beginnt mit dem ersten Pfeil
            else if (ended()) {
                if (now - endAt > 800 && ((k1 | k2) & hw::K_C)) {
                    initMatch();
                    match++;
                    startExtend();
                    hw::beep(900, 60);
                }
            } else if (phase == PH_WATCH) tickWatch();
            else if (now >= inputBlockUntil) {
                uint8_t dir = firstDir(active == 1 ? k1 : k2);
                if (dir != NONE) handle(dir);
                if (ended()) endAt = now;
            }
        } else if (!play::live()) {
            lt = now;
            if (phase == PH_WATCH) phaseStart = now;               // Vorfuehrung neu beginnen
        }
        sendNow();
        draw();
        play::present();
    }
}

void guest() {
    initMatch();
    play::Edges e;
    e.sync(play::localPad());
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        // eigenes Druck-Echo sofort zeigen (der Host wertet dieselben Druecke aus)
        uint8_t dir = firstDir(e.take(play::localPad()));
        if (dir != NONE && iAmActive() && (phase == PH_REPLAY || phase == PH_EXTEND) && hw::ms() >= inputBlockUntil) {
            flash(dir);
            inputBlockUntil = hw::ms() + 250;
        }
        if (play::receive(b, &n) && n >= 6) {
            uint8_t np = b[1], prevWatch = watchFlash;
            seqLen = b[2]; progress = b[3]; active = b[4]; watchFlash = b[5];
            if (np == PH_WATCH && iAmActive() && watchFlash != prevWatch && watchFlash != NONE)
                hw::beep(TONE[watchFlash & 3], 180);
            if (np != phase) {
                if (np == PH_P1_WINS || np == PH_P2_WINS) {
                    hw::beep(120, 350);
                    flashLed((np == PH_P2_WINS) ? hw::LED_GREEN : hw::LED_RED, 400);
                }
                if (np == PH_EXTEND && phase == PH_REPLAY && iAmActive()) flashLed(hw::LED_GREEN, 80);
                else if (np == PH_DRAW) hw::beep(1500, 200);
                if (np == PH_EXTEND && iAmActive()) { extendSignal(); inputBlockUntil = hw::ms() + 600; }
                if (np == PH_REPLAY && iAmActive()) inputBlockUntil = hw::ms() + 500;
            }
            phase = np;
        }
        draw();
        play::present();
    }
}

}  // namespace wnr
