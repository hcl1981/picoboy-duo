// Lightcycle (Tron) - quer. Beide fahren gleichzeitig; wer in eine Spur oder
// die Wand faehrt, verliert die Runde.
//
// Spielfeld 62x30 Zellen zu 2x2 Pixeln. P1 zieht eine volle Spur, P2 eine
// gerasterte, damit man sie auf dem Schwarzweiss-Display unterscheiden kann.
//
// Uebertragung: Das Gitter selbst ist zu gross fuer ein Paket (465 Byte). Der
// Host schickt deshalb (a) die Richtungen der letzten 32 Schritte, aus denen
// der Gast die Spuren lueckenlos nachzeichnet, und (b) reihum ein Stueck des
// echten Gitters, das eventuelle Abweichungen wieder geraderueckt.

#include <initializer_list>
#include <string.h>
#include "play.h"

namespace tron {
namespace {

constexpr int GW = 62, GH = 30, OX = 2, OY = 2;
constexpr uint32_t STEP_MS = 75, PREROLL_MS = 1200;
constexpr int HIST = 32;                            // Schritte Verlauf im Paket
constexpr int CHUNK_CELLS = 124;                    // 31 Byte je Gitterstueck
constexpr int CHUNKS = (GW * GH + CHUNK_CELLS - 1) / CHUNK_CELLS;   // 15
constexpr int PKT = 13 + HIST / 2 + 1 + CHUNK_CELLS / 4;         // 61 Byte

enum { UP, RT, DN, LF };
const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

struct Bike { int x, y; uint8_t d; };

uint8_t grid[GW][GH];          // 0 leer, 1 Spur P1, 2 Spur P2
Bike b1, b2;
uint8_t status, s1, s2, roundNo, preroll;
uint16_t step;
uint8_t hist[2][HIST / 4];     // Index 0 = juengster Schritt, 2 Bit je Schritt
uint8_t chunk;

void initRound() {
    memset(grid, 0, sizeof grid);
    memset(hist, 0, sizeof hist);
    b1 = { 8, GH / 2, RT };
    b2 = { GW - 9, GH / 2, LF };
    grid[b1.x][b1.y] = 1;
    grid[b2.x][b2.y] = 2;
    status = 0;
    step = 0;
}

void pushHist(uint8_t* h, uint8_t d) {
    for (int i = HIST / 4 - 1; i > 0; i--) h[i] = (uint8_t)((h[i] << 2) | (h[i - 1] >> 6));
    h[0] = (uint8_t)((h[0] << 2) | d);
}
uint8_t histAt(const uint8_t* h, int i) { return (h[i / 4] >> ((i % 4) * 2)) & 3; }

bool opposite(uint8_t a, uint8_t b) { return (a ^ b) == 2; }

// Frisch gedrueckte Richtung vor gehaltener; nie um 180 Grad wenden.
uint8_t wantDir(uint8_t edges, uint8_t held, uint8_t cur) {
    static const uint8_t keyDir[4] = { UP, DN, LF, RT };   // Reihenfolge der K_*-Bits
    for (uint8_t m : { edges, held })
        for (int k = 0; k < 4; k++)
            if (m & (1u << k) && !opposite(keyDir[k], cur)) return keyDir[k];
    return cur;
}

bool blocked(int x, int y) { return x < 0 || x >= GW || y < 0 || y >= GH || grid[x][y]; }

void hostStep(uint8_t e1, uint8_t h1, uint8_t e2, uint8_t h2, bool moving) {
    b1.d = wantDir(e1, h1, b1.d);
    b2.d = wantDir(e2, h2, b2.d);
    if (!moving) return;
    int n1x = b1.x + DX[b1.d], n1y = b1.y + DY[b1.d];
    int n2x = b2.x + DX[b2.d], n2y = b2.y + DY[b2.d];
    bool d1 = blocked(n1x, n1y), d2 = blocked(n2x, n2y);
    if (!d1 && !d2 && n1x == n2x && n1y == n2y) d1 = d2 = true;              // Frontal
    if (!d1 && !d2 && n1x == b2.x && n1y == b2.y && n2x == b1.x && n2y == b1.y) d1 = d2 = true;
    if (d1 || d2) {
        status = d1 && d2 ? 3 : d1 ? 2 : 1;
        if (status == 1) s1++;
        else if (status == 2) s2++;
        hw::beep(120, 250);
        return;
    }
    b1.x = n1x; b1.y = n1y; b2.x = n2x; b2.y = n2y;
    grid[b1.x][b1.y] = 1;
    grid[b2.x][b2.y] = 2;
    step++;
    pushHist(hist[0], b1.d);
    pushHist(hist[1], b2.d);
}

int pack(uint8_t* out) {
    uint8_t* p = out;
    *p++ = roundNo; *p++ = status; *p++ = s1; *p++ = s2; *p++ = preroll;
    play::put16(p, step);
    *p++ = (uint8_t)b1.x; *p++ = (uint8_t)b1.y; *p++ = b1.d;
    *p++ = (uint8_t)b2.x; *p++ = (uint8_t)b2.y; *p++ = b2.d;
    memcpy(p, hist, sizeof hist); p += sizeof hist;
    // ein Stueck des Gitters, 2 Bit je Zelle
    *p++ = chunk;
    int first = chunk * CHUNK_CELLS;
    memset(p, 0, CHUNK_CELLS / 4);
    for (int i = 0; i < CHUNK_CELLS && first + i < GW * GH; i++) {
        int c = first + i;
        p[i / 4] |= (uint8_t)(grid[c % GW][c / GW] << ((i % 4) * 2));
    }
    p += CHUNK_CELLS / 4;
    chunk = (uint8_t)((chunk + 1) % CHUNKS);
    return (int)(p - out);
}

void draw() {
    gfx::clear();
    gfx::rect(OX - 1, OY - 1, GW * 2 + 2, GH * 2 + 2, gfx::ON);
    for (int x = 0; x < GW; x++)
        for (int y = 0; y < GH; y++) {
            int px = OX + x * 2, py = OY + y * 2;
            if (grid[x][y] == 1) gfx::fillRect(px, py, 2, 2, gfx::ON);
            else if (grid[x][y] == 2) { gfx::pixel(px, py, gfx::ON); gfx::pixel(px + 1, py + 1, gfx::ON); }
        }
    // Koepfe voll (auch bei der gerasterten P2-Spur), so dick wie die Spur;
    // waehrend des Vorlaufs blinkt der eigene
    bool blink = preroll && (hw::ms() / 150) & 1;
    const Bike& me = play::host() ? b1 : b2;
    const Bike& other = play::host() ? b2 : b1;
    gfx::fillRect(OX + other.x * 2, OY + other.y * 2, 2, 2, gfx::ON);
    if (!blink) gfx::fillRect(OX + me.x * 2, OY + me.y * 2, 2, 2, gfx::ON);
    else gfx::fillRect(OX + me.x * 2, OY + me.y * 2, 2, 2, gfx::OFF);
    if (preroll) {
        int tx = OX + me.x * 2 - 8, ty = OY + me.y * 2 - 12;
        if (tx < 1) tx = 1;
        if (tx > 128 - 19) tx = 128 - 19;
        if (ty < 1) ty = OY + me.y * 2 + 5;                 // oben kein Platz: darunter
        gfx::text(tx, ty, "YOU", gfx::ON, 1, gfx::OFF);
    }
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
    uint32_t lt = hw::ms(), last = hw::ms(), endAt = 0, prerollLeft = PREROLL_MS;
    while (play::running()) {
        uint32_t now = hw::ms(), dt = now - last;
        last = now;
        if (!play::live()) lt = now;                 // Pause: nichts laeuft weiter
        else {
            play::Pad p1 = play::pad(1), p2 = play::pad(2);
            prerollLeft = prerollLeft > dt ? prerollLeft - dt : 0;
            preroll = prerollLeft > 0;
            if (status) {
                uint8_t k = e1.take(p1) | e2.take(p2);
                if (now - endAt > 700 && (k & hw::K_C)) {
                    roundNo++;
                    initRound();
                    prerollLeft = PREROLL_MS;
                    lt = now;
                    hw::beep(900, 60);
                }
            } else {
                for (int guard = 0; now - lt >= STEP_MS && guard < 3; guard++) {
                    lt += STEP_MS;
                    hostStep(e1.take(p1) & hw::K_DIRS, p1.held, e2.take(p2) & hw::K_DIRS, p2.held, !preroll);
                    if (status) { endAt = now; break; }
                }
                if (now - lt >= STEP_MS) lt = now;
            }
        }
        uint8_t buf[play::PAYLOAD_MAX];
        play::send(buf, pack(buf));
        draw();
        play::present();
    }
}

void guest() {
    s1 = s2 = 0;
    initRound();
    int myRound = -1;
    uint8_t buf[play::PAYLOAD_MAX];
    int n;
    while (play::running()) {
        if (play::receive(buf, &n) && n >= PKT) {
            const uint8_t* p = buf;
            uint8_t r = *p++, st = *p++;
            s1 = *p++; s2 = *p++; preroll = *p++;
            int hstep = (uint16_t)play::get16(p);
            Bike n1 = { p[0], p[1], p[2] }, n2 = { p[3], p[4], p[5] };
            p += 6;
            const uint8_t* h1 = p;
            const uint8_t* h2 = p + HIST / 4;
            p += sizeof hist;
            if (r != myRound) { myRound = r; initRound(); }
            int missing = hstep - step;
            if (missing > 0 && missing <= HIST) {
                for (int s = step + 1; s <= hstep; s++) {
                    int i = hstep - s;
                    uint8_t d1 = histAt(h1, i), d2 = histAt(h2, i);
                    b1.x += DX[d1]; b1.y += DY[d1];
                    b2.x += DX[d2]; b2.y += DY[d2];
                    if (b1.x >= 0 && b1.x < GW && b1.y >= 0 && b1.y < GH) grid[b1.x][b1.y] = 1;
                    if (b2.x >= 0 && b2.x < GW && b2.y >= 0 && b2.y < GH) grid[b2.x][b2.y] = 2;
                }
            }
            step = (uint16_t)hstep;
            b1 = n1; b2 = n2;
            if (b1.x < GW && b1.y < GH) grid[b1.x][b1.y] = 1;
            if (b2.x < GW && b2.y < GH) grid[b2.x][b2.y] = 2;
            // Gitterstueck des Hosts uebernehmen (heilt jede Abweichung)
            int c0 = *p++ * CHUNK_CELLS;
            for (int i = 0; i < CHUNK_CELLS && c0 + i < GW * GH; i++) {
                int c = c0 + i;
                grid[c % GW][c / GW] = (p[i / 4] >> ((i % 4) * 2)) & 3;
            }
            if (!status && st) hw::beep(120, 250);
            if (status && !st) hw::beep(900, 60);
            status = st;
        }
        draw();
        play::present();
    }
}

}  // namespace tron
