// Vier gewinnt - quer. Brett 7x6 in der Mitte, P1 gefuellte Steine, P2 Ringe.
// Links/Rechts waehlt die Spalte, Mitte wirft ein. Die Gewinnreihe blinkt.

#include "play.h"

namespace c4 {
namespace {

constexpr int NX = 7, NY = 6, CS = 9, OX = 32, OY = 10, R = 3;

uint8_t board[NX][NY];            // 0 leer, 1 P1, 2 P2; Reihe 0 oben
uint8_t curCol, currentPlayer, status, lastCol, lastRow, match;   // status: 0 laeuft, 1/2 Sieger, 3 voll
int winX[4], winY[4];
bool haveWin;

void initMatch() {
    for (auto& c : board) for (uint8_t& v : c) v = 0;
    curCol = 3;
    currentPlayer = 1;
    status = 0;
    lastCol = lastRow = 0xFF;
    haveWin = false;
}

// Sucht irgendeine Viererreihe (beide Seiten rechnen sie selbst aus dem Brett)
bool findWin() {
    static const int D[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
    for (int c = 0; c < NX; c++)
        for (int r = 0; r < NY; r++) {
            uint8_t p = board[c][r];
            if (!p) continue;
            for (auto& d : D) {
                int k = 1;
                while (k < 4) {
                    int x = c + d[0] * k, y = r + d[1] * k;
                    if (x < 0 || x >= NX || y < 0 || y >= NY || board[x][y] != p) break;
                    k++;
                }
                if (k == 4) {
                    for (int i = 0; i < 4; i++) { winX[i] = c + d[0] * i; winY[i] = r + d[1] * i; }
                    return true;
                }
            }
        }
    return false;
}

bool full() {
    for (int c = 0; c < NX; c++) if (!board[c][0]) return false;
    return true;
}

void drop(int col) {
    if (board[col][0]) return;
    int row = NY - 1;
    while (board[col][row]) row--;
    board[col][row] = currentPlayer;
    lastCol = (uint8_t)col; lastRow = (uint8_t)row;
    hw::beep(700 + currentPlayer * 200, 60);
    if (findWin()) { status = currentPlayer; haveWin = true; hw::beep(1500, 200); }
    else if (full()) { status = 3; hw::beep(400, 300); }
    else currentPlayer = currentPlayer == 1 ? 2 : 1;
    // Cursor auf eine freie Spalte stellen
    if (!status && board[curCol][0])
        for (int k = 1; k < NX; k++)
            if (!board[(curCol + k) % NX][0]) { curCol = (uint8_t)((curCol + k) % NX); break; }
}

void moveCursor(int dir) {
    int c = curCol;
    for (int k = 0; k < NX; k++) {
        c = (c + dir + NX) % NX;
        if (!board[c][0]) { curCol = (uint8_t)c; hw::beep(500, 15); return; }
    }
}

void piece(int cx, int cy, uint8_t p) {
    if (p == 1) gfx::fillCircle(cx, cy, R, gfx::ON);
    else gfx::circle(cx, cy, R, gfx::ON);
}

// Legende: Stein und Spieler (wer dran ist, zeigt der Einwurfpfeil)
void side(int x, uint8_t p) {
    piece(x + 5, 16, p);
    gfx::text(x + 11, 13, p == 1 ? "P1" : "P2");
}

void draw() {
    gfx::clear();
    bool blink = (hw::ms() / 250) & 1;
    // Rahmen und Loecher
    gfx::vline(OX - 1, OY, NY * CS, gfx::ON);
    gfx::vline(OX + NX * CS, OY, NY * CS, gfx::ON);
    for (int c = 0; c < NX; c++)
        for (int r = 0; r < NY; r++) {
            int cx = OX + c * CS + CS / 2, cy = OY + r * CS + CS / 2;
            uint8_t v = board[c][r];
            bool winCell = false;
            if (haveWin) for (int i = 0; i < 4; i++) if (winX[i] == c && winY[i] == r) winCell = true;
            if (!v) gfx::pixel(cx, cy, gfx::ON);
            else if (!(winCell && blink)) piece(cx, cy, v);
            if (v && c == lastCol && r == lastRow && !status && blink) gfx::pixel(cx, cy, gfx::INV);
        }
    side(0, 1);
    side(98, 2);
    if (!status) {
        // Einwurfpfeil ueber der Spalte - nur, wenn ich dran bin
        if ((currentPlayer == 1) == play::host()) {
            int cx = OX + curCol * CS + CS / 2;
            gfx::fillTriangle(cx - 3, 2, cx + 3, 2, cx, 7, gfx::ON);
        }
    } else {
        int me = play::host() ? 1 : 2;
        const char* t = status == 3 ? "DRAW - PUSH!" : status == me ? "YOU WIN! - PUSH!" : "YOU LOSE - PUSH!";
        gfx::fillRect(0, 0, 128, 9, gfx::ON);
        gfx::textCenter(64, 1, t, gfx::OFF);
    }
}

int pack(uint8_t* out) {
    uint8_t* p = out;
    *p++ = match; *p++ = curCol; *p++ = currentPlayer; *p++ = status; *p++ = lastCol; *p++ = lastRow;
    for (int i = 0; i < 11; i++) p[i] = 0;
    for (int i = 0; i < NX * NY; i++) p[i / 4] |= (uint8_t)(board[i % NX][i / NX] << ((i % 4) * 2));
    return 6 + 11;
}

void unpack(const uint8_t* p) {
    uint8_t prev = status, prevPlayer = currentPlayer;
    curCol = p[1]; currentPlayer = p[2]; status = p[3]; lastCol = p[4]; lastRow = p[5];
    p += 6;
    for (int i = 0; i < NX * NY; i++) board[i % NX][i / NX] = (p[i / 4] >> ((i % 4) * 2)) & 3;
    haveWin = status == 1 || status == 2 ? findWin() : false;
    if (!prev && status) hw::beep(status == 3 ? 400 : 1500, 250);
    else if (prevPlayer != currentPlayer) hw::beep(800, 50);
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
                uint8_t k = currentPlayer == 1 ? k1 : k2;
                if (k & hw::K_LEFT) moveCursor(-1);
                if (k & hw::K_RIGHT) moveCursor(1);
                if (k & (hw::K_C | hw::K_DOWN)) { drop(curCol); if (status) endAt = hw::ms(); }
            }
        }
        uint8_t buf[17];
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
        if (play::receive(b, &n) && n >= 17) unpack(b);
        draw();
        play::present();
    }
}

}  // namespace c4
