// Tennis (Pong) - hochkant. Jeder sieht seinen eigenen Schlaeger unten: der
// Host spiegelt das Feld senkrecht, der Gast zeichnet es unveraendert.
// Wer zuerst 7 Punkte hat, gewinnt.

#include <math.h>
#include "play.h"

namespace pong {
namespace {

constexpr int W = 64, H = 128;
constexpr int PW = 14, PH = 2;
constexpr int TY = 0, BY = H - 1;              // Torlinien
constexpr int P1Y = TY + 4, P2Y = BY - 4 - PH;  // P1 oben, P2 unten (Spiel-Koordinaten)
constexpr int WIN = 7;
constexpr float START = 1.6f, MAXV = 3.6f, DEFL = 1.3f;
constexpr uint32_t TICK_MS = 25;

float bX, bY, bVX, bVY;
int p1X, p2X;
uint8_t s1, s2, status, lastScorer, match;     // status 0 laeuft, 1/2 Sieger, 3 Punktpause
uint32_t pauseUntil;

void resetBall(int dir) {
    bX = W / 2.0f;
    bY = (TY + BY) / 2.0f;
    float a = hw::rndRange(-30, 30) * 0.0174533f;
    bVX = sinf(a) * START;
    bVY = (dir >= 0 ? 1.0f : -1.0f) * cosf(a) * START;
}

void initMatch() {
    s1 = s2 = 0;
    status = 0;
    p1X = p2X = (W - PW) / 2;
    resetBall(hw::rnd() & 1 ? 1 : -1);
}

void movePad(int& x, uint8_t held) {
    if (held & hw::K_LEFT) x -= 2;
    if (held & hw::K_RIGHT) x += 2;
    if (x < 1) x = 1;
    if (x > W - 1 - PW) x = W - 1 - PW;
}

void bounce(int padX) {
    float r = ((bX - padX) - PW / 2.0f) / (PW / 2.0f);
    bVX = r * DEFL;
    float spd = sqrtf(bVX * bVX + bVY * bVY);
    float ns = spd * 1.05f;
    if (ns > MAXV) ns = MAXV;
    bVX *= ns / spd;
    bVY *= ns / spd;
    hw::beep(1200, 20);
}

void tick(uint8_t h1, uint8_t h2) {
    movePad(p1X, h1);
    movePad(p2X, h2);
    if (status == 3) {
        if (hw::ms() >= pauseUntil) {
            if (s1 >= WIN) status = 1;
            else if (s2 >= WIN) status = 2;
            else { status = 0; resetBall(lastScorer == 1 ? 1 : -1); }
        }
        return;
    }
    if (status) return;
    for (int i = 0; i < 4; i++) {                    // 4 Teilschritte je Takt
        bX += bVX / 4; bY += bVY / 4;
        if (bX < 2) { bX = 2; bVX = -bVX; hw::beep(800, 15); }
        if (bX > W - 3) { bX = W - 3; bVX = -bVX; hw::beep(800, 15); }
        if (bVY < 0 && bY - 1 <= P1Y + PH && bY - 1 >= P1Y - 2 && bX >= p1X - 1 && bX <= p1X + PW) {
            bY = P1Y + PH + 2; bVY = -bVY; bounce(p1X);
        }
        if (bVY > 0 && bY + 1 >= P2Y && bY + 1 <= P2Y + PH + 2 && bX >= p2X - 1 && bX <= p2X + PW) {
            bY = P2Y - 2; bVY = -bVY; bounce(p2X);
        }
        if (bY < TY) { s2++; lastScorer = 2; status = 3; pauseUntil = hw::ms() + 800; hw::beep(150, 200); break; }
        if (bY > BY) { s1++; lastScorer = 1; status = 3; pauseUntil = hw::ms() + 800; hw::beep(150, 200); break; }
    }
}

void draw() {
    gfx::clear();
    bool mirror = play::host();                      // eigener Schlaeger immer unten
    auto Y = [&](int y) { return mirror ? TY + BY - y : y; };
    gfx::vline(0, 0, H, gfx::ON);
    gfx::vline(W - 1, 0, H, gfx::ON);
    for (int x = 2; x < W - 2; x += 6) gfx::hline(x, H / 2, 3, gfx::ON);
    // Punkte: eigene unter der Mittellinie, gegnerische darueber
    char b[4];
    int mine = mirror ? s1 : s2, other = mirror ? s2 : s1;
    b[0] = (char)('0' + other % 10); b[1] = 0;
    gfx::text(4, H / 2 - 10, b);
    b[0] = (char)('0' + mine % 10);
    gfx::text(4, H / 2 + 3, b);
    gfx::fillRect(p1X, mirror ? Y(P1Y) - PH + 1 : P1Y, PW, PH, gfx::ON);
    gfx::fillRect(p2X, mirror ? Y(P2Y) - PH + 1 : P2Y, PW, PH, gfx::ON);
    if (status != 3 || (hw::ms() / 100) & 1) gfx::fillRect((int)bX - 1, Y((int)bY) - 1, 3, 3, gfx::ON);
    if (status == 1 || status == 2) play::resultBox(status, s1, s2);
}

}  // namespace

void host() {
    initMatch();
    match = 0;
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t lt = hw::ms(), endAt = 0;
    while (play::running()) {
        uint32_t now = hw::ms();
        if (!play::live()) { lt = now; if (status == 3) pauseUntil = now + 800; }
        else {
            play::Pad p1 = play::pad(1), p2 = play::pad(2);
            uint8_t k = e1.take(p1) | e2.take(p2);
            for (int guard = 0; now - lt >= TICK_MS && guard < 4; guard++) {
                lt += TICK_MS;
                uint8_t before = status;
                tick(p1.held, p2.held);
                if ((before == 0 || before == 3) && (status == 1 || status == 2)) { endAt = now; hw::beep(120, 300); }
            }
            if (now - lt >= TICK_MS) lt = now;
            if ((status == 1 || status == 2) && now - endAt > 700 && (k & hw::K_C)) {
                initMatch();
                match++;
                hw::beep(900, 60);
            }
        }
        int by = (int)bY < 0 ? 0 : (int)bY > BY ? BY : (int)bY;
        uint8_t b[9] = { match, status, s1, s2, (uint8_t)p1X, (uint8_t)p2X,
                         (uint8_t)(int)bX, (uint8_t)by, lastScorer };
        play::send(b, sizeof b);
        draw();
        play::present();
    }
}

void guest() {
    initMatch();
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(b, &n) && n >= 9) {
            uint8_t prev = status;
            status = b[1]; s1 = b[2]; s2 = b[3];
            p1X = b[4]; p2X = b[5]; bX = b[6]; bY = b[7]; lastScorer = b[8];
            if (prev != 3 && status == 3) hw::beep(150, 200);
            if ((prev == 0 || prev == 3) && (status == 1 || status == 2)) hw::beep(120, 300);
        }
        draw();
        play::present();
    }
}

}  // namespace pong
