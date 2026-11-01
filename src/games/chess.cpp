// Schach - quer. Host = Weiss (P1), Gast = Schwarz (P2). Rochade, en passant,
// Umwandlung mit Auswahl, Schach-Anzeige, keine illegalen Zuege, Matt und Patt.
// Beim Gast ist das Brett um 180 Grad gedreht (Schwarz spielt von unten).
//
// Brett 64x64 links (Felder 8x8, dunkle Felder gepunktet), rechts die Anzeige.
// Weisse Figuren sind gefuellt, schwarze nur Umriss.

#include <stdlib.h>
#include <string.h>
#include "play.h"

namespace chess {
namespace {

enum { PT_NONE, PT_PAWN, PT_KNIGHT, PT_BISHOP, PT_ROOK, PT_QUEEN, PT_KING };
constexpr uint8_t BLACK = 0x08;
constexpr int PX = 67;                      // Anzeige rechts vom Brett

// 7x7-Silhouetten, Bit 6 = linke Spalte
const uint8_t GLYPH[7][7] = {
    {},
    { 0x00, 0x08, 0x1C, 0x1C, 0x08, 0x1C, 0x3E },   // Bauer
    { 0x18, 0x3C, 0x76, 0x4E, 0x1E, 0x3E, 0x3E },   // Springer
    { 0x08, 0x1C, 0x36, 0x3E, 0x1C, 0x08, 0x3E },   // Laeufer
    { 0x2A, 0x3E, 0x1C, 0x1C, 0x1C, 0x3E, 0x3E },   // Turm
    { 0x49, 0x6B, 0x7F, 0x3E, 0x1C, 0x3E, 0x3E },   // Dame
    { 0x08, 0x1C, 0x08, 0x3E, 0x3E, 0x1C, 0x3E },   // Koenig
};

uint8_t board[8][8];                        // [y][x], y = 0 ist Reihe 8 (Schwarz)
uint8_t currentPlayer, status, inCheck, phase, promoSel, match;
int8_t curX, curY, selX, selY, promoX, promoY, epX, epY, lastFX, lastFY, lastTX, lastTY;
bool cWK, cWQ, cBK, cBQ;

inline bool inside(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
inline uint8_t ptype(uint8_t p) { return p & 7; }
inline uint8_t colorOf(uint8_t p) { return p == 0 ? 0 : (p & BLACK) ? 2 : 1; }
inline uint8_t mk(uint8_t t, bool black) { return (uint8_t)(t | (black ? BLACK : 0)); }

// ---- Regeln (wie in der i2collection) ----
bool sliderHit(uint8_t bd[8][8], int x, int y, int dx, int dy, int tx, int ty) {
    for (int nx = x + dx, ny = y + dy; inside(nx, ny); nx += dx, ny += dy) {
        if (nx == tx && ny == ty) return true;
        if (bd[ny][nx]) return false;
    }
    return false;
}

bool isAttacked(uint8_t bd[8][8], int tx, int ty, uint8_t by) {
    static const int KX[8] = { 1, 2, 2, 1, -1, -2, -2, -1 }, KY[8] = { -2, -1, 1, 2, 2, 1, -1, -2 };
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            uint8_t p = bd[y][x];
            if (!p || colorOf(p) != by) continue;
            uint8_t t = ptype(p);
            if (t == PT_PAWN) {
                int dir = by == 1 ? -1 : 1;
                if (y + dir == ty && abs(x - tx) == 1) return true;
            } else if (t == PT_KNIGHT) {
                for (int k = 0; k < 8; k++) if (x + KX[k] == tx && y + KY[k] == ty) return true;
            } else if (t == PT_KING) {
                if (abs(x - tx) <= 1 && abs(y - ty) <= 1 && !(x == tx && y == ty)) return true;
            } else {
                bool orth = t == PT_ROOK || t == PT_QUEEN, diag = t == PT_BISHOP || t == PT_QUEEN;
                if (orth && (sliderHit(bd, x, y, 1, 0, tx, ty) || sliderHit(bd, x, y, -1, 0, tx, ty) ||
                             sliderHit(bd, x, y, 0, 1, tx, ty) || sliderHit(bd, x, y, 0, -1, tx, ty))) return true;
                if (diag && (sliderHit(bd, x, y, 1, 1, tx, ty) || sliderHit(bd, x, y, 1, -1, tx, ty) ||
                             sliderHit(bd, x, y, -1, 1, tx, ty) || sliderHit(bd, x, y, -1, -1, tx, ty))) return true;
            }
        }
    return false;
}

bool findKing(uint8_t bd[8][8], uint8_t col, int& kx, int& ky) {
    uint8_t k = mk(PT_KING, col == 2);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (bd[y][x] == k) { kx = x; ky = y; return true; }
    return false;
}

bool kingInCheck(uint8_t bd[8][8], uint8_t col) {
    int kx, ky;
    return findKing(bd, col, kx, ky) && isAttacked(bd, kx, ky, col == 1 ? 2 : 1);
}

void genPseudo(int fx, int fy, bool out[8][8]) {
    memset(out, 0, 64 * sizeof(bool));
    uint8_t p = board[fy][fx];
    if (!p) return;
    uint8_t col = colorOf(p), t = ptype(p), opp = col == 1 ? 2 : 1;
    if (t == PT_PAWN) {
        int dir = col == 1 ? -1 : 1, start = col == 1 ? 6 : 1;
        if (inside(fx, fy + dir) && !board[fy + dir][fx]) {
            out[fy + dir][fx] = true;
            if (fy == start && !board[fy + 2 * dir][fx]) out[fy + 2 * dir][fx] = true;
        }
        for (int s = -1; s <= 1; s += 2) {
            int nx = fx + s, ny = fy + dir;
            if (!inside(nx, ny)) continue;
            if (board[ny][nx] && colorOf(board[ny][nx]) == opp) out[ny][nx] = true;
            if (epX == nx && epY == ny) out[ny][nx] = true;
        }
    } else if (t == PT_KNIGHT) {
        static const int KX[8] = { 1, 2, 2, 1, -1, -2, -2, -1 }, KY[8] = { -2, -1, 1, 2, 2, 1, -1, -2 };
        for (int k = 0; k < 8; k++) {
            int nx = fx + KX[k], ny = fy + KY[k];
            if (inside(nx, ny) && colorOf(board[ny][nx]) != col) out[ny][nx] = true;
        }
    } else if (t == PT_KING) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = fx + dx, ny = fy + dy;
                if ((dx || dy) && inside(nx, ny) && colorOf(board[ny][nx]) != col) out[ny][nx] = true;
            }
        int hr = col == 1 ? 7 : 0;
        bool rk = col == 1 ? cWK : cBK, rq = col == 1 ? cWQ : cBQ;
        if (fx == 4 && fy == hr && !isAttacked(board, 4, hr, opp)) {
            if (rk && !board[hr][5] && !board[hr][6] && !isAttacked(board, 5, hr, opp) && !isAttacked(board, 6, hr, opp))
                out[hr][6] = true;
            if (rq && !board[hr][1] && !board[hr][2] && !board[hr][3] && !isAttacked(board, 3, hr, opp) && !isAttacked(board, 2, hr, opp))
                out[hr][2] = true;
        }
    } else {
        static const int D[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
        int from = (t == PT_ROOK || t == PT_QUEEN) ? 0 : 4, to = (t == PT_BISHOP || t == PT_QUEEN) ? 8 : 4;
        for (int d = from; d < to; d++)
            for (int nx = fx + D[d][0], ny = fy + D[d][1]; inside(nx, ny); nx += D[d][0], ny += D[d][1]) {
                if (!board[ny][nx]) { out[ny][nx] = true; continue; }
                if (colorOf(board[ny][nx]) != col) out[ny][nx] = true;
                break;
            }
    }
}

bool movesIntoCheck(int fx, int fy, int tx, int ty, uint8_t col) {
    uint8_t tmp[8][8];
    memcpy(tmp, board, sizeof tmp);
    uint8_t p = tmp[fy][fx];
    if (ptype(p) == PT_PAWN && tx != fx && !tmp[ty][tx]) tmp[fy][tx] = 0;          // en passant
    if (ptype(p) == PT_KING && abs(tx - fx) == 2) {                                   // Rochade
        if (tx > fx) { tmp[ty][tx - 1] = tmp[fy][7]; tmp[fy][7] = 0; }
        else { tmp[ty][tx + 1] = tmp[fy][0]; tmp[fy][0] = 0; }
    }
    tmp[ty][tx] = p;
    tmp[fy][fx] = 0;
    return kingInCheck(tmp, col);
}

int legalFrom(int fx, int fy, bool mask[8][8]) {
    bool ps[8][8];
    genPseudo(fx, fy, ps);
    uint8_t col = colorOf(board[fy][fx]);
    int n = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            mask[y][x] = ps[y][x] && !movesIntoCheck(fx, fy, x, y, col);
            if (mask[y][x]) n++;
        }
    return n;
}

bool hasAnyLegal(uint8_t col) {
    bool m[8][8];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (board[y][x] && colorOf(board[y][x]) == col && legalFrom(x, y, m)) return true;
    return false;
}

void updateCastle(int fx, int fy, int tx, int ty) {
    if (fx == 4 && fy == 7) cWK = cWQ = false;
    if (fx == 4 && fy == 0) cBK = cBQ = false;
    if ((fx == 0 && fy == 7) || (tx == 0 && ty == 7)) cWQ = false;
    if ((fx == 7 && fy == 7) || (tx == 7 && ty == 7)) cWK = false;
    if ((fx == 0 && fy == 0) || (tx == 0 && ty == 0)) cBQ = false;
    if ((fx == 7 && fy == 0) || (tx == 7 && ty == 0)) cBK = false;
}

void finalizeTurn() {
    currentPlayer = currentPlayer == 1 ? 2 : 1;
    bool chk = kingInCheck(board, currentPlayer);
    inCheck = chk ? currentPlayer : 0;
    if (!hasAnyLegal(currentPlayer)) {
        status = chk ? (currentPlayer == 1 ? 2 : 1) : 3;
        if (chk) { hw::beep(1500, 220); hw::sleepMs(240); hw::beep(1900, 260); }
        else hw::beep(400, 350);
    } else {
        status = 0;
        if (chk) hw::beep(1300, 120);
    }
    selX = -1;
    phase = 0;
}

void confirmPromo() {
    static const uint8_t order[4] = { PT_QUEEN, PT_ROOK, PT_BISHOP, PT_KNIGHT };
    board[promoY][promoX] = mk(order[promoSel], board[promoY][promoX] & BLACK);
    hw::beep(1100, 80);
    finalizeTurn();
}

void executeMove(int fx, int fy, int tx, int ty) {
    uint8_t p = board[fy][fx], col = colorOf(p);
    bool pawn = ptype(p) == PT_PAWN, cap = board[ty][tx] != 0;
    if (pawn && tx != fx && !board[ty][tx]) { board[fy][tx] = 0; cap = true; }
    if (ptype(p) == PT_KING && abs(tx - fx) == 2) {
        if (tx > fx) { board[ty][tx - 1] = board[fy][7]; board[fy][7] = 0; }
        else { board[ty][tx + 1] = board[fy][0]; board[fy][0] = 0; }
        hw::beep(700, 50);
    }
    int8_t nex = -1, ney = -1;
    if (pawn && abs(ty - fy) == 2) { nex = (int8_t)tx; ney = (int8_t)((fy + ty) / 2); }
    board[ty][tx] = p;
    board[fy][fx] = 0;
    updateCastle(fx, fy, tx, ty);
    epX = nex; epY = ney;
    lastFX = (int8_t)fx; lastFY = (int8_t)fy; lastTX = (int8_t)tx; lastTY = (int8_t)ty;
    hw::beep(cap ? 950 : 680, 60);
    if (pawn && (col == 1 ? ty == 0 : ty == 7)) {
        phase = 1; promoX = (int8_t)tx; promoY = (int8_t)ty; promoSel = 0; selX = -1;
        hw::beep(1200, 90);
    } else finalizeTurn();
}

void initMatch() {
    static const uint8_t back[8] = { PT_ROOK, PT_KNIGHT, PT_BISHOP, PT_QUEEN, PT_KING, PT_BISHOP, PT_KNIGHT, PT_ROOK };
    memset(board, 0, sizeof board);
    for (int x = 0; x < 8; x++) {
        board[0][x] = mk(back[x], true);
        board[1][x] = mk(PT_PAWN, true);
        board[6][x] = mk(PT_PAWN, false);
        board[7][x] = mk(back[x], false);
    }
    currentPlayer = 1; status = inCheck = phase = promoSel = 0;
    curX = 4; curY = 6; selX = selY = promoX = promoY = epX = epY = -1;
    lastFX = lastFY = lastTX = lastTY = -1;
    cWK = cWQ = cBK = cBQ = true;
}

void handleInput(uint8_t e, uint8_t player) {
    if (status) return;
    if (phase == 1) {
        if ((e & hw::K_LEFT) && promoSel > 0) { promoSel--; hw::beep(500, 15); }
        if ((e & hw::K_RIGHT) && promoSel < 3) { promoSel++; hw::beep(500, 15); }
        if (e & hw::K_C) confirmPromo();
        return;
    }
    int s = player == 1 ? 1 : -1;               // Schwarz sieht das Brett gedreht
    int dx = 0, dy = 0;
    if (e & hw::K_UP) dy -= s;
    if (e & hw::K_DOWN) dy += s;
    if (e & hw::K_LEFT) dx -= s;
    if (e & hw::K_RIGHT) dx += s;
    if (dx || dy) {
        int nx = curX + dx, ny = curY + dy;
        if (inside(nx, ny)) { curX = (int8_t)nx; curY = (int8_t)ny; hw::beep(500, 12); }
    }
    if (e & hw::K_C) {
        uint8_t cp = board[curY][curX];
        if (selX < 0) {
            if (cp && colorOf(cp) == player) { selX = curX; selY = curY; hw::beep(820, 30); }
            else hw::beep(250, 60);
        } else if (curX == selX && curY == selY) {
            selX = -1;
            hw::beep(420, 25);
        } else {
            bool m[8][8];
            legalFrom(selX, selY, m);
            if (m[curY][curX]) executeMove(selX, selY, curX, curY);
            else if (cp && colorOf(cp) == player) { selX = curX; selY = curY; hw::beep(820, 30); }
            else hw::beep(250, 60);
        }
    }
}

// ---- Uebertragung ----
int pack(uint8_t* out) {
    uint8_t* p = out;
    auto u = [](int8_t v) { return (uint8_t)(v < 0 ? 255 : v); };
    *p++ = match; *p++ = currentPlayer; *p++ = status; *p++ = inCheck; *p++ = phase; *p++ = promoSel;
    *p++ = u(curX); *p++ = u(curY); *p++ = u(selX); *p++ = u(selY); *p++ = u(promoX); *p++ = u(promoY);
    *p++ = u(epX); *p++ = u(epY);
    *p++ = (uint8_t)((cWK ? 1 : 0) | (cWQ ? 2 : 0) | (cBK ? 4 : 0) | (cBQ ? 8 : 0));
    *p++ = u(lastFX); *p++ = u(lastFY); *p++ = u(lastTX); *p++ = u(lastTY);
    for (int i = 0; i < 64; i += 2)
        *p++ = (uint8_t)((board[i / 8][i % 8] & 15) | ((board[(i + 1) / 8][(i + 1) % 8] & 15) << 4));
    return (int)(p - out);
}
constexpr int PKT = 19 + 32;

void unpack(const uint8_t* p) {
    auto s = [](uint8_t v) { return (int8_t)(v == 255 ? -1 : v); };
    uint8_t prevStatus = status, prevCheck = inCheck, prevPlayer = currentPlayer;
    p++;
    currentPlayer = *p++; status = *p++; inCheck = *p++; phase = *p++; promoSel = *p++;
    curX = s(*p++); curY = s(*p++); selX = s(*p++); selY = s(*p++); promoX = s(*p++); promoY = s(*p++);
    epX = s(*p++); epY = s(*p++);
    uint8_t cr = *p++;
    cWK = cr & 1; cWQ = cr & 2; cBK = cr & 4; cBQ = cr & 8;
    lastFX = s(*p++); lastFY = s(*p++); lastTX = s(*p++); lastTY = s(*p++);
    for (int i = 0; i < 64; i += 2, p++) {
        board[i / 8][i % 8] = *p & 15;
        board[(i + 1) / 8][(i + 1) % 8] = *p >> 4;
    }
    if (!prevStatus && status) hw::beep(status == 3 ? 400 : 120, 350);
    else if (!prevCheck && inCheck) hw::beep(1300, 100);
    else if (prevPlayer != currentPlayer) hw::beep(680, 50);
}

// ---- Zeichnen ----
bool flipped() { return !play::host(); }

void cellXY(int bx, int by, int& x, int& y) {
    x = (flipped() ? 7 - bx : bx) * 8;
    y = (flipped() ? 7 - by : by) * 8;
}

void drawGlyph(int x, int y, uint8_t type, bool black) {
    const uint8_t* g = GLYPH[type];
    // Hof: Silhouette um einen Pixel erweitert freiraeumen (hebt sie vom Punktraster ab)
    for (int j = -1; j <= 7; j++)
        for (int i = -1; i <= 7; i++) {
            bool near = false;
            for (int dj = -1; dj <= 1 && !near; dj++)
                for (int di = -1; di <= 1 && !near; di++) {
                    int jj = j + dj, ii = i + di;
                    if (jj >= 0 && jj < 7 && ii >= 0 && ii < 7 && (g[jj] >> (6 - ii) & 1)) near = true;
                }
            if (near) gfx::pixel(x + i, y + j, gfx::OFF);
        }
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 7; i++) {
            if (!(g[j] >> (6 - i) & 1)) continue;
            if (black) {                    // nur Umriss: innere Pixel frei lassen
                auto in = [&](int ii, int jj) { return ii >= 0 && ii < 7 && jj >= 0 && jj < 7 && (g[jj] >> (6 - ii) & 1); };
                if (in(i - 1, j) && in(i + 1, j) && in(i, j - 1) && in(i, j + 1)) continue;
            }
            gfx::pixel(x + i, y + j, gfx::ON);
        }
}

void squareName(char* b, int x, int y) { b[0] = (char)('a' + x); b[1] = (char)('8' - y); b[2] = 0; }

void drawPanel(bool amActive) {
    gfx::text(PX, 0, "You:");
    drawGlyph(PX + 26, 0, PT_KING, !play::host());
    if (status) {
        gfx::text(PX, 14, status == 3 ? "Stalemate" : "Checkmate");
        int me = play::host() ? 1 : 2;
        gfx::text(PX, 24, status == 3 ? "Draw" : status == me ? "You win!" : "You lose");
        if ((hw::ms() / 400) & 1) gfx::text(PX, 50, "PUSH!");
        return;
    }
    if (amActive) {
        gfx::fillRect(PX - 1, 11, 62, 10, gfx::ON);
        gfx::text(PX, 12, "Your move", gfx::OFF);
    } else gfx::text(PX, 12, "Waiting");
    if (inCheck && (hw::ms() / 300) & 1) gfx::text(PX, 23, "CHECK!");
    if (phase == 1) {
        if (!amActive) { gfx::text(PX, 36, "promoting"); gfx::text(PX, 44, "..."); return; }
        gfx::text(PX, 34, "Promote:");
        static const uint8_t order[4] = { PT_QUEEN, PT_ROOK, PT_BISHOP, PT_KNIGHT };
        for (int i = 0; i < 4; i++) {
            int x = PX + 1 + i * 15;
            drawGlyph(x, 45, order[i], !play::host());
            if (i == promoSel) gfx::rect(x - 2, 43, 11, 11, gfx::ON);
        }
        return;
    }
    if (lastFX >= 0) {
        char a[3], b[3], l[8];
        squareName(a, lastFX, lastFY);
        squareName(b, lastTX, lastTY);
        l[0] = a[0]; l[1] = a[1]; l[2] = '-'; l[3] = b[0]; l[4] = b[1]; l[5] = 0;
        gfx::text(PX, 36, "Last:");
        gfx::text(PX, 45, l);
    }
}

void draw() {
    gfx::clear();
    bool amActive = (play::host() ? 1 : 2) == currentPlayer;
    bool blink = (hw::ms() / 250) & 1;
    for (int by = 0; by < 8; by++)
        for (int bx = 0; bx < 8; bx++) {
            int x, y;
            cellXY(bx, by, x, y);
            if ((bx + by) & 1) gfx::patternRect(x, y, 8, 8, 2);
        }
    for (int by = 0; by < 8; by++)
        for (int bx = 0; bx < 8; bx++) {
            uint8_t p = board[by][bx];
            if (!p) continue;
            int x, y;
            cellXY(bx, by, x, y);
            drawGlyph(x, y + 1, ptype(p), p & BLACK);
        }
    if (!status && inCheck && blink) {
        int kx, ky, x, y;
        if (findKing(board, currentPlayer, kx, ky)) { cellXY(kx, ky, x, y); gfx::rect(x, y, 8, 8, gfx::ON); }
    }
    if (amActive && !status) {
        if (selX >= 0) {
            int x, y;
            cellXY(selX, selY, x, y);
            gfx::rect(x, y, 8, 8, gfx::ON);
            bool m[8][8];
            legalFrom(selX, selY, m);
            for (int by = 0; by < 8; by++)
                for (int bx = 0; bx < 8; bx++) {
                    if (!m[by][bx]) continue;
                    cellXY(bx, by, x, y);
                    if (board[by][bx] || (bx == epX && by == epY)) {   // Schlagzug: Ecken
                        for (int c = 0; c < 4; c++) {
                            int cx = x + (c & 1) * 7, cy = y + (c >> 1) * 7;
                            gfx::pixel(cx, cy, gfx::ON);
                        }
                    } else gfx::fillRect(x + 3, y + 3, 2, 2, gfx::INV);
                }
        }
        if (phase == 0 && blink) {
            int x, y;
            cellXY(curX, curY, x, y);
            gfx::rect(x, y, 8, 8, gfx::INV);
        }
    }
    gfx::vline(65, 0, 64, gfx::ON);
    drawPanel(amActive);
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
                if (hw::ms() - endAt > 1000 && ((k1 | k2) & hw::K_C)) { initMatch(); match++; hw::beep(900, 60); }
            } else {
                handleInput(currentPlayer == 1 ? k1 : k2, currentPlayer);
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

}  // namespace chess
