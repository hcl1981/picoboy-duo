// Kaesekaestchen (Dots and Boxes) - hochkant. 6x8 Punkte, abwechselnd einen
// Strich ziehen; wer ein Kaestchen schliesst, bekommt es und ist nochmal dran.
// P1-Kaestchen werden gefuellt, P2-Kaestchen bekommen ein Kreuz.
//
// Steuerung: das Steuerkreuz bewegt einen Rahmen von Punkt zu Punkt. Mitte
// haelt den Punkt fest (Pfeile zeigen die freien Striche), eine Richtung zieht
// dann den Strich von dort aus; nochmal Mitte bricht ab.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "play.h"

namespace dots {
namespace {

constexpr int NX = 6, NY = 8, SP = 12, OX = 2, OY = 22;
constexpr int NUM_H = (NX - 1) * NY, NUM_V = NX * (NY - 1), NUM_LINES = NUM_H + NUM_V, NUM_BOX = (NX - 1) * (NY - 1);

bool linesH[NX - 1][NY], linesV[NX][NY - 1];
uint8_t boxes[NX - 1][NY - 1];
uint8_t score1, score2, currentPlayer, status, match;
int dotX, dotY;                 // Cursor auf einem Punkt
bool armed;                     // Punkt festgehalten, Richtung zieht den Strich

void coords(int idx, int& c, int& r, bool& h) {
    h = idx < NUM_H;
    if (h) { c = idx % (NX - 1); r = idx / (NX - 1); }
    else { int v = idx - NUM_H; c = v % NX; r = v / NX; }
}

bool drawn(int idx) {
    int c, r; bool h;
    coords(idx, c, r, h);
    return h ? linesH[c][r] : linesV[c][r];
}

bool closed(int c, int r) { return linesH[c][r] && linesH[c][r + 1] && linesV[c][r] && linesV[c + 1][r]; }

void doMove(int idx) {
    if (drawn(idx)) { hw::beep(250, 60); return; }
    int c, r; bool h;
    coords(idx, c, r, h);
    if (h) linesH[c][r] = true; else linesV[c][r] = true;
    int got = 0;
    auto claim = [&](int bc, int br) {
        if (bc >= 0 && bc < NX - 1 && br >= 0 && br < NY - 1 && !boxes[bc][br] && closed(bc, br)) {
            boxes[bc][br] = currentPlayer;
            got++;
        }
    };
    if (h) { claim(c, r - 1); claim(c, r); }
    else { claim(c - 1, r); claim(c, r); }
    if (got) {
        if (currentPlayer == 1) score1 += got; else score2 += got;
        hw::beep(1500, 80);
    } else {
        hw::beep(700, 40);
        currentPlayer = currentPlayer == 1 ? 2 : 1;
    }
    if (score1 + score2 >= NUM_BOX) {
        status = score1 > score2 ? 1 : score2 > score1 ? 2 : 3;
        hw::beep(120, 350);
    }
}

// Strich vom Punkt (x,y) in Richtung (dx,dy); -1 = ausserhalb des Felds
int lineFrom(int x, int y, int dx, int dy) {
    int nx = x + dx, ny = y + dy;
    if (nx < 0 || nx >= NX || ny < 0 || ny >= NY) return -1;
    if (dy == 0) return y * (NX - 1) + (dx > 0 ? x : nx);
    return NUM_H + (dy > 0 ? y : ny) * NX + x;
}

bool freeLine(int x, int y, int dx, int dy) {
    int i = lineFrom(x, y, dx, dy);
    return i >= 0 && !drawn(i);
}

void initMatch() {
    memset(linesH, 0, sizeof linesH);
    memset(linesV, 0, sizeof linesV);
    memset(boxes, 0, sizeof boxes);
    score1 = score2 = 0;
    currentPlayer = 1;
    status = 0;
    dotX = NX / 2; dotY = NY / 2;
    armed = false;
}

void handle(uint8_t e) {
    int dx = 0, dy = 0;
    if (e & hw::K_UP) dy = -1;
    else if (e & hw::K_DOWN) dy = 1;
    else if (e & hw::K_LEFT) dx = -1;
    else if (e & hw::K_RIGHT) dx = 1;
    if (!armed) {
        if (dx || dy) {
            int nx = dotX + dx, ny = dotY + dy;
            if (nx >= 0 && nx < NX && ny >= 0 && ny < NY) { dotX = nx; dotY = ny; hw::beep(500, 12); }
        } else if (e & hw::K_C) {
            if (freeLine(dotX, dotY, 1, 0) || freeLine(dotX, dotY, -1, 0) ||
                freeLine(dotX, dotY, 0, 1) || freeLine(dotX, dotY, 0, -1)) { armed = true; hw::beep(900, 20); }
            else hw::beep(250, 60);                 // hier ist nichts mehr frei
        }
    } else if (dx || dy) {
        if (freeLine(dotX, dotY, dx, dy)) { armed = false; doMove(lineFrom(dotX, dotY, dx, dy)); }
        else hw::beep(250, 60);
    } else if (e & hw::K_C) {
        armed = false;
        hw::beep(500, 12);
    }
}

void draw() {
    gfx::clear();
    char b[12];
    snprintf(b, sizeof b, "P1 %u", score1);
    gfx::text(0, 0, b);
    snprintf(b, sizeof b, "%u P2", score2);
    gfx::text(64 - gfx::textWidth(b), 0, b);
    bool myTurn = (currentPlayer == 1) == play::host();
    if (!status) {
        if (myTurn) {
            gfx::fillRect(0, 9, 64, 9, gfx::ON);
            gfx::textCenter(32, 10, "YOUR TURN", gfx::OFF);
        } else gfx::textCenter(32, 10, "WAIT...");
    }
    for (int c = 0; c < NX - 1; c++)
        for (int r = 0; r < NY - 1; r++) {
            int x = OX + c * SP, y = OY + r * SP;
            if (boxes[c][r] == 1) gfx::fillRect(x + 3, y + 3, SP - 5, SP - 5, gfx::ON);
            else if (boxes[c][r] == 2) {
                gfx::line(x + 3, y + 3, x + SP - 3, y + SP - 3, gfx::ON);
                gfx::line(x + SP - 3, y + 3, x + 3, y + SP - 3, gfx::ON);
            }
        }
    for (int r = 0; r < NY; r++)
        for (int c = 0; c < NX - 1; c++)
            if (linesH[c][r]) gfx::hline(OX + c * SP, OY + r * SP, SP, gfx::ON);
    for (int r = 0; r < NY - 1; r++)
        for (int c = 0; c < NX; c++)
            if (linesV[c][r]) gfx::vline(OX + c * SP, OY + r * SP, SP, gfx::ON);
    for (int r = 0; r < NY; r++)
        for (int c = 0; c < NX; c++) gfx::fillRect(OX + c * SP - 1, OY + r * SP - 1, 3, 3, gfx::ON);   // 3x3: Striche laufen mittig
    // Cursor nur bei dem, der dran ist: Rahmen um den Punkt, blinkend - fest
    // mit Pfeilen zu den freien Strichen, sobald der Punkt festgehalten ist
    if (!status && myTurn) {
        int x = OX + dotX * SP, y = OY + dotY * SP;
        if (armed || (hw::ms() / 250) & 1) gfx::rect(x - 3, y - 3, 7, 7, gfx::ON);
        if (armed) {
            static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (auto& d : D) {
                if (!freeLine(dotX, dotY, d[0], d[1])) continue;
                int ax = d[0], ay = d[1], px = -ay, py = ax;      // Richtung und quer dazu
                for (int t = 4; t <= 8; t++) gfx::pixel(x + ax * t, y + ay * t, gfx::ON);
                gfx::pixel(x + ax * 7 + px, y + ay * 7 + py, gfx::ON);
                gfx::pixel(x + ax * 7 - px, y + ay * 7 - py, gfx::ON);
                gfx::pixel(x + ax * 6 + 2 * px, y + ay * 6 + 2 * py, gfx::ON);
                gfx::pixel(x + ax * 6 - 2 * px, y + ay * 6 - 2 * py, gfx::ON);
            }
        }
    }
    if (status) play::resultBox(status, score1, score2);
}

int pack(uint8_t* out) {
    uint8_t* p = out;
    *p++ = match; *p++ = (uint8_t)((dotY * NX + dotX) | (armed ? 0x80 : 0)); *p++ = currentPlayer; *p++ = score1; *p++ = score2; *p++ = status;
    uint8_t bits[11] = {};
    for (int i = 0; i < NUM_LINES; i++) if (drawn(i)) bits[i >> 3] |= (uint8_t)(1u << (i & 7));
    memcpy(p, bits, 11); p += 11;
    uint8_t bx[9] = {};
    for (int i = 0; i < NUM_BOX; i++) bx[i / 4] |= (uint8_t)(boxes[i % (NX - 1)][i / (NX - 1)] << ((i % 4) * 2));
    memcpy(p, bx, 9); p += 9;
    return (int)(p - out);
}
constexpr int PKT = 6 + 11 + 9;

void unpack(const uint8_t* p) {
    uint8_t prev = status, prevPlayer = currentPlayer;
    dotX = (p[1] & 0x7F) % NX; dotY = (p[1] & 0x7F) / NX; armed = p[1] & 0x80; currentPlayer = p[2]; score1 = p[3]; score2 = p[4]; status = p[5];
    p += 6;
    for (int i = 0; i < NUM_LINES; i++) {
        int c, r; bool h;
        coords(i, c, r, h);
        bool v = (p[i >> 3] >> (i & 7)) & 1;
        if (h) linesH[c][r] = v; else linesV[c][r] = v;
    }
    p += 11;
    for (int i = 0; i < NUM_BOX; i++) boxes[i % (NX - 1)][i / (NX - 1)] = (p[i / 4] >> ((i % 4) * 2)) & 3;
    if (!prev && status) hw::beep(120, 350);
    else if (prevPlayer != currentPlayer) hw::beep(700, 40);
}

}  // namespace

void host() {
    initMatch();
    match = 0;
    play::Edges e1, e2;
    e1.sync(play::pad(1));
    e2.sync(play::pad(2));
    uint32_t endAt = 0;
    while (play::running()) {
        if (play::live()) {
            uint8_t k1 = e1.take(play::pad(1)), k2 = e2.take(play::pad(2));
            if (status) {
                if (hw::ms() - endAt > 800 && ((k1 | k2) & hw::K_C)) { initMatch(); match++; hw::beep(900, 60); }
            } else {
                handle(currentPlayer == 1 ? k1 : k2);
                if (status) endAt = hw::ms();
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

}  // namespace dots
