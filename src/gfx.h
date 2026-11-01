// SH1106-OLED 128x64 an SPI0 (SCK GP18, MOSI GP19, DC GP8, RST GP9, CS GP10)
// mit 1-Bit-Bildpuffer und einfachen Zeichenfunktionen.
//
// Zwei Lagen:
//   LAND  quer,     128 x 64  (native Lage, wie MicroPython)
//   PORT  hochkant,  64 x 128 (wie U8G2_R3 in PicoPac)
// Alle Koordinaten sind logisch, die Umrechnung macht pixel().
#pragma once
#include <stdint.h>

namespace gfx {

enum Orient : uint8_t { LAND = 0, PORT = 1 };

// Farben: 0 = aus, 1 = an, 2 = invertieren
constexpr int OFF = 0, ON = 1, INV = 2;

void init();
void setOrient(Orient o);
Orient orient();
int W();
int H();

void clear();
void flush();                      // Bildpuffer zum Display

void pixel(int x, int y, int c);
bool get(int x, int y);
void hline(int x, int y, int w, int c);
void vline(int x, int y, int h, int c);
void line(int x0, int y0, int x1, int y1, int c);
void rect(int x, int y, int w, int h, int c);
void fillRect(int x, int y, int w, int h, int c);
// Muster: 1 = Schachbrett (50 %), 2 = Punkte (25 %), bezogen auf absolute Koordinaten
void patternRect(int x, int y, int w, int h, int pattern);
void circle(int cx, int cy, int r, int c);
void fillCircle(int cx, int cy, int r, int c);
void triangle(int x0, int y0, int x1, int y1, int x2, int y2, int c);
void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int c);
// Bitmap: Zeilen von oben nach unten, je Zeile ein Byte, Bit (w-1-x) = Spalte x.
void bitmap(int x, int y, const uint8_t* rows, int w, int h, int c);

// Text in 5x7 (Zelle 6x8). UTF-8 mit deutschen Umlauten, ss und Grad.
// bg < 0: transparent. Liefert die Breite in Pixeln.
int text(int x, int y, const char* s, int c = ON, int scale = 1, int bg = -1);
int textWidth(const char* s, int scale = 1);
void textCenter(int cx, int y, const char* s, int c = ON, int scale = 1, int bg = -1);

// Kasten mit Rahmen in der Bildmitte fuer Meldungen (bis zu 3 Zeilen, die
// erste doppelt so gross, wenn big).
void messageBox(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, bool big = false);

}  // namespace gfx
