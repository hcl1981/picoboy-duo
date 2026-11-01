#include "play.h"

#include <stdio.h>
#include <string.h>

namespace play {


namespace {

constexpr uint8_t MAGIC_H = 0xD7;          // Host -> Gast (Protokollversion 1)
constexpr uint8_t MAGIC_G = 0x7D;          // Gast -> Host
constexpr uint32_t FRAME_MS = 16;

bool hostRole;
uint8_t curGame;                           // 1..NUM_GAMES, 0 = keins
uint8_t curSession;
uint8_t hostSessionCounter;
uint32_t startMs;

// Gast
uint32_t lastRxSeen;

struct GuestPkt {
    bool valid;
    Pad pad;
    uint8_t game, session;
};

GuestPkt readGuest() {
    GuestPkt g{};
    uint8_t b[usblink::S2M_MAX];
    int n = usblink::getRx(b, sizeof b);
    if (n < 10 || b[0] != MAGIC_G) return g;
    g.valid = true;
    g.pad.held = b[1];
    memcpy(g.pad.cnt, b + 2, 5);
    g.game = b[8];
    g.session = b[9];
    return g;
}

void report(uint8_t game, uint8_t session) {
    Pad p = localPad();
    uint8_t b[10] = { MAGIC_G, p.held, p.cnt[0], p.cnt[1], p.cnt[2], p.cnt[3], p.cnt[4], 0, game, session };   // Byte 7 frei
    usblink::setTx(b, sizeof b);
}

void header(uint8_t game) {
    uint8_t b[3] = { MAGIC_H, game, curSession };
    usblink::setTx(b, sizeof b);
}

bool guestInGame() {
    GuestPkt g = readGuest();
    return g.valid && g.game == curGame && g.session == curSession;
}

// Tastendruecke des Gasts, die er ausserhalb des Spiels macht, schluckt der
// Host - vor allem den Druck auf Mitte, mit dem der Gast seinen Startbildschirm
// verlaesst: der kommt oft im selben Paket an wie die Meldung "bin im Spiel"
// und waere sonst die erste Aktion im Spiel (Bombe, Schuss, Einwurf ...).
uint8_t guestAdj[5], guestPrev[5];
bool guestTracked, guestWasIn;

void trackGuest() {
    GuestPkt g = readGuest();
    if (!g.valid) return;
    bool in = g.game == curGame && g.session == curSession;
    if (!guestTracked) { memcpy(guestPrev, g.pad.cnt, 5); guestTracked = true; }
    if (!in || !guestWasIn)
        for (int k = 0; k < 5; k++) guestAdj[k] += (uint8_t)(g.pad.cnt[k] - guestPrev[k]);
    memcpy(guestPrev, g.pad.cnt, 5);
    guestWasIn = in;
}

}  // namespace

void displayName(const Game& g, char* out, int cap) {
    int n = 0;
    for (const char* s = g.name; *s && n < cap - 1; s++)
        if (*s != '|') out[n++] = *s;
    out[n] = 0;
}

namespace {

enum SplashResult { SPLASH_START, SPLASH_CANCEL, SPLASH_CONFLICT };

// Startbildschirm in der Lage des Spiels: nur der Name, ob noch auf den
// Mitspieler gewartet wird und wie es losgeht. Host: Links bricht ab; merkt
// er, dass die Gegenseite ebenfalls Host ist, gibt er SPLASH_CONFLICT zurueck.
// Gast: endet auch, wenn der Host das Spiel verlaesst.
SplashResult splash(const Game& g) {
    gfx::setOrient(g.orient);
    Edges e;
    e.sync(localPad());
    uint32_t conflicts = usblink::stats().conflict;
    for (;;) {
        uint8_t k = e.take(localPad());
        bool partner = true;
        if (hostRole) {
            partner = guestPresent();
            if (k & hw::K_LEFT) return SPLASH_CANCEL;
            if (!partner && usblink::stats().conflict - conflicts >= 3) return SPLASH_CONFLICT;
        } else {
            uint8_t game, sess;
            if (hostOffer(&game, &sess) && (game != curGame || sess != curSession)) return SPLASH_CANCEL;
            report(0, 0);
        }
        if (partner && (k & hw::K_C)) return SPLASH_START;

        gfx::clear();
        int W = gfx::W(), H = gfx::H();
        char a[24], b[24];
        const char* cut = strchr(g.name, '|');
        b[0] = 0;
        if (cut && W < 100) {                        // hochkant: an '|' umbrechen
            snprintf(a, sizeof a, "%.*s", (int)(cut - g.name), g.name);
            snprintf(b, sizeof b, "%s", cut + 1);
        } else displayName(g, a, sizeof a);
        int sc = gfx::textWidth(a, 2) <= W && gfx::textWidth(b, 2) <= W ? 2 : 1;
        int y = H / 2 - (b[0] ? 24 : 16) * sc / 2 - 8;
        gfx::textCenter(W / 2, y, a, gfx::ON, sc);
        if (b[0]) gfx::textCenter(W / 2, y + 9 * sc, b, gfx::ON, sc);
        bool blink = (hw::ms() / 250) & 1;
        if (!partner) {
            gfx::textCenter(W / 2, H - 22, "WAITING FOR");
            gfx::textCenter(W / 2, H - 13, blink ? "PARTNER..." : "PARTNER");
        } else if (blink) gfx::textCenter(W / 2, H - 13, "P U S H !");
        gfx::flush();
        hw::sleepMs(20);
    }
}

}  // namespace

// ---- Edges --------------------------------------------------------------
void Edges::sync(const Pad& p) { memcpy(last, p.cnt, 5); }

uint8_t Edges::take(const Pad& p) {
    uint8_t m = 0;
    for (int k = 0; k < 5; k++)
        if (p.cnt[k] != last[k]) { m |= 1u << k; last[k] = p.cnt[k]; }
    return m;
}

// ---- Verbindung ---------------------------------------------------------
void beginHost() {
    hostRole = true;
    curGame = 0;
    usblink::start(true);
    hostIdle();
}

void beginGuest() {
    hostRole = false;
    curGame = 0;
    usblink::start(false);
    guestIdle();
}

void end() {
    usblink::stop();
    hw::led(hw::LED_RED, false);
}

bool isHost() { return hostRole; }

bool linkUp() { return usblink::msSinceOk() < 300; }

void hostIdle() {
    curGame = 0;
    header(0);
}

bool guestPresent() { return usblink::msSinceOk() < 1000 && readGuest().valid; }

bool hostOffer(uint8_t* game, uint8_t* session) {
    if (usblink::msSinceOk() > 1000) return false;
    uint8_t b[3];
    if (usblink::getRx(b, 3) < 3 || b[0] != MAGIC_H) return false;
    *game = b[1];
    *session = b[2];
    return true;
}

void guestIdle() { report(0, 0); }

// ---- Spielrahmen --------------------------------------------------------
bool runHost(int idx) {
    curGame = (uint8_t)(idx + 1);
    curSession = (uint8_t)(++hostSessionCounter + (hw::rnd() & 0xF0));
    header(curGame);                    // Gast sieht das Angebot schon waehrend des Startbildschirms
    guestTracked = guestWasIn = false;
    memset(guestAdj, 0, sizeof guestAdj);
    SplashResult r = splash(GAMES[idx]);
    if (r == SPLASH_START) {
        startMs = hw::ms();
        GAMES[idx].host();
    }
    // Einen Moment lang "Spiel 0" senden, damit der Gast sicher mitbekommt,
    // dass es vorbei ist (danach lauscht dieses Geraet im Menue selbst als Gast).
    hostIdle();
    hw::sleepMs(300);
    gfx::setOrient(gfx::LAND);
    hw::led(hw::LED_RED, false);
    return r == SPLASH_CONFLICT;
}

void runGuest(int idx, uint8_t session) {
    curGame = (uint8_t)(idx + 1);
    curSession = session;
    if (splash(GAMES[idx]) == SPLASH_START) {
        lastRxSeen = usblink::rxCount() - 1;   // ersten Zustand sofort uebernehmen
        startMs = hw::ms();
        report(curGame, curSession);
        GAMES[idx].guest();
    }
    curGame = 0;
    guestIdle();
    gfx::setOrient(gfx::LAND);
    hw::led(hw::LED_RED, false);
}

bool host() { return hostRole; }

uint32_t sinceStart() { return hw::ms() - startMs; }

// Langes Halten von Mitte hat bewusst keine Sonderfunktion - ein Spiel laeuft,
// bis man das Geraet ausschaltet (wie bei PicoPac). Der Gast verlaesst ein Spiel
// nur, wenn der Host ein anderes anbietet.
bool running() {
    hw::led(hw::LED_RED, !linkUp() && sinceStart() > 1500);
    if (hostRole) return true;
    uint8_t game, sess;
    if (hostOffer(&game, &sess) && (game != curGame || sess != curSession)) return false;
    report(curGame, curSession);
    return true;
}

bool live() {
    if (!linkUp()) return false;
    if (!hostRole) return true;
    trackGuest();
    return guestInGame();
}

void present() {
    uint32_t down = usblink::msSinceOk();
    if (down > 800 && sinceStart() > 800) {
        gfx::messageBox("LINK LOST", "CHECK CABLE");
    } else if (hostRole && !guestInGame() && sinceStart() > 300) {
        gfx::messageBox("WAITING FOR", "PARTNER");
    }
    gfx::flush();
    // Bildtakt ~60 Hz; schlafen statt kreiseln schont die Knopfzelle
    static uint32_t lastFrame;
    uint32_t d = hw::ms() - lastFrame;
    if (d < FRAME_MS) hw::sleepMs(FRAME_MS - d);
    lastFrame = hw::ms();
}

Pad localPad() {
    Pad p;
    p.held = hw::held();
    hw::counts(p.cnt);
    return p;
}

Pad pad(int player) {
    if (player == 1) return localPad();
    trackGuest();
    Pad p = readGuest().pad;
    for (int k = 0; k < 5; k++) p.cnt[k] = (uint8_t)(p.cnt[k] - guestAdj[k]);
    return p;
}

void send(const uint8_t* p, int n) {
    uint8_t b[usblink::M2S_MAX];
    if (n > PAYLOAD_MAX) n = PAYLOAD_MAX;
    b[0] = MAGIC_H;
    b[1] = curGame;
    b[2] = curSession;
    memcpy(b + 3, p, n);
    usblink::setTx(b, 3 + n);
}

bool receive(uint8_t* p, int* n) {
    uint32_t c = usblink::rxCount();
    if (c == lastRxSeen) return false;
    lastRxSeen = c;
    uint8_t b[usblink::M2S_MAX];
    int len = usblink::getRx(b, sizeof b);
    if (len < 4 || b[0] != MAGIC_H || b[1] != curGame || b[2] != curSession) return false;
    *n = len - 3;
    memcpy(p, b + 3, *n);
    return true;
}

void resultBox(int winner, int s1, int s2, const char* extra) {
    int me = hostRole ? 1 : 2;
    const char* title = winner == 3 ? "DRAW" : winner == me ? "YOU WIN!" : "YOU LOSE";
    char score[16];
    int mine = hostRole ? s1 : s2, other = hostRole ? s2 : s1;
    snprintf(score, sizeof score, "%d : %d", mine, other);
    gfx::messageBox(title, score, extra ? extra : "P U S H !", gfx::W() >= 100);
}

}  // namespace play
