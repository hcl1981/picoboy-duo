// Vector Duel - quer. Zwei Raumschiffe mit Traegheit, Schild und Schuessen;
// die Raender sind verbunden. Wer zuerst 5 Treffer landet, gewinnt.
// P1 ist ein Umriss, P2 ein gefuelltes Dreieck.

#include <initializer_list>
#include <math.h>
#include <string.h>
#include "play.h"

namespace duel {
namespace {

constexpr int W = 128, H = 64, TY = 9;          // Spielfeld unter der Anzeigezeile
constexpr int WIN = 5, NSHOT = 8;
constexpr float TURN = 0.18f, THR = 0.055f, DRAG = 0.9925f, MAXV = 2.3f, SHV = 2.4f;
constexpr uint8_t SHOT_LIFE = 50;
constexpr uint32_t TICK_MS = 33, RESPAWN_MS = 1200, INVULN_MS = 1500, FIRE_GAP_MS = 150;
constexpr uint8_t SH_MAX = 90, SH_REC = 30;
constexpr float SH_R = 7.0f, SHIP_R = 4.0f;

struct Ship {
    float x, y, vx, vy, ang;
    bool alive, shield, locked;
    uint8_t charge, regen;
    uint32_t respawnAt, invulnUntil, nextShot;
};
struct Shot { float x, y, vx, vy; uint8_t life, owner; };

Ship p1, p2;
Shot shots[NSHOT];
uint8_t s1, s2, status, match;

void respawn(Ship& s, bool first) {
    s.x = first ? 30.0f : W - 30.0f;
    s.y = (TY + H) / 2.0f;
    s.vx = s.vy = 0;
    s.ang = first ? 0.0f : 3.14159f;
    s.alive = true;
    s.invulnUntil = hw::ms() + INVULN_MS;
    s.charge = SH_MAX;
    s.shield = s.locked = false;
    s.regen = 0;
}

void initMatch() {
    s1 = s2 = 0;
    status = 0;
    for (Shot& s : shots) s.life = 0;
    respawn(p1, true);
    respawn(p2, false);
}

void wrap(float& x, float& y) {
    if (x < 0) x += W;
    if (x >= W) x -= W;
    if (y < TY) y += H - TY;
    if (y >= H) y -= H - TY;
}

void fire(Ship& s, uint8_t owner) {
    for (Shot& sh : shots)
        if (!sh.life) {
            sh.x = s.x + cosf(s.ang) * 6;
            sh.y = s.y + sinf(s.ang) * 6;
            sh.vx = cosf(s.ang) * SHV + s.vx * 0.5f;
            sh.vy = sinf(s.ang) * SHV + s.vy * 0.5f;
            sh.life = SHOT_LIFE;
            sh.owner = owner;
            hw::beep(1500, 15);
            return;
        }
}

void control(Ship& s, uint8_t held, bool shoot, uint8_t owner) {
    if (!s.alive) return;
    if (held & hw::K_LEFT) s.ang -= TURN;
    if (held & hw::K_RIGHT) s.ang += TURN;
    if (s.ang < 0) s.ang += 6.2831853f;
    if (s.ang >= 6.2831853f) s.ang -= 6.2831853f;
    if (held & hw::K_UP) {
        s.vx += cosf(s.ang) * THR;
        s.vy += sinf(s.ang) * THR;
        float v = sqrtf(s.vx * s.vx + s.vy * s.vy);
        if (v > MAXV) { s.vx *= MAXV / v; s.vy *= MAXV / v; }
    }
    if (s.locked && s.charge >= SH_REC) s.locked = false;
    s.shield = (held & hw::K_DOWN) && !s.locked && s.charge > 0;
    if (shoot && !s.shield && hw::ms() >= s.nextShot) {
        fire(s, owner);
        s.nextShot = hw::ms() + FIRE_GAP_MS;
    }
}

void shieldTick(Ship& s) {
    if (!s.alive) return;
    if (s.shield) {
        s.charge = s.charge >= 2 ? s.charge - 2 : 0;
        if (!s.charge) { s.shield = false; s.locked = true; }
    } else if (++s.regen >= 4) {
        s.regen = 0;
        if (s.charge < SH_MAX) s.charge++;
    }
}

void move(Ship& s, bool first) {
    if (!s.alive) {
        if (hw::ms() >= s.respawnAt) respawn(s, first);
        return;
    }
    s.vx *= DRAG; s.vy *= DRAG;
    s.x += s.vx; s.y += s.vy;
    wrap(s.x, s.y);
}

// Zusammenstoss: auseinanderschieben und Impuls tauschen, kein Schaden.
void collide(Ship& a, Ship& b) {
    if (!a.alive || !b.alive) return;
    float dx = b.x - a.x, dy = b.y - a.y, d2 = dx * dx + dy * dy, md = 2 * SHIP_R;
    if (d2 >= md * md) return;
    float d = sqrtf(d2);
    if (d < 0.001f) { dx = 1; dy = 0; d = 1; }
    float nx = dx / d, ny = dy / d, ov = (md - d) * 0.5f + 0.1f;
    a.x -= nx * ov; a.y -= ny * ov;
    b.x += nx * ov; b.y += ny * ov;
    wrap(a.x, a.y); wrap(b.x, b.y);
    float rv = (a.vx - b.vx) * nx + (a.vy - b.vy) * ny;
    if (rv > 0) {
        float imp = rv * 0.95f;
        a.vx -= nx * imp; a.vy -= ny * imp;
        b.vx += nx * imp; b.vy += ny * imp;
        hw::beep(900, 30);
    }
}

void tick(uint8_t h1, bool f1, uint8_t h2, bool f2) {
    if (status) return;
    control(p1, h1, f1, 1);
    control(p2, h2, f2, 2);
    move(p1, true); move(p2, false);
    shieldTick(p1); shieldTick(p2);
    collide(p1, p2);
    uint32_t now = hw::ms();
    for (Shot& sh : shots) {
        if (!sh.life) continue;
        sh.x += sh.vx; sh.y += sh.vy;
        wrap(sh.x, sh.y);
        if (!--sh.life) continue;
        Ship& t = sh.owner == 1 ? p2 : p1;
        if (!t.alive || now < t.invulnUntil) continue;
        float dx = sh.x - t.x, dy = sh.y - t.y, r2 = dx * dx + dy * dy;
        if (t.shield && r2 < SH_R * SH_R) { sh.life = 0; hw::beep(1900, 25); }
        else if (r2 < SHIP_R * SHIP_R) {
            sh.life = 0;
            if (sh.owner == 1) s1++; else s2++;
            t.alive = false;
            t.respawnAt = now + RESPAWN_MS;
            hw::beep(120, 200);
            if (s1 >= WIN) status = 1;
            else if (s2 >= WIN) status = 2;
            if (status) hw::beep(120, 350);
        }
    }
}

// ---- Uebertragung ----
int pack(uint8_t* out) {
    uint8_t* p = out;
    uint32_t now = hw::ms();
    *p++ = match; *p++ = status; *p++ = s1; *p++ = s2;
    for (Ship* s : { &p1, &p2 }) {
        *p++ = (uint8_t)(int)s->x;
        *p++ = (uint8_t)(int)s->y;
        *p++ = (uint8_t)(int)(s->ang * 256.0f / 6.2831853f);
        *p++ = (uint8_t)((s->alive ? 1 : 0) | (now < s->invulnUntil ? 2 : 0) | (s->shield ? 4 : 0) | (s->locked ? 8 : 0));
        *p++ = s->charge;
    }
    for (Shot& sh : shots) {
        *p++ = (uint8_t)(int)sh.x;
        *p++ = (uint8_t)(int)sh.y;
        *p++ = (uint8_t)((sh.life > 63 ? 63 : sh.life) | (sh.owner == 2 ? 0x80 : 0));
    }
    return (int)(p - out);
}
constexpr int PKT = 4 + 2 * 5 + NSHOT * 3;

void unpack(const uint8_t* p) {
    uint8_t prev = status;
    p++;                                   // Match-Zaehler
    status = *p++; s1 = *p++; s2 = *p++;
    uint32_t now = hw::ms();
    for (Ship* s : { &p1, &p2 }) {
        s->x = *p++;
        s->y = *p++;
        s->ang = *p++ * 6.2831853f / 256.0f;
        uint8_t f = *p++;
        s->alive = f & 1;
        s->invulnUntil = (f & 2) ? now + 100 : 0;
        s->shield = f & 4;
        s->locked = f & 8;
        s->charge = *p++;
    }
    for (Shot& sh : shots) {
        sh.x = *p++;
        sh.y = *p++;
        sh.life = p[0] & 63;
        sh.owner = (p[0] & 0x80) ? 2 : 1;
        p++;
    }
    if (!prev && status) hw::beep(120, 350);
}

// ---- Zeichnen ----
void drawShip(const Ship& s, bool filled) {
    if (!s.alive) return;
    if (hw::ms() < s.invulnUntil && (hw::ms() / 80) & 1) return;
    float c = cosf(s.ang), n = sinf(s.ang);
    const float pts[3][2] = { { 6, 0 }, { -4, -3.5f }, { -4, 3.5f } };
    int x[3], y[3];
    for (int i = 0; i < 3; i++) {
        x[i] = (int)lroundf(s.x + pts[i][0] * c - pts[i][1] * n);
        y[i] = (int)lroundf(s.y + pts[i][0] * n + pts[i][1] * c);
    }
    if (filled) gfx::fillTriangle(x[0], y[0], x[1], y[1], x[2], y[2], gfx::ON);
    else gfx::triangle(x[0], y[0], x[1], y[1], x[2], y[2], gfx::ON);
    if (s.shield && (hw::ms() / 60) % 4) gfx::circle((int)s.x, (int)s.y, 7, gfx::ON);
}

void draw() {
    gfx::clear();
    char b[8];
    b[0] = 'P'; b[1] = '1'; b[2] = ' '; b[3] = (char)('0' + s1); b[4] = 0;
    gfx::text(0, 0, b);
    b[0] = (char)('0' + s2); b[1] = ' '; b[2] = 'P'; b[3] = '2'; b[4] = 0;
    gfx::text(W - 23, 0, b);
    // eigene Schildanzeige in der Mitte
    const Ship& me = play::host() ? p1 : p2;
    gfx::rect(34, 1, 60, 6, gfx::ON);
    int fw = me.charge * 58 / SH_MAX;
    if (me.locked) { for (int x = 0; x < fw; x += 2) gfx::vline(35 + x, 2, 4, gfx::ON); }
    else gfx::fillRect(35, 2, fw, 4, gfx::ON);
    gfx::hline(0, TY - 1, W, gfx::ON);
    drawShip(p1, false);
    drawShip(p2, true);
    for (const Shot& sh : shots)
        if (sh.life) gfx::fillRect((int)sh.x, (int)sh.y, 2, 2, gfx::ON);
    if (status) play::resultBox(status, s1, s2);
}

}  // namespace

void host() {
    initMatch();
    match = 0;
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t lt = hw::ms(), endAt = 0;
    bool f1 = false, f2 = false;
    while (play::running()) {
        uint32_t now = hw::ms();
        if (!play::live()) lt = now;
        else {
            play::Pad a = play::pad(1), b = play::pad(2);
            uint8_t k1 = e1.take(a), k2 = e2.take(b);
            f1 |= (k1 & hw::K_C) != 0;          // Schuss bis zum naechsten Takt merken
            f2 |= (k2 & hw::K_C) != 0;
            for (int guard = 0; now - lt >= TICK_MS && guard < 3; guard++) {
                lt += TICK_MS;
                uint8_t before = status;
                tick(a.held, f1, b.held, f2);
                f1 = f2 = false;
                if (!before && status) endAt = now;
            }
            if (now - lt >= TICK_MS) lt = now;
            if (status && now - endAt > 800 && ((k1 | k2) & hw::K_C)) {
                initMatch();
                match++;
                f1 = f2 = false;
                hw::beep(900, 60);
            }
        }
        uint8_t buf[PKT];
        play::send(buf, pack(buf));
        draw();
        play::present();
    }
}

void guest() {
    initMatch();
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(b, &n) && n >= PKT) unpack(b);
        draw();
        play::present();
    }
}

}  // namespace duel
