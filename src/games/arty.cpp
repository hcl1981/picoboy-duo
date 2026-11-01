// Pocket Tanks (Artillerie) - quer. Abwechselnd Winkel und Kraft einstellen,
// Waffe waehlen, schiessen; Wind lenkt ab. Wer zuerst 3 Runden gewinnt.
//
// Gelaende: eine Hoehenkarte (128 Spalten). Das Original hatte ein Pixelgitter
// mit Schwerkraft - nach dem Setzen ist das aber ohnehin wieder eine
// Hoehenkarte, also wird ein Krater gleich als "was darueber liegt, rutscht
// nach" abgezogen. Die untersten Zeilen sind Grundgestein.
//
// Waffen: Basis (unbegrenzt), Inferno (2x, doppelter Radius), Schock (2x,
// doppelter Schaden), Kuppel (1x, Schutzschild statt Schuss).
//
// Uebertragung: Zustand plus reihum ein Viertel der Hoehenkarte je Paket.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "play.h"

namespace arty {
namespace {

constexpr int W = 128, H = 64, BEDROCK = 3;
constexpr int MAX_HP = 50, WINS = 3;
constexpr int BLAST = 7, SHIELD_R = 11, SHIELD_SEG = 24;
constexpr int EXP_GROW = 10, EXP_PLAT = 2, EXP_SHR = 10, EXP_TOTAL = EXP_GROW + EXP_PLAT + EXP_SHR;
constexpr float GRAV = 0.064f, WIND_ACC = 0.0011f, SPEED = 0.042f;
constexpr uint32_t TICK_MS = 33;
constexpr int CHUNK = 32;

enum { WP_BASIC, WP_INFERNO, WP_SHOCK, WP_DOME };
enum { PH_AIM = 0, PH_FLY = 1, PH_EXPLODE = 2, PH_MATCH = 4, PH_ROUNDWAIT = 5 };

uint8_t hgt[W];
uint8_t terrVer;
int tx[3], ty[3];                 // Index 1/2
uint8_t hp[3], wins[3];
uint8_t active, phase, match;
int8_t wind;
int angle, power, angleP[3], powerP[3];
uint8_t selWeapon[3], ammo[3][4];
uint32_t shieldMask[3];
int shieldX[3], shieldY[3];
float prjX, prjY, prjVX, prjVY;
uint8_t prjOwner, prjWeapon;
int expX, expY;
uint8_t expFrame, expWeapon, curWeapon, curAmmo, selMode;
uint32_t aimReadyAt, roundWaitUntil, lastRepeat;

int holeR(uint8_t w) { return w == WP_INFERNO ? 18 : 9; }
int blastR(uint8_t w) { return w == WP_INFERNO ? 2 * BLAST : BLAST; }
int damage(uint8_t w) { return w == WP_SHOCK ? 60 : 30; }
int groundTop(int x) { return H - hgt[x < 0 ? 0 : x >= W ? W - 1 : x]; }

void genTerrain() {
    float a = hw::rnd() % 1000 / 100.0f, b = hw::rnd() % 1000 / 100.0f;
    float c = hw::rnd() % 1000 / 100.0f, d = hw::rnd() % 1000 / 100.0f;
    for (int x = 0; x < W; x++) {
        float t = x * 0.085f;
        int h = 22 + (int)(7 * sinf(t + a) + 5 * sinf(2 * t + b) + 3 * sinf(0.6f * t + c) + 2 * sinf(4 * t + d));
        hgt[x] = (uint8_t)(h < 6 ? 6 : h > 40 ? 40 : h);
    }
    // zu steile Kanten abtragen
    for (int pass = 0; pass < 300; pass++) {
        bool moved = false;
        for (int x = 0; x < W - 1; x++) {
            int diff = hgt[x] - hgt[x + 1];
            if (diff > 2) { hgt[x]--; hgt[x + 1]++; moved = true; }
            else if (diff < -2) { hgt[x]++; hgt[x + 1]--; moved = true; }
        }
        if (!moved) break;
    }
}

void placeTanks() {
    tx[1] = 12; tx[2] = W - 13;
    for (int p = 1; p <= 2; p++) {
        int top = 0;
        for (int x = tx[p] - 5; x <= tx[p] + 5; x++) if (hgt[x] > top) top = hgt[x];
        for (int x = tx[p] - 4; x <= tx[p] + 4; x++) hgt[x] = (uint8_t)top;
        ty[p] = groundTop(tx[p]);
    }
}

void newTerrain() {
    genTerrain();
    placeTanks();
    terrVer++;
}

void loadAim() { angle = angleP[active]; power = powerP[active]; }
void saveAim() { angleP[active] = angle; powerP[active] = power; }

void initRound() {
    hp[1] = hp[2] = MAX_HP;
    active = (wins[1] + wins[2]) % 2 == 0 ? 1 : 2;
    phase = PH_AIM;
    selMode = 0;
    loadAim();
    wind = (int8_t)hw::rndRange(-8, 8);
    newTerrain();
    shieldMask[1] = shieldMask[2] = 0;
    aimReadyAt = hw::ms() + 800;
}

void initMatch() {
    wins[1] = wins[2] = 0;
    for (int p = 1; p <= 2; p++) {
        angleP[p] = 45; powerP[p] = 50;
        ammo[p][WP_BASIC] = 0; ammo[p][WP_INFERNO] = 2; ammo[p][WP_SHOCK] = 2; ammo[p][WP_DOME] = 1;
        selWeapon[p] = WP_BASIC;
    }
    initRound();
    expFrame = 0;
}

bool hasWeapon(uint8_t p, uint8_t w) { return w == WP_BASIC || ammo[p][w] > 0; }

void cycleWeapon(uint8_t p, int dir) {
    for (int i = 0; i < 4; i++) {
        selWeapon[p] = (uint8_t)((selWeapon[p] + 4 + dir) % 4);
        if (hasWeapon(p, selWeapon[p])) return;
    }
}

void applyDamage(int cx, int cy, int r, int dmg) {
    for (int p = 1; p <= 2; p++) {
        float d = sqrtf((float)((cx - tx[p]) * (cx - tx[p]) + (cy - ty[p]) * (cy - ty[p])));
        if (d < r) {
            int hit = (int)(dmg * (1.0f - d / r));
            hp[p] = hit > hp[p] ? 0 : (uint8_t)(hp[p] - hit);
        }
    }
}

void switchPlayer() {
    saveAim();
    selMode = 0;
    if (!hp[1] || !hp[2]) {
        if (!hp[1] && hp[2]) wins[2]++;
        else if (!hp[2] && hp[1]) wins[1]++;
        if (wins[1] >= WINS || wins[2] >= WINS) { phase = PH_MATCH; hw::beep(120, 350); return; }
        initRound();
        phase = PH_ROUNDWAIT;
        roundWaitUntil = hw::ms() + 1200;
        return;
    }
    active = active == 1 ? 2 : 1;
    phase = PH_AIM;
    loadAim();
    wind = (int8_t)hw::rndRange(-8, 8);
}

void doFire() {
    uint8_t p = active, w = selWeapon[p];
    if (!hasWeapon(p, w)) w = selWeapon[p] = WP_BASIC;
    selMode = 0;
    if (w == WP_DOME) {
        ammo[p][WP_DOME]--;
        shieldMask[p] = (1u << SHIELD_SEG) - 1;
        shieldX[p] = tx[p];
        shieldY[p] = ty[p] - 3;
        cycleWeapon(p, 1);
        hw::beep(1200, 80);
        switchPlayer();                   // kein Schuss -> Zug endet
        return;
    }
    if (w != WP_BASIC) ammo[p][w]--;
    prjWeapon = w;
    prjOwner = p;
    float rad = angle * 0.0174533f, dx = cosf(rad), dy = -sinf(rad);
    if (p == 2) dx = -dx;
    float v = power * SPEED;
    prjX = tx[p] + dx * 6;
    prjY = ty[p] - 4 + dy * 6;
    prjVX = dx * v;
    prjVY = dy * v;
    phase = PH_FLY;
    hw::beep(220, 90);
    if (w != WP_BASIC && !ammo[p][w]) cycleWeapon(p, 1);
}

void explode(int x, int y) {
    expX = x; expY = y; expFrame = 0; expWeapon = prjWeapon;
    applyDamage(x, y, blastR(prjWeapon) + 4, damage(prjWeapon));
    phase = PH_EXPLODE;
    hw::beep(150, 200);
}

void tickFly() {
    for (int i = 0; i < 2; i++) {
        float ox = prjX, oy = prjY;
        prjX += prjVX; prjY += prjVY;
        prjVY += GRAV;
        prjVX += wind * WIND_ACC;
        int ix = (int)prjX, iy = (int)prjY;
        if (iy < 0) continue;                       // ueber dem Bild: Schwerkraft holt ihn zurueck
        if (ix < 0 || ix >= W) { switchPlayer(); return; }
        // Schild: nur, wenn der Schuss von aussen durch ein intaktes Segment kommt
        for (int p = 1; p <= 2; p++) {
            if (!shieldMask[p] || p == prjOwner) continue;
            float odx = ox - shieldX[p], ody = oy - shieldY[p], ndx = prjX - shieldX[p], ndy = prjY - shieldY[p];
            float r2 = (float)SHIELD_R * SHIELD_R;
            if (odx * odx + ody * ody > r2 && ndx * ndx + ndy * ndy <= r2) {
                float a = atan2f(ndy, ndx);
                if (a < 0) a += 6.2831853f;
                int seg = (int)(a / 6.2831853f * SHIELD_SEG) % SHIELD_SEG;
                if (shieldMask[p] & (1u << seg)) {
                    float d = sqrtf(ndx * ndx + ndy * ndy);
                    if (d < 1) d = 1;
                    explode(shieldX[p] + (int)(SHIELD_R * ndx / d), shieldY[p] + (int)(SHIELD_R * ndy / d));
                    return;
                }
            }
        }
        bool hitTank = false;
        for (int p = 1; p <= 2; p++)
            if ((ix - tx[p]) * (ix - tx[p]) + (iy - ty[p] + 2) * (iy - ty[p] + 2) < 16) hitTank = true;
        if (hitTank || iy >= groundTop(ix)) { explode(ix, iy); return; }
    }
}

void carveCrater() {
    int r = holeR(expWeapon);
    for (int x = expX - r; x <= expX + r; x++) {
        if (x < 0 || x >= W) continue;
        int dx = x - expX, dy = (int)sqrtf((float)(r * r - dx * dx));
        int top = groundTop(x), lo = expY - dy, hi = expY + dy;
        if (hi > H - 1 - BEDROCK) hi = H - 1 - BEDROCK;
        int a = lo > top ? lo : top;
        if (hi >= a) {
            int h = hgt[x] - (hi - a + 1);
            hgt[x] = (uint8_t)(h < BEDROCK ? BEDROCK : h);
        }
    }
    terrVer++;
    // Schild verschwindet, wo die Explosion ihn erfasst
    for (int p = 1; p <= 2; p++) {
        if (!shieldMask[p]) continue;
        for (int s = 0; s < SHIELD_SEG; s++) {
            float th = (s + 0.5f) / SHIELD_SEG * 6.2831853f;
            int sx = shieldX[p] + (int)(SHIELD_R * cosf(th)) - expX;
            int sy = shieldY[p] + (int)(SHIELD_R * sinf(th)) - expY;
            if (sx * sx + sy * sy <= r * r) shieldMask[p] &= ~(1u << s);
        }
    }
    ty[1] = groundTop(tx[1]);
    ty[2] = groundTop(tx[2]);
}

void tickAim(const play::Pad& pad, uint8_t edges) {
    uint32_t now = hw::ms();
    if (!selMode) {
        if (now - lastRepeat >= 80) {
            lastRepeat = now;
            int sign = active == 1 ? 1 : -1;
            if (pad.held & hw::K_LEFT) angle += 2 * sign;
            if (pad.held & hw::K_RIGHT) angle -= 2 * sign;
            if (angle > 175) angle = 175;
            if (angle < 5) angle = 5;
            if (pad.held & hw::K_UP && power < 100) power += 2;
            if (pad.held & hw::K_DOWN && power > 10) power -= 2;
        }
        if (edges & hw::K_C) { selMode = 1; hw::beep(700, 30); }
    } else {
        if (edges & hw::K_LEFT) { cycleWeapon(active, -1); hw::beep(600, 20); }
        if (edges & hw::K_RIGHT) { cycleWeapon(active, 1); hw::beep(600, 20); }
        if (edges & (hw::K_UP | hw::K_DOWN)) { selMode = 0; hw::beep(400, 20); }
        else if ((edges & hw::K_C) && now >= aimReadyAt) doFire();
    }
}

void tick(const play::Pad* pads, const uint8_t* edges) {
    switch (phase) {
        case PH_AIM: tickAim(pads[active], edges[active]); break;
        case PH_FLY: tickFly(); break;
        case PH_EXPLODE:
            if (expFrame == EXP_GROW) carveCrater();
            if (++expFrame >= EXP_TOTAL) { expFrame = 0; switchPlayer(); }
            break;
        case PH_ROUNDWAIT:
            if (hw::ms() >= roundWaitUntil) { phase = PH_AIM; aimReadyAt = hw::ms() + 500; }
            break;
    }
    curWeapon = selWeapon[active];
    curAmmo = curWeapon == WP_BASIC ? 0 : ammo[active][curWeapon];
}

// ---- Uebertragung ----
uint8_t chunkIdx;

int pack(uint8_t* out) {
    uint8_t* p = out;
    *p++ = match; *p++ = phase; *p++ = active; *p++ = (uint8_t)angle; *p++ = (uint8_t)power;
    *p++ = (uint8_t)wind; *p++ = hp[1]; *p++ = hp[2]; *p++ = wins[1]; *p++ = wins[2];
    *p++ = (uint8_t)tx[1]; *p++ = (uint8_t)ty[1]; *p++ = (uint8_t)tx[2]; *p++ = (uint8_t)ty[2];
    play::put16(p, (int)prjX); play::put16(p, (int)prjY); *p++ = prjOwner;
    *p++ = (uint8_t)expX; *p++ = (uint8_t)expY; *p++ = expFrame; *p++ = expWeapon;
    *p++ = curWeapon; *p++ = curAmmo; *p++ = selMode;
    for (int s = 1; s <= 2; s++) {
        *p++ = (uint8_t)shieldX[s]; *p++ = (uint8_t)shieldY[s];
        *p++ = (uint8_t)shieldMask[s]; *p++ = (uint8_t)(shieldMask[s] >> 8); *p++ = (uint8_t)(shieldMask[s] >> 16);
    }
    *p++ = terrVer; *p++ = chunkIdx;
    memcpy(p, hgt + chunkIdx * CHUNK, CHUNK); p += CHUNK;
    chunkIdx = (uint8_t)((chunkIdx + 1) % (W / CHUNK));
    return (int)(p - out);
}
constexpr int PKT = 36 + 2 + CHUNK;

uint8_t chunksSeen;       // Gast: welche Viertel der Karte schon da sind (Bit je Viertel)

void unpack(const uint8_t* p) {
    uint8_t prevPhase = phase, prevActive = active;
    p++;
    phase = *p++; active = *p++; angle = *p++; power = *p++; wind = (int8_t)*p++;
    hp[1] = *p++; hp[2] = *p++; wins[1] = *p++; wins[2] = *p++;
    tx[1] = *p++; ty[1] = *p++; tx[2] = *p++; ty[2] = *p++;
    prjX = (float)play::get16(p); prjY = (float)play::get16(p); prjOwner = *p++;
    expX = *p++; expY = *p++; expFrame = *p++; expWeapon = *p++;
    curWeapon = *p++; curAmmo = *p++; selMode = *p++;
    for (int s = 1; s <= 2; s++) {
        shieldX[s] = *p++; shieldY[s] = *p++;
        shieldMask[s] = p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16); p += 3;
    }
    p++;                                  // Gelaendeversion (nur zur Info)
    uint8_t ci = *p++ % (W / CHUNK);
    memcpy(hgt + ci * CHUNK, p, CHUNK);
    chunksSeen |= 1u << ci;
    if (prevPhase != PH_FLY && phase == PH_FLY) hw::beep(220, 90);
    if (prevPhase != PH_EXPLODE && phase == PH_EXPLODE) hw::beep(150, 200);
    if (prevPhase != PH_MATCH && phase == PH_MATCH) hw::beep(120, 350);
    if (phase == PH_AIM && prevActive != active && active == 2) hw::beep(900, 40);
}

// ---- Zeichnen ----
// Rohrrichtung (Einheitsvektor) fuer Spieler p
void barrelDir(int p, float& dx, float& dy) {
    int a = p == active ? angle : 45;        // der Gast kennt nur den Winkel des aktiven Spielers
    float rad = a * 0.0174533f;
    dx = cosf(rad); dy = -sinf(rad);
    if (p == 2) dx = -dx;
}

void drawTank(int p) {
    int x = tx[p], y = ty[p];
    gfx::fillRect(x - 3, y - 3, 7, 3, gfx::ON);
    gfx::fillRect(x - 2, y - 5, 5, 2, gfx::ON);
    float dx, dy;
    barrelDir(p, dx, dy);
    gfx::line(x, y - 5, x + (int)lroundf(dx * 7), y - 5 + (int)lroundf(dy * 7), gfx::ON);
}

// Beim Zielen: gestrichelte Verlaengerung des Rohrs und ein schmaler
// Kraftbalken neben dem Panzer (aussen, zum Bildrand hin).
void drawAim() {
    int x = tx[active], y = ty[active];
    float dx, dy;
    barrelDir(active, dx, dy);
    for (int d = 10; d <= 34; d += 3)
        gfx::pixel(x + (int)lroundf(dx * d), y - 5 + (int)lroundf(dy * d), gfx::ON);
    int bx = active == 1 ? x - 9 : x + 7, h = 22, fill = (power * (h - 2) + 50) / 100;
    gfx::fillRect(bx - 1, y - h - 1, 5, h + 2, gfx::OFF);
    gfx::rect(bx, y - h, 3, h, gfx::ON);
    gfx::vline(bx + 1, y - 1 - fill, fill, gfx::ON);
}

void drawWeaponChoice() {
    static const char* names[] = { "SHELL", "INFERNO", "SHOCK", "DOME" };
    char b[24];
    if (curWeapon == WP_BASIC) snprintf(b, sizeof b, "< %s >", names[0]);
    else snprintf(b, sizeof b, "< %s x%u >", names[curWeapon & 3], curAmmo);
    int w = gfx::textWidth(b) + 8;
    if (w < 70) w = 70;
    gfx::fillRect(W / 2 - w / 2, 11, w, 21, gfx::OFF);
    gfx::rect(W / 2 - w / 2, 11, w, 21, gfx::ON);
    gfx::textCenter(W / 2, 13, b);
    gfx::textCenter(W / 2, 22, "PUSH: FIRE");
}

void drawHud() {
    char b[8];
    for (int p = 1; p <= 2; p++) {
        int x = p == 1 ? 0 : W - 24;
        gfx::rect(x, 1, 24, 4, gfx::ON);
        gfx::hline(x + 1, 2, hp[p] * 22 / MAX_HP, gfx::ON);
        gfx::hline(x + 1, 3, hp[p] * 22 / MAX_HP, gfx::ON);
    }
    snprintf(b, sizeof b, "%u:%u", wins[1], wins[2]);
    gfx::textCenter(W / 2, 0, b);
    // Wind als Pfeil unter dem Stand: Laenge = Staerke
    int wx = W / 2, wy = 10, len = wind * 3;
    if (!len) gfx::pixel(wx, wy, gfx::ON);
    else {
        gfx::hline(len > 0 ? wx : wx + len, wy, (len > 0 ? len : -len) + 1, gfx::ON);
        int tip = wx + len, s = len > 0 ? -1 : 1;
        gfx::pixel(tip + s, wy - 1, gfx::ON);
        gfx::pixel(tip + s, wy + 1, gfx::ON);
    }
}

void draw() {
    gfx::clear();
    if (phase == PH_MATCH) {
        play::resultBox(wins[1] > wins[2] ? 1 : 2, wins[1], wins[2]);
        return;
    }
    // Gelaende: Oberkante durchgezogen, darunter gepunktet
    for (int x = 0; x < W; x++) {
        int top = groundTop(x);
        gfx::pixel(x, top, gfx::ON);
        if (!(x & 1))
            for (int y = top + 2 - (top & 1); y < H; y += 2) gfx::pixel(x, y, gfx::ON);
    }
    drawTank(1);
    drawTank(2);
    for (int p = 1; p <= 2; p++) {
        if (!shieldMask[p]) continue;
        for (int i = 0; i < 64; i++) {
            float th = i / 64.0f * 6.2831853f;
            if (shieldMask[p] & (1u << (i * SHIELD_SEG / 64)))
                gfx::pixel(shieldX[p] + (int)lroundf(SHIELD_R * cosf(th)), shieldY[p] + (int)lroundf(SHIELD_R * sinf(th)), gfx::ON);
        }
    }
    if (phase == PH_FLY && prjY >= 0) gfx::fillRect((int)prjX - 1, (int)prjY - 1, 2, 2, gfx::ON);
    if (phase == PH_EXPLODE) {
        int maxR = holeR(expWeapon), outer = maxR, inner = 0;
        if (expFrame < EXP_GROW) outer = maxR * (expFrame + 1) / EXP_GROW;
        else if (expFrame >= EXP_GROW + EXP_PLAT) inner = maxR * (expFrame - EXP_GROW - EXP_PLAT + 1) / EXP_SHR;
        if (inner < maxR) {
            gfx::fillCircle(expX, expY, outer, gfx::ON);
            if (expWeapon == WP_SHOCK) gfx::circle(expX, expY, outer / 2, gfx::OFF);
            if (inner > 0) gfx::fillCircle(expX, expY, inner, gfx::OFF);
        }
    }
    drawHud();
    if (phase == PH_AIM) {
        drawAim();
        bool mine = (active == 1) == play::host();
        if (mine) {                                        // Pfeil nur, wenn ich dran bin
            int x = tx[active], y = ty[active] - 12;
            gfx::fillTriangle(x - 2, y, x + 2, y, x, y + 2, gfx::ON);
        }
        if (selMode) drawWeaponChoice();
    }
    if (phase == PH_ROUNDWAIT) {
        char b[16];
        snprintf(b, sizeof b, "%u : %u", wins[1], wins[2]);
        gfx::messageBox("NEXT ROUND", b);
    }
}

}  // namespace

void host() {
    match = 0;
    initMatch();
    play::Edges ed[3];
    ed[1].sync(play::pad(1));
    ed[2].sync(play::pad(2));
    uint32_t lt = hw::ms();
    uint8_t pending[3] = {};
    while (play::running()) {
        uint32_t now = hw::ms();
        if (!play::live()) { lt = now; if (phase == PH_ROUNDWAIT) roundWaitUntil = now + 1200; }
        else {
            play::Pad pads[3] = { {}, play::pad(1), play::pad(2) };
            pending[1] |= ed[1].take(pads[1]);
            pending[2] |= ed[2].take(pads[2]);
            if (phase == PH_MATCH) {
                if ((pending[1] | pending[2]) & hw::K_C) {
                    match++;
                    initMatch();
                    hw::beep(900, 60);
                }
                pending[1] = pending[2] = 0;
            }
            for (int guard = 0; now - lt >= TICK_MS && guard < 3; guard++) {
                lt += TICK_MS;
                // nur der Spieler am Zug darf etwas ausloesen; die Eingaben des
                // anderen verfallen, damit sie nicht spaeter nachwirken
                uint8_t e[3] = { 0, pending[1], pending[2] };
                if (phase != PH_AIM) e[1] = e[2] = 0;
                tick(pads, e);
                pending[1] = pending[2] = 0;
            }
            if (now - lt >= TICK_MS) lt = now;
        }
        uint8_t buf[PKT];
        play::send(buf, pack(buf));
        draw();
        play::present();
    }
}

void guest() {
    initMatch();
    chunksSeen = 0;
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(b, &n) && n >= PKT) unpack(b);
        if (chunksSeen != 0x0F) {                 // erst zeichnen, wenn die Karte komplett ist
            gfx::clear();
            gfx::textCenter(W / 2, 28, "LOADING...");
        } else draw();
        play::present();
    }
}

}  // namespace arty
