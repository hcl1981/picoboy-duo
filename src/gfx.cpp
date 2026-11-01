#include "gfx.h"
#include "font5x7.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdlib.h>
#include <initializer_list>
#include <string.h>

namespace gfx {
namespace {

constexpr uint PIN_SCK = 18, PIN_MOSI = 19, PIN_DC = 8, PIN_RST = 9, PIN_CS = 10;
constexpr int PW = 128, PH = 64;              // physikalisch (quer)

uint8_t fb[PW * PH / 8];                      // Seite (8 Zeilen) * 128 Spalten, Bit 0 = oben
Orient cur = LAND;

void cmd(const uint8_t* c, size_t n) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi0, c, n);
    gpio_put(PIN_CS, 1);
}

inline void setPhys(int col, int row, int c) {
    uint8_t& b = fb[(row >> 3) * PW + col];
    uint8_t m = (uint8_t)(1u << (row & 7));
    if (c == ON) b |= m;
    else if (c == OFF) b &= (uint8_t)~m;
    else b ^= m;
}

// UTF-8 -> Codepage 437 (nur was wir brauchen)
uint8_t nextChar(const char*& s) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x80) return c;
    if (c == 0xC2 || c == 0xC3) {
        uint8_t d = (uint8_t)*s;
        if (!d) return '?';
        s++;
        if (c == 0xC2) return d == 0xB0 ? 0xF8 : '?';          // Grad
        switch (d) {
            case 0xA4: return 0x84;   // ae
            case 0xB6: return 0x94;   // oe
            case 0xBC: return 0x81;   // ue
            case 0x84: return 0x8E;   // Ae
            case 0x96: return 0x99;   // Oe
            case 0x9C: return 0x9A;   // Ue
            case 0x9F: return 0xE1;   // ss
        }
        return '?';
    }
    while ((*s & 0xC0) == 0x80) s++;          // unbekannte Folge ueberspringen
    return '?';
}

}  // namespace

void init() {
    spi_init(spi0, 8 * 1000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    for (uint p : { PIN_DC, PIN_RST, PIN_CS }) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_OUT);
        gpio_put(p, 1);
    }
    gpio_put(PIN_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_RST, 1);
    sleep_ms(20);
    // Dieselbe Folge wie U8g2 (SH1106 "noname"), mit der PicoPac auf genau
    // diesem Geraet laeuft; Kontrast 31 wie dort (spart Strom bei Knopfzelle).
    static const uint8_t initSeq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 31, 0xD9, 0xF1, 0xDB, 0x40, 0x2E, 0xA4, 0xA6,
    };
    cmd(initSeq, sizeof initSeq);
    clear();
    flush();
    static const uint8_t on[] = { 0xAF };
    cmd(on, 1);
}

void setOrient(Orient o) { cur = o; }
Orient orient() { return cur; }
int W() { return cur == LAND ? PW : PH; }
int H() { return cur == LAND ? PH : PW; }

void clear() { memset(fb, 0, sizeof fb); }

void flush() {
    for (int page = 0; page < PH / 8; page++) {
        // SH1106: 132 Spalten RAM, sichtbar ab Spalte 2
        const uint8_t addr[] = { (uint8_t)(0xB0 | page), 0x02, 0x10 };
        cmd(addr, sizeof addr);
        gpio_put(PIN_DC, 1);
        gpio_put(PIN_CS, 0);
        spi_write_blocking(spi0, fb + page * PW, PW);
        gpio_put(PIN_CS, 1);
    }
}

void pixel(int x, int y, int c) {
    if (cur == LAND) {
        if ((unsigned)x >= PW || (unsigned)y >= PH) return;
        setPhys(x, y, c);
    } else {
        if ((unsigned)x >= PH || (unsigned)y >= PW) return;
        setPhys(y, PH - 1 - x, c);
    }
}

bool get(int x, int y) {
    int col, row;
    if (cur == LAND) {
        if ((unsigned)x >= PW || (unsigned)y >= PH) return false;
        col = x; row = y;
    } else {
        if ((unsigned)x >= PH || (unsigned)y >= PW) return false;
        col = y; row = PH - 1 - x;
    }
    return fb[(row >> 3) * PW + col] & (1u << (row & 7));
}

void hline(int x, int y, int w, int c) { for (int i = 0; i < w; i++) pixel(x + i, y, c); }
void vline(int x, int y, int h, int c) { for (int i = 0; i < h; i++) pixel(x, y + i, c); }

void line(int x0, int y0, int x1, int y1, int c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void rect(int x, int y, int w, int h, int c) {
    if (w <= 0 || h <= 0) return;
    hline(x, y, w, c);
    if (h > 1) hline(x, y + h - 1, w, c);
    if (h > 2) { vline(x, y + 1, h - 2, c); if (w > 1) vline(x + w - 1, y + 1, h - 2, c); }
}

void fillRect(int x, int y, int w, int h, int c) {
    for (int j = 0; j < h; j++) hline(x, y + j, w, c);
}

void patternRect(int x, int y, int w, int h, int pattern) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            bool on = pattern == 1 ? ((i + j) & 1) == 0 : ((i & 1) == 0 && (j & 1) == 0);
            pixel(i, j, on ? ON : OFF);
        }
}

void circle(int cx, int cy, int r, int c) {
    if (r <= 0) { pixel(cx, cy, c); return; }
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        // Bei INV duerfen die Achspunkte nicht doppelt kippen
        pixel(cx + x, cy + y, c); pixel(cx - x, cy - y, c);
        if (y) { pixel(cx + x, cy - y, c); pixel(cx - x, cy + y, c); }
        if (x != y) {
            pixel(cx + y, cy + x, c); pixel(cx - y, cy - x, c);
            if (y) { pixel(cx - y, cy + x, c); pixel(cx + y, cy - x, c); }
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void fillCircle(int cx, int cy, int r, int c) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r + r) pixel(cx + dx, cy + dy, c);
}

void triangle(int x0, int y0, int x1, int y1, int x2, int y2, int c) {
    line(x0, y0, x1, y1, c);
    line(x1, y1, x2, y2, c);
    line(x2, y2, x0, y0, c);
}

void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int c) {
    int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = minY; y <= maxY; y++)
        for (int x = minX; x <= maxX; x++) {
            int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) pixel(x, y, c);
        }
}

void bitmap(int x, int y, const uint8_t* rows, int w, int h, int c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (rows[j] & (1u << (w - 1 - i))) pixel(x + i, y + j, c);
}

int text(int x, int y, const char* s, int c, int scale, int bg) {
    int x0 = x;
    while (*s) {
        uint8_t ch = nextChar(s);
        const uint8_t* g = font5x7 + ch * 5;
        if (bg >= 0) fillRect(x, y, 6 * scale, 8 * scale, bg);
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 8; row++)
                if (g[col] & (1u << row)) {
                    if (scale == 1) pixel(x + col, y + row, c);
                    else fillRect(x + col * scale, y + row * scale, scale, scale, c);
                }
        x += 6 * scale;
    }
    return x - x0;
}

int textWidth(const char* s, int scale) {
    int n = 0;
    while (*s) { nextChar(s); n++; }
    return n ? (n * 6 - 1) * scale : 0;
}

void textCenter(int cx, int y, const char* s, int c, int scale, int bg) {
    text(cx - textWidth(s, scale) / 2, y, s, c, scale, bg);
}

void messageBox(const char* l1, const char* l2, const char* l3, bool big) {
    const char* lines[3] = { l1, l2, l3 };
    int n = 0, w = 0, h = 0;
    for (int i = 0; i < 3; i++) {
        if (!lines[i]) break;
        int sc = (i == 0 && big) ? 2 : 1;
        int tw = textWidth(lines[i], sc);
        if (tw > w) w = tw;
        h += 8 * sc + 2;
        n++;
    }
    w += 8; h += 4;
    if (w > W()) w = W();
    int bx = (W() - w) / 2, by = (H() - h) / 2;
    fillRect(bx, by, w, h, OFF);
    rect(bx, by, w, h, ON);
    int y = by + 3;
    for (int i = 0; i < n; i++) {
        int sc = (i == 0 && big) ? 2 : 1;
        textCenter(W() / 2, y, lines[i], ON, sc);
        y += 8 * sc + 2;
    }
}

}  // namespace gfx
