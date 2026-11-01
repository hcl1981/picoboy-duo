// PC-Simulator fuer PicoBoy Duo: laesst Host und Gast als zwei Prozesse laufen
// (fork + socketpair statt Kabel) und speichert regelmaessig beide Bildschirme.
//
// Benutzt die ECHTEN Quellen von gfx.cpp, play.cpp und den Spielen. Ersetzt
// werden nur hw.cpp (Tasten kommen aus einem Zufallsskript), link.cpp (Pakete
// laufen ueber einen Socket) und das Display: die SPI-Bytes landen in einem
// nachgebildeten SH1106, aus dessen RAM das Bild entsteht.
//
//   ./build.sh && ./pbsim <spiel 0..8> <sekunden> <ausgabeordner>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hardware/spi.h"
#include "gfx.h"
#include "hw.h"
#include "link.h"
#include "play.h"

static const char* role = "host";
static const char* outDir = ".";
static uint32_t deadline = 10000, dumpEvery = 500, nextDump = 0;
static uint32_t seedBase = 1;

// ---- nachgebildetes SH1106 ------------------------------------------------
static int dc, page, col, argSkip;
static uint8_t ram[8][132];

void gpio_init(uint) {}
void gpio_set_dir(uint, int) {}
void gpio_set_function(uint, int) {}
void gpio_pull_up(uint) {}
void gpio_put(uint pin, int v) { if (pin == 8) dc = v; }
void sleep_ms(uint32_t ms) { usleep(ms * 1000); }
void busy_wait_us_32(uint32_t us) { usleep(us); }

// Flash-Attrappe fuer das EEPROM
#include "hardware/flash.h"
uint8_t sim_flash[PICO_FLASH_SIZE_BYTES];
void flash_range_erase(uint32_t off, size_t n) { memset(sim_flash + off, 0xFF, n); }
void flash_range_program(uint32_t off, const uint8_t* d, size_t n) { memcpy(sim_flash + off, d, n); }
uint spi_init(spi_inst_t*, uint b) { return b; }
void spi_set_format(spi_inst_t*, uint, int, int, int) {}

// Nachgebildeter Beschleunigungssensor STK8BA58 (Adresse 0x18): die Schwerkraft
// kreist langsam, damit sich der Wuerfel im HWTest dreht. Ohne Sensor
// (PBSIM_NOSENSOR=1) meldet er sich nicht, wie ein defektes Geraet.
#include <math.h>
#include "hardware/i2c.h"
static int sensorReg;
static bool sensorFound(uint8_t addr) { return addr == 0x18 && !getenv("PBSIM_NOSENSOR"); }
int i2c_write_timeout_us(i2c_inst_t*, uint8_t addr, const uint8_t* d, size_t n, bool, uint) {
    if (!sensorFound(addr)) return PICO_ERROR_GENERIC;
    if (n) sensorReg = d[0];
    return (int)n;
}
int i2c_read_timeout_us(i2c_inst_t*, uint8_t addr, uint8_t* d, size_t n, bool, uint) {
    if (!sensorFound(addr)) return PICO_ERROR_GENERIC;
    double t = time_us_64() / 1e6;
    double g[3] = { 0.7 * sin(t), 0.7 * cos(t * 0.7), -0.7 };
    int axis = (sensorReg - 2) / 2;
    int raw = (int)(g[axis >= 0 && axis < 3 ? axis : 0] * 1024);   // 12 Bit, 1024 = 1 g
    uint16_t v = (uint16_t)(raw * 16);
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)(v >> (i ? 8 : 0));
    return (int)n;
}

static void dump();

int spi_write_blocking(spi_inst_t*, const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint8_t b = d[i];
        if (dc) {
            if (col < 132) ram[page][col] = b;
            col++;
            if (page == 7 && col == 130) dump();          // letzte Seite fertig = Bild fertig
            continue;
        }
        if (argSkip) { argSkip--; continue; }
        if (b >= 0xB0 && b <= 0xB7) page = b & 7;
        else if (b <= 0x0F) col = (col & 0xF0) | b;
        else if (b >= 0x10 && b <= 0x1F) col = (col & 0x0F) | ((b & 0x0F) << 4);
        else if (b == 0x81 || b == 0xD5 || b == 0xA8 || b == 0xD3 || b == 0x8D || b == 0x20 ||
                 b == 0xDA || b == 0xD9 || b == 0xDB || b == 0xAD) argSkip = 1;
    }
    return (int)n;
}

// Bild so speichern, wie man das Geraet haelt (hochkant gedreht).
static void dump() {
    uint32_t t = hw::ms();
    if (t >= deadline) _exit(0);
    if (t < nextDump) return;
    nextDump = t + dumpEvery;
    bool port = gfx::orient() == gfx::PORT && !getenv("PBSIM_RAW");   // RAW: Displayspeicher wie er ist (quer)
    int w = port ? 64 : 128, h = port ? 128 : 64;
    char name[256];
    snprintf(name, sizeof name, "%s/%s_%06u.pgm", outDir, role, t);
    FILE* f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P5 %d %d 255\n", w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = port ? y : x, r = port ? 63 - x : y;          // logisch -> RAM (siehe gfx::pixel)
            bool on = ram[r >> 3][c + 2] & (1u << (r & 7));
            fputc(on ? 255 : 0, f);
        }
    fclose(f);
}

// ---- hw ------------------------------------------------------------------
namespace hw {
static uint32_t rng = 0x9E3779B9u;
static timespec t0;
static uint8_t curHeld, cnt[5];
static uint32_t since[5], nextChange;
static uint32_t scriptRng;

uint32_t ms() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000);
}

// Feste Tastenfolge aus PBSIM_KEYS_HOST / PBSIM_KEYS_GUEST: ab 5 s alle 300 ms
// ein Zeichen (U D L R C = kurz druecken, . = nichts); '*' am Ende wiederholt.
static const char* keys;
static void keyScript() {
    uint32_t t = ms();
    if (t < 5000) return;
    int step = (int)((t - 5000) / 300), phase = (int)((t - 5000) % 300);
    int len = (int)strlen(keys);
    bool loop = len && keys[len - 1] == '*';
    if (loop) len--;
    if (!len || (!loop && step >= len)) { curHeld = 0; return; }
    char c = keys[step % len];
    uint8_t n = 0;
    if (phase < 120)
        switch (c) { case 'U': n = K_UP; break; case 'D': n = K_DOWN; break;
                     case 'L': n = K_LEFT; break; case 'R': n = K_RIGHT; break; case 'C': n = K_C; break; }
    for (int k = 0; k < 5; k++)
        if ((n & (1u << k)) && !(curHeld & (1u << k))) { cnt[k]++; since[k] = t; }
    curHeld = n;
}

// Zufallsspieler: alle 150-500 ms eine neue Tastenlage, Mitte nur kurz.
static void script() {
    if (keys) { keyScript(); return; }
    uint32_t t = ms();
    if (t < nextChange) return;
    if (t < 6000) {                       // Startbildschirm: nur Mitte, kein Abbruch mit Links
        uint8_t n = (t / 400) & 1 ? K_C : 0;
        if (n && !curHeld) { cnt[4]++; since[4] = t; }
        curHeld = n;
        nextChange = t + 60;
        return;
    }
    scriptRng = scriptRng * 1664525u + 1013904223u;
    uint32_t r = scriptRng >> 8;
    nextChange = t + 150 + r % 350;
    uint8_t n = 0;
    switch (r % 9) {
        case 0: case 1: n = 0; break;
        case 2: n = K_UP; break;
        case 3: n = K_DOWN; break;
        case 4: n = K_LEFT; break;
        case 5: n = K_RIGHT; break;
        case 6: case 7: n = K_C; nextChange = t + 60; break;
        default: n = (r & 16) ? K_UP | K_LEFT : K_DOWN | K_RIGHT; break;
    }
    for (int k = 0; k < 5; k++)
        if ((n & (1u << k)) && !(curHeld & (1u << k))) { cnt[k]++; since[k] = t; }
    curHeld = n;
}

void init() { clock_gettime(CLOCK_MONOTONIC, &t0); }
uint8_t held() { script(); return curHeld; }
void counts(uint8_t out[5]) { script(); memcpy(out, cnt, 5); }
uint32_t heldMs(Key k) { script(); return (curHeld & (1u << k)) ? ms() - since[k] + 1 : 0; }
void beep(uint16_t, uint16_t) {}
void setSound(bool) {}
bool soundOn() { return false; }
void led(Led, bool) {}
void ledLevel(Led, uint8_t) {}
uint32_t rnd() { uint32_t x = rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return rng = x; }
int rndRange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
void seed(uint32_t s) { rng ^= s * 2654435761u; if (!rng) rng = 1; }
void sleepMs(uint32_t t) { usleep(t * 1000); }
}  // namespace hw

// Rohe Pins fuer die PicoPac-Spiele (digitalRead): Tasten des Skripts in
// Querlage-Belegung, aktiv low
int gpio_get(uint pin) {
    static const uint8_t pinOf[5] = { 1, 3, 4, 2, 0 };   // UP DOWN LEFT RIGHT CENTER
    uint8_t h = hw::held();
    for (int k = 0; k < 5; k++) if (pinOf[k] == pin && (h & (1u << k))) return 0;
    return 1;
}
uint64_t time_us_64(void) { return (uint64_t)hw::ms() * 1000; }
absolute_time_t get_absolute_time(void) { return time_us_64(); }
uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }

// ---- link ueber socketpair -------------------------------------------------
namespace usblink {
static int sock = -1;
static bool hostRole;
static uint8_t tx[M2S_MAX], rx[M2S_MAX];
static int txLen, rxLen;
static uint32_t rxCnt, lastOk, lastSend;
static Stats st;

// Wie der echte Link: nur der Host stoesst alle 15 ms einen Austausch an, der
// Gast antwortet darauf. Zwei Hosts sehen sich gegenseitig als Konflikt, zwei
// Gaeste hoeren nichts. Jedes Paket beginnt mit der Rolle des Absenders.
static bool active;
static void deliver(const uint8_t* b, int n) {
    int cap = hostRole ? S2M_MAX : M2S_MAX;
    rxLen = n > cap ? cap : n;
    memcpy(rx, b, rxLen);
    rxCnt++;
    lastOk = hw::ms();
    st.ok++;
}
static void sendRole() {
    uint8_t b[1 + M2S_MAX];
    b[0] = hostRole ? 1 : 0;
    memcpy(b + 1, tx, txLen);
    send(sock, b, 1 + txLen, MSG_DONTWAIT);
}
static void pump() {
    if (!active) return;
    uint32_t now = hw::ms();
    if (hostRole && now - lastSend >= 15) { lastSend = now; sendRole(); }
    uint8_t b[256];
    for (;;) {
        ssize_t n = recv(sock, b, sizeof b, MSG_DONTWAIT);
        if (n <= 0) break;
        bool fromHost = b[0] == 1;
        if (hostRole) {
            if (fromHost) st.conflict++;
            else deliver(b + 1, (int)n - 1);
        } else if (fromHost) {
            deliver(b + 1, (int)n - 1);
            sendRole();
        }
    }
}

void start(bool host) { hostRole = host; active = true; txLen = rxLen = 0; lastOk = hw::ms() - 100000; }
void stop() { active = false; }
bool running() { return active; }
bool isHost() { return hostRole; }
void setTx(const uint8_t* d, int n) {
    int cap = hostRole ? M2S_MAX : S2M_MAX;
    txLen = n > cap ? cap : n;
    memcpy(tx, d, txLen);
    pump();
}
int getRx(uint8_t* d, int cap) { pump(); int n = rxLen < cap ? rxLen : cap; memcpy(d, rx, n); return n; }
uint32_t rxCount() { pump(); return rxCnt; }
uint32_t msSinceOk() { pump(); return hw::ms() - lastOk; }
void setHalfBitNs(uint32_t) {}
uint32_t halfBitNs() { return 0; }
Stats stats() { return st; }
void resetStats() {}
}  // namespace usblink

int pb_main();   // main.cpp der Firmware (mit -Dmain=pb_main uebersetzt)

int main(int argc, char** argv) {
    int game = argc > 1 ? atoi(argv[1]) : 0;
    if (game == -2) {                     // komplette Firmware auf beiden Seiten
        deadline = (argc > 2 ? atoi(argv[2]) : 10) * 1000;
        outDir = argc > 3 ? argv[3] : ".";
        if (argc > 4) dumpEvery = atoi(argv[4]);
        if (argc > 5) seedBase = atoi(argv[5]);
        int sv[2];
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
        pid_t pid = fork();
        bool a = pid != 0;
        usblink::sock = a ? sv[0] : sv[1];
        role = a ? "a" : "b";
        hw::init();
        hw::seed(seedBase * (a ? 11u : 23u));
        hw::keys = getenv(a ? "PBSIM_KEYS_HOST" : "PBSIM_KEYS_GUEST");
        if (hw::keys && !*hw::keys) hw::keys = nullptr;
        return pb_main();
    }
    if (game < 0) {                       // Menue der Firmware, ohne Gegenstelle
        deadline = (argc > 2 ? atoi(argv[2]) : 10) * 1000;
        outDir = argc > 3 ? argv[3] : ".";
        if (argc > 4) dumpEvery = atoi(argv[4]);
        int sv[2];
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
        usblink::sock = sv[0];
        hw::init();
        hw::keys = getenv("PBSIM_KEYS_HOST");
        return pb_main();
    }
    deadline = (argc > 2 ? atoi(argv[2]) : 10) * 1000;
    outDir = argc > 3 ? argv[3] : ".";
    if (argc > 4) dumpEvery = atoi(argv[4]);
    if (argc > 5) seedBase = atoi(argv[5]);
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv)) { perror("socketpair"); return 1; }
    pid_t pid = fork();
    bool host = pid != 0;
    usblink::sock = host ? sv[0] : sv[1];
    role = host ? "host" : "guest";
    hw::init();
    hw::scriptRng = seedBase * (host ? 12345u : 67891u);
    hw::keys = getenv(host ? "PBSIM_KEYS_HOST" : "PBSIM_KEYS_GUEST");
    if (hw::keys && !*hw::keys) hw::keys = nullptr;          // leer = Zufallsspieler
    hw::seed(seedBase * 777u);
    gfx::init();
    if (host) {
        play::beginHost();
        play::runHost(game);
        printf("host: Spiel verlassen\n");
        int status;
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        return 0;
    }
    play::beginGuest();
    for (;;) {
        play::guestIdle();
        uint8_t g, s;
        if (play::hostOffer(&g, &s) && g >= 1 && g <= play::NUM_GAMES) play::runGuest(g - 1, s);
        hw::sleepMs(10);
        if (hw::ms() >= deadline) _exit(0);
    }
}
