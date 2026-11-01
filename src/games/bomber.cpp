// Bombing Bob (Bomberman) - quer. Laufen, Bomben legen, Ziegel wegsprengen,
// den Gegner erwischen. Unter Ziegeln liegen Extras: B = eine Bombe mehr,
// F = groessere Reichweite.
//
// Feld 15x7 Kacheln zu 8x8 Pixeln (das Original hatte 13x11 zu 16 px - auf
// 64 Pixeln Hoehe waeren das nur 5-Pixel-Kacheln geworden). P1 ist eine
// gefuellte Kugel, P2 ein Umriss.

#include <string.h>
#include "play.h"

namespace bomber {
namespace {

constexpr int COLS = 15, ROWS = 7, CELL = 8, OX = 4, OY = 8;
constexpr int GRIDB = (COLS * ROWS * 3 + 7) / 8;     // 40 Byte, 3 Bit je Zelle
constexpr uint32_t TICK_MS = 40;
constexpr int SPEED = 1, HALF = 3;
constexpr uint8_t FUSE = 50, FLAME = 14, SPREAD = 1;
constexpr int MAXBOMB = 12, CAP_BOMB = 4, CAP_RANGE = 5;

enum { EMPTY = 0, BRICK = 1, WALL = 2, R_FLAME = 3, R_BOMB = 4, R_PBOMB = 5, R_PFIRE = 6 };

struct Player { int px, py; bool alive; uint8_t face, walk; int obc, obr; uint8_t maxB, range; };
struct Bomb { bool on; int c, r; uint8_t fuse, owner, range; };

Player pl[3];                     // Index 1/2
uint8_t s1, s2, status, roundNo;
uint8_t terr[COLS][ROWS], fl[COLS][ROWS], pup[COLS][ROWS];
Bomb bombs[MAXBOMB];
uint8_t rgrid[COLS][ROWS];        // was gezeichnet wird (Host baut, Gast empfaengt)

bool burning(int c, int r) { return fl[c][r] > 0 && fl[c][r] <= FLAME; }

int bombAt(int c, int r) {
    for (int i = 0; i < MAXBOMB; i++) if (bombs[i].on && bombs[i].c == c && bombs[i].r == r) return i;
    return -1;
}

int bombCount(uint8_t o) {
    int n = 0;
    for (const Bomb& b : bombs) if (b.on && b.owner == o) n++;
    return n;
}

// Fest fuer eine Figur; ihre eben gelegte Bombe (sc,sr) laesst sie noch durch.
bool solidPx(int fx, int fy, int sc, int sr) {
    if (fx < 0 || fy < 0) return true;
    int c = fx / CELL, r = fy / CELL;
    if (c >= COLS || r >= ROWS) return true;
    if (terr[c][r] != EMPTY) return true;
    return bombAt(c, r) >= 0 && !(c == sc && r == sr);
}

bool canBeAt(int px, int py, int sc, int sr) {
    return !solidPx(px - HALF, py - HALF, sc, sr) && !solidPx(px + HALF, py - HALF, sc, sr)
        && !solidPx(px - HALF, py + HALF, sc, sr) && !solidPx(px + HALF, py + HALF, sc, sr);
}

bool boxInCell(int px, int py, int c, int r) {
    return c >= (px - HALF) / CELL && c <= (px + HALF) / CELL && r >= (py - HALF) / CELL && r <= (py + HALF) / CELL;
}

uint8_t dirOf(uint8_t held) {
    if (held & hw::K_UP) return 0;
    if (held & hw::K_RIGHT) return 1;
    if (held & hw::K_DOWN) return 2;
    if (held & hw::K_LEFT) return 3;
    return 0xFF;
}

// Bewegung mit sanftem Einrasten in die Gaenge (wie im Original)
void movePlayer(Player& p, uint8_t dir) {
    if (dir == 0xFF) return;
    p.face = dir;
    int ccx = p.px / CELL * CELL + CELL / 2, ccy = p.py / CELL * CELL + CELL / 2;
    bool moved = false;
    if (dir == 1 || dir == 3) {
        if (p.py != ccy) { p.py += p.py < ccy ? SPEED : -SPEED; moved = true; }
        int nx = p.px + (dir == 1 ? SPEED : -SPEED);
        if (canBeAt(nx, p.py, p.obc, p.obr)) { p.px = nx; moved = true; }
    } else {
        if (p.px != ccx) { p.px += p.px < ccx ? SPEED : -SPEED; moved = true; }
        int ny = p.py + (dir == 0 ? -SPEED : SPEED);
        if (canBeAt(p.px, ny, p.obc, p.obr)) { p.py = ny; moved = true; }
    }
    if (moved) p.walk++;
    if (p.obc >= 0 && !boxInCell(p.px, p.py, p.obc, p.obr)) p.obc = p.obr = -1;
}

void tryPlace(Player& p, uint8_t owner) {
    int c = p.px / CELL, r = p.py / CELL;
    if (bombCount(owner) >= p.maxB || bombAt(c, r) >= 0 || terr[c][r] != EMPTY) return;
    for (Bomb& b : bombs)
        if (!b.on) {
            b = { true, c, r, FUSE, owner, p.range };
            p.obc = c; p.obr = r;
            hw::beep(400, 20);
            return;
        }
}

// Zentrum zuendet sofort, jede weitere Kachel SPREAD Takte spaeter.
void explodeAt(int bc, int br, uint8_t range) {
    fl[bc][br] = FLAME;
    pup[bc][br] = 0;
    const int dc[4] = { 0, 1, 0, -1 }, dr[4] = { -1, 0, 1, 0 };
    for (int d = 0; d < 4; d++)
        for (int i = 1; i <= range; i++) {
            int c = bc + dc[d] * i, r = br + dr[d] * i;
            if (c < 0 || c >= COLS || r < 0 || r >= ROWS || terr[c][r] == WALL) break;
            uint8_t life = (uint8_t)(FLAME + i * SPREAD);
            if (terr[c][r] == BRICK) {
                terr[c][r] = EMPTY;
                fl[c][r] = life;
                if (hw::rndRange(0, 99) < 18) pup[c][r] = (uint8_t)hw::rndRange(1, 2);
                break;
            }
            fl[c][r] = life;
            pup[c][r] = 0;
            int bi = bombAt(c, r);
            if (bi >= 0) bombs[bi].fuse = 0;          // Kettenreaktion
        }
}

void collect(Player& p) {
    int c = p.px / CELL, r = p.py / CELL;
    uint8_t u = pup[c][r];
    if (!u) return;
    if (u == 1 && p.maxB < CAP_BOMB) { p.maxB++; hw::beep(1600, 60); }
    else if (u == 2 && p.range < CAP_RANGE) { p.range++; hw::beep(1900, 60); }
    else hw::beep(1200, 40);
    pup[c][r] = 0;
}

void initRound() {
    for (int x = 0; x < COLS; x++)
        for (int y = 0; y < ROWS; y++) {
            bool border = x == 0 || y == 0 || x == COLS - 1 || y == ROWS - 1;
            terr[x][y] = border || (x % 2 == 0 && y % 2 == 0) ? WALL : hw::rndRange(0, 99) < 62 ? BRICK : EMPTY;
            fl[x][y] = pup[x][y] = 0;
        }
    const int free[6][2] = { { 1, 1 }, { 2, 1 }, { 1, 2 }, { COLS - 2, ROWS - 2 }, { COLS - 3, ROWS - 2 }, { COLS - 2, ROWS - 3 } };
    for (auto& f : free) terr[f[0]][f[1]] = EMPTY;
    for (Bomb& b : bombs) b.on = false;
    pl[1] = { CELL + CELL / 2, CELL + CELL / 2, true, 2, 0, -1, -1, 1, 2 };
    pl[2] = { (COLS - 2) * CELL + CELL / 2, (ROWS - 2) * CELL + CELL / 2, true, 0, 0, -1, -1, 1, 2 };
    status = 0;
}

void buildRender() {
    for (int x = 0; x < COLS; x++)
        for (int y = 0; y < ROWS; y++) {
            if (burning(x, y)) rgrid[x][y] = R_FLAME;
            else if (bombAt(x, y) >= 0) rgrid[x][y] = R_BOMB;
            else if (!fl[x][y] && pup[x][y]) rgrid[x][y] = pup[x][y] == 1 ? R_PBOMB : R_PFIRE;
            else rgrid[x][y] = terr[x][y];
        }
}

void tick(uint8_t d1, bool b1, uint8_t d2, bool b2) {
    if (status) return;
    for (int i = 1; i <= 2; i++) {
        Player& p = pl[i];
        if (!p.alive) continue;
        movePlayer(p, i == 1 ? d1 : d2);
        collect(p);
        if (i == 1 ? b1 : b2) tryPlace(p, (uint8_t)i);
    }
    for (Bomb& b : bombs) if (b.on && b.fuse) b.fuse--;
    bool again = true, boom = false;
    while (again) {
        again = false;
        for (Bomb& b : bombs)
            if (b.on && !b.fuse) { b.on = false; explodeAt(b.c, b.r, b.range); again = boom = true; }
    }
    if (boom) hw::beep(90, 70);
    for (auto& col : fl) for (uint8_t& f : col) if (f) f--;
    for (int i = 1; i <= 2; i++)
        if (pl[i].alive && burning(pl[i].px / CELL, pl[i].py / CELL)) pl[i].alive = false;
    if (!pl[1].alive || !pl[2].alive) {
        if (!pl[1].alive && !pl[2].alive) status = 3;
        else if (!pl[2].alive) { status = 1; s1++; }
        else { status = 2; s2++; }
        hw::beep(120, 300);
    }
}

// ---- Uebertragung ----
int pack(uint8_t* out) {
    buildRender();
    uint8_t* p = out;
    *p++ = roundNo; *p++ = status; *p++ = s1; *p++ = s2;
    for (int i = 1; i <= 2; i++) {
        *p++ = (uint8_t)pl[i].px; *p++ = (uint8_t)pl[i].py;
        *p++ = (uint8_t)((pl[i].alive ? 1 : 0) | (pl[i].face << 1) | (((pl[i].walk >> 2) & 1) << 3));
    }
    memset(p, 0, GRIDB);
    int bit = 0;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            for (int k = 0; k < 3; k++, bit++)
                if (rgrid[x][y] & (1u << k)) p[bit >> 3] |= (uint8_t)(1u << (bit & 7));
    return (int)(p + GRIDB - out);
}
constexpr int PKT = 4 + 6 + GRIDB;

void unpack(const uint8_t* p) {
    p++;
    uint8_t st = *p++;
    s1 = *p++; s2 = *p++;
    for (int i = 1; i <= 2; i++) {
        pl[i].px = *p++; pl[i].py = *p++;
        uint8_t f = *p++;
        pl[i].alive = f & 1;
        pl[i].face = (f >> 1) & 3;
        pl[i].walk = (uint8_t)(((f >> 3) & 1) << 2);
    }
    int bit = 0, flames = 0;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) {
            uint8_t v = 0;
            for (int k = 0; k < 3; k++, bit++)
                if (p[bit >> 3] & (1u << (bit & 7))) v |= (uint8_t)(1u << k);
            rgrid[x][y] = v;
            if (v == R_FLAME) flames++;
        }
    static int prevFlames;
    if (!prevFlames && flames) hw::beep(90, 70);
    prevFlames = flames;
    if (!status && st) hw::beep(120, 300);
    status = st;
}

// ---- Zeichnen ----
// Figur 7x7 - genau so gross wie ihr Platz (HALF = 3), damit an einer Wand
// nichts in die Kachel ragt. Zeilen als Bitmuster, Bit 6 = links.
// P1 gefuellt, P2 als Umriss; die Fuesse wechseln im Lauftakt.
constexpr uint8_t FIG1[6] = { 0x1C, 0x3E, 0x6B, 0x7F, 0x7F, 0x3E };   // ..###.. .#####. ##.#.## ...
constexpr uint8_t FIG2[6] = { 0x1C, 0x22, 0x55, 0x41, 0x41, 0x3E };   // ..###.. .#...#. #.#.#.# ...
constexpr uint8_t FEET[2] = { 0x22, 0x14 };                            // .#...#.  /  ..#.#..

void drawFigure(int i) {
    const Player& p = pl[i];
    if (!p.alive) return;
    int x0 = OX + p.px - HALF, y0 = OY + p.py - HALF;
    int step = (p.walk >> 2) & 1;
    const uint8_t* body = i == 1 ? FIG1 : FIG2;
    for (int y = 0; y < 7; y++) {
        uint8_t shape = y < 6 ? FIG1[y] : (uint8_t)(FEET[0] | FEET[1]);   // Hintergrund (Bombe/Flamme) frei machen
        uint8_t bits = y < 6 ? body[y] : FEET[step];
        for (int x = 0; x < 7; x++) {
            uint8_t m = (uint8_t)(0x40 >> x);
            if (bits & m) gfx::pixel(x0 + x, y0 + y, gfx::ON);
            else if (shape & m) gfx::pixel(x0 + x, y0 + y, gfx::OFF);
        }
    }
}

void drawCell(int x, int y, uint8_t v, uint8_t anim) {
    int px = OX + x * CELL, py = OY + y * CELL;
    switch (v) {
        case WALL:
            gfx::fillRect(px, py, CELL - 1, CELL - 1, gfx::ON);
            gfx::pixel(px + 2, py + 2, gfx::OFF);
            break;
        case BRICK:                                    // leeres Kaestchen mit einem Pixel -
            gfx::rect(px, py, CELL - 1, CELL - 1, gfx::ON);   // das Gegenstueck zur vollen Mauer
            gfx::pixel(px + 2, py + 2, gfx::ON);
            break;
        case R_FLAME: {
            int cx = px + 3, cy = py + 3;
            gfx::fillRect(cx - 1, cy - 1, 3, 3, gfx::ON);
            int a = anim & 1 ? 3 : 2;
            gfx::hline(cx - a, cy, 2 * a + 1, gfx::ON);
            gfx::vline(cx, cy - a, 2 * a + 1, gfx::ON);
            if (anim & 1) { gfx::pixel(cx - 2, cy - 2, gfx::ON); gfx::pixel(cx + 2, cy + 2, gfx::ON); }
            else { gfx::pixel(cx + 2, cy - 2, gfx::ON); gfx::pixel(cx - 2, cy + 2, gfx::ON); }
            break;
        }
        case R_BOMB:
            gfx::fillCircle(px + 3, py + 4, 2, gfx::ON);
            gfx::pixel(px + 5, py + 1, gfx::ON);
            if (anim & 1) gfx::pixel(px + 6, py, gfx::ON);    // Funke an der Lunte
            break;
        case R_PBOMB:
        case R_PFIRE:
            gfx::fillRect(px, py, CELL - 1, CELL - 1, gfx::ON);
            gfx::text(px + 1, py, v == R_PBOMB ? "B" : "F", gfx::OFF);
            break;
    }
}

void draw() {
    gfx::clear();
    char b[4] = { 0 };
    // Anzeige: Symbol und Punkte beider Spieler, "DU" beim eigenen
    gfx::fillCircle(3, 3, 3, gfx::ON);
    b[0] = (char)('0' + s1 % 10); gfx::text(9, 0, b);
    gfx::circle(124, 3, 3, gfx::ON);
    b[0] = (char)('0' + s2 % 10); gfx::text(114, 0, b);
    gfx::textCenter(64, 0, play::host() ? "<YOU" : "YOU>");
    uint8_t anim = (uint8_t)(hw::ms() / 100);
    for (int x = 0; x < COLS; x++)
        for (int y = 0; y < ROWS; y++) drawCell(x, y, rgrid[x][y], anim);
    drawFigure(1);
    drawFigure(2);
    if (status) play::resultBox(status, s1, s2);
}

}  // namespace

void host() {
    s1 = s2 = 0;
    roundNo = 0;
    initRound();
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t lt = hw::ms(), endAt = 0;
    bool bomb1 = false, bomb2 = false;
    while (play::running()) {
        uint32_t now = hw::ms();
        if (!play::live()) lt = now;
        else {
            play::Pad a = play::pad(1), b = play::pad(2);
            uint8_t k1 = e1.take(a), k2 = e2.take(b);
            if (status) {
                if (now - endAt > 800 && ((k1 | k2) & hw::K_C)) {
                    roundNo++;
                    initRound();
                    bomb1 = bomb2 = false;
                    hw::beep(900, 60);
                }
            } else {
                bomb1 |= (k1 & hw::K_C) != 0;        // bis zum naechsten Takt merken
                bomb2 |= (k2 & hw::K_C) != 0;
                for (int guard = 0; now - lt >= TICK_MS && guard < 3; guard++) {
                    lt += TICK_MS;
                    tick(dirOf(a.held), bomb1, dirOf(b.held), bomb2);
                    bomb1 = bomb2 = false;
                    if (status) { endAt = now; break; }
                }
                if (now - lt >= TICK_MS) lt = now;
            }
        }
        buildRender();
        uint8_t buf[PKT];
        play::send(buf, pack(buf));
        draw();
        play::present();
    }
}

void guest() {
    s1 = s2 = 0;
    initRound();
    buildRender();
    uint8_t b[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(b, &n) && n >= PKT) unpack(b);
        draw();
        play::present();
    }
}

}  // namespace bomber
