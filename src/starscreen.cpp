// Siehe starscreen.h. Gezeichnet wird mit dem U8g2-Objekt der PicoPac-Spiele,
// damit Schrift und Sterne genau wie im Startmenue (Misc/starintro.h) aussehen.
#include <math.h>

#include "hw.h"
#include "starscreen.h"
#include "picopac/U8g2lib.h"      // zuletzt: Arduino-Makros (UP, LEFT ...)

extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2;   // picopac.cpp

namespace {

// Sternenfeld wie StarIntro: Ursprung (32,33), Tempo 0.1..0.2 px, je Bild * 1.1
constexpr int NSTARS = 120;
constexpr float CX = 32, CY = 33;
struct Star { float x, y, vx, vy; };
Star stars[NSTARS];
bool starsReady = false;

void resetStar(Star& s) {
    s.x = CX; s.y = CY;
    float a = hw::rndRange(0, 3599) / 1800.0f * 3.14159265f;
    float v = (float)(hw::rndRange(0, 99) + 100) / 1000.0f;
    s.vx = v * cosf(a);
    s.vy = v * sinf(a);
}

void initStars() {
    for (Star& s : stars) {
        resetStar(s);
        for (int i = hw::rndRange(0, 40); i > 0; i--) {   // schon verteilt starten
            s.x += s.vx; s.y += s.vy; s.vx *= 1.1f; s.vy *= 1.1f;
        }
        if (s.x < 0 || s.x >= 64 || s.y < 0 || s.y >= 128) resetStar(s);
    }
}

}  // namespace

namespace starscreen {

void begin() {
    if (!starsReady) { initStars(); starsReady = true; }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_tr);
}

void centered(const char* s, int y) {
    u8g2.setCursor(32 - u8g2.getStrWidth(s) / 2, y);
    u8g2.print(s);
}

void box(int x, int y, int w, int h, bool filled) {
    if (filled) u8g2.drawBox(x, y, w, h);
    else u8g2.drawFrame(x, y, w, h);
}

void finish(const char* label, const char* item) {
    if (item && (hw::ms() % 500) < 250) {
        centered(label, 108);
        centered("<        >", 123);
        centered(item, 123);
    }
    for (Star& s : stars) {
        u8g2.drawPixel((int)s.x, (int)s.y);
        s.x += s.vx; s.y += s.vy;
        s.vx *= 1.1f; s.vy *= 1.1f;
        if (s.x < 0 || s.x >= 64 || s.y < 0 || s.y >= 128) resetStar(s);
    }
    u8g2.sendBuffer();
}

// Lauflicht rot - gelb - gruen - Pause, Helligkeiten wie im Original
void ledChase() {
    uint32_t t = hw::ms() % 600;
    hw::ledLevel(hw::LED_RED, t < 150 ? 65 : 0);
    hw::ledLevel(hw::LED_YELLOW, t >= 150 && t < 300 ? 15 : 0);
    hw::ledLevel(hw::LED_GREEN, t >= 300 && t < 450 ? 185 : 0);
}

void ledsOff() {
    hw::led(hw::LED_RED, false);
    hw::led(hw::LED_YELLOW, false);
    hw::led(hw::LED_GREEN, false);
}

}  // namespace starscreen
