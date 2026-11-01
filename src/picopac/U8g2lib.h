// Nachbildung der Arduino-Klasse U8G2 (cppsrc/U8g2lib.h) fuer das Pico SDK.
// Die Zeichenfunktionen sind die der echten U8g2-C-Bibliothek; nachgebildet
// ist nur die duenne C++-Huelle samt Print (setCursor/print/println) - mit
// derselben Cursorlogik wie im Original, damit Text an derselben Stelle steht.
#pragma once
#include "Arduino.h"
extern "C" {
#include "u8g2.h"
}

uint8_t settingsContrast();     // settings.cpp

class U8G2 {
public:
    u8g2_t u8g2;
    u8g2_uint_t tx = 0, ty = 0;

    u8g2_t* getU8g2() { return &u8g2; }

    bool begin() {
        u8g2_InitDisplay(&u8g2);
        u8g2_ClearDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        return true;
    }
    // PicoPac stellt immer 31/32 ein (Demo blitzt kurz mit 255): das "normale"
    // Niveau ersetzt die Helligkeit aus den Einstellungen, der Blitz bleibt.
    void setContrast(uint8_t v) { u8g2_SetContrast(&u8g2, v < 64 ? settingsContrast() : v); }
    void setDisplayRotation(const u8g2_cb_t* r) { u8g2_SetDisplayRotation(&u8g2, r); }

    void clearBuffer() { u8g2_ClearBuffer(&u8g2); }
    void sendBuffer() { u8g2_SendBuffer(&u8g2); }
    void clearDisplay() { u8g2_ClearDisplay(&u8g2); }
    void clear() { tx = ty = 0; clearDisplay(); clearBuffer(); }

    void setDrawColor(uint8_t c) { u8g2_SetDrawColor(&u8g2, c); }
    void drawPixel(u8g2_uint_t x, u8g2_uint_t y) { u8g2_DrawPixel(&u8g2, x, y); }
    void drawLine(u8g2_uint_t x0, u8g2_uint_t y0, u8g2_uint_t x1, u8g2_uint_t y1) { u8g2_DrawLine(&u8g2, x0, y0, x1, y1); }
    void drawBox(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h) { u8g2_DrawBox(&u8g2, x, y, w, h); }
    void drawFrame(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h) { u8g2_DrawFrame(&u8g2, x, y, w, h); }
    void drawRFrame(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r) { u8g2_DrawRFrame(&u8g2, x, y, w, h, r); }
    void drawRBox(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r) { u8g2_DrawRBox(&u8g2, x, y, w, h, r); }
    void drawCircle(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t r, uint8_t opt = U8G2_DRAW_ALL) { u8g2_DrawCircle(&u8g2, x, y, r, opt); }
    void drawDisc(u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t r, uint8_t opt = U8G2_DRAW_ALL) { u8g2_DrawDisc(&u8g2, x, y, r, opt); }
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) { u8g2_DrawTriangle(&u8g2, x0, y0, x1, y1, x2, y2); }

    void setFont(const uint8_t* f) { u8g2_SetFont(&u8g2, f); }
    void setFontDirection(uint8_t d) { u8g2_SetFontDirection(&u8g2, d); }
    u8g2_uint_t drawStr(u8g2_uint_t x, u8g2_uint_t y, const char* s) { return u8g2_DrawStr(&u8g2, x, y, s); }
    u8g2_uint_t getStrWidth(const char* s) { return u8g2_GetStrWidth(&u8g2, s); }

    // ---- Print wie in U8g2lib.h: '\n' wird uebersprungen (u8x8_ascii_next),
    //      alles andere als Glyphe gezeichnet, Cursor je nach Schriftrichtung ----
    void setCursor(u8g2_uint_t x, u8g2_uint_t y) { tx = x; ty = y; }
    size_t write(uint8_t v) {
        if (v == 0 || v == '\n') return 1;
        u8g2_uint_t delta = u8g2_DrawGlyph(&u8g2, tx, ty, v);
        switch (u8g2.font_decode.dir) {
            case 0: tx += delta; break;
            case 1: ty += delta; break;
            case 2: tx -= delta; break;
            case 3: ty -= delta; break;
        }
        return 1;
    }
    size_t print(const char* s) { size_t n = 0; while (*s) n += write((uint8_t)*s++); return n; }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return print(b); }
    size_t print(unsigned int v) { char b[16]; snprintf(b, sizeof b, "%u", v); return print(b); }
    size_t print(long v) { char b[24]; snprintf(b, sizeof b, "%ld", v); return print(b); }
    size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof b, "%lu", v); return print(b); }
    template <class T> size_t println(T v) { size_t n = print(v); return n + println(); }
    size_t println() { write('\r'); write('\n'); return 2; }
};

// Der Displaytyp aus PicoPac.ino - SPI0 an GP18/19, Pins wie dort angegeben.
class U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI : public U8G2 {
public:
    U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI(const u8g2_cb_t* rotation, uint8_t cs, uint8_t dc, uint8_t reset);
};
