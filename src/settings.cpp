// Siehe settings.h. Aufbau wie MULTI und das Startmenue (starscreen.h):
// Links/Rechts waehlt die Einstellung, Mitte (oder Hoch/Runter) aendert sie.
// Gespeichert wird beim Verlassen, damit der Flash nicht bei jedem Druck
// geloescht wird.
#include "hw.h"
#include "gfx.h"
#include "play.h"
#include "settings.h"
#include "starscreen.h"
#include "picopac/U8g2lib.h"      // zuletzt: Arduino-Makros (UP, LEFT ...)

extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2;   // picopac.cpp

namespace {

// EEPROM: 0..9 Highscores der PicoPac-Spiele, ab 16 die Einstellungen
constexpr int EE_MAGIC = 16, EE_SOUND = 17, EE_BRIGHT = 18;
constexpr uint8_t MAGIC = 0x5E;

// Kontraststufen des SH1106; Stufe 3 = 31 wie PicoPac (schont die Knopfzelle)
constexpr uint8_t LEVELS[] = { 4, 12, 31, 90, 255 };
constexpr int NUM_LEVELS = sizeof LEVELS;
constexpr int DEFAULT_LEVEL = 2;

bool sound = true;
int level = DEFAULT_LEVEL;

void apply() {
    hw::setSound(sound);
    u8g2_SetContrast(u8g2.getU8g2(), LEVELS[level]);
}

void save() {
    EEPROM.write(EE_MAGIC, MAGIC);
    EEPROM.write(EE_SOUND, sound ? 1 : 0);
    EEPROM.write(EE_BRIGHT, (uint8_t)level);
    EEPROM.commit();                  // schreibt nur, wenn sich etwas geaendert hat
}

enum { ITEM_SOUND, ITEM_BRIGHT, ITEM_SCORES, ITEM_INFO, ITEM_BACK, NUM_ITEMS };
const char* const NAMES[] = { "Sound", "Display", "Scores", "Info", "BACK" };

// Lizenzhinweise (Schrift 5x7: hoechstens 12 Zeichen je Zeile)
const char* const INFO[] = {
    "PicoBoy Duo",
    "",
    "The firmware",
    "as a whole:",
    "GNU GPL v2+",
    "",
    "Popbit is",
    "Jump'n'Bump",
    "(C) 1998",
    "Brainchild",
    "Design,",
    "GPL v2+",
    "",
    "U8g2 by",
    "olikraus,",
    "BSD 2-Clause",
    "",
    "5x7 font:",
    "(C) 2012",
    "Adafruit",
    "Industries,",
    "BSD",
    "",
    "PicoPac and",
    "Duo games:",
    "hcl1981",
    "(Duo: MIT)",
};
constexpr int INFO_LINES = sizeof INFO / sizeof INFO[0];
constexpr int INFO_TOP = 22, INFO_STEP = 8, INFO_ROWS = (128 - INFO_TOP) / INFO_STEP;

void waitRelease() { while (hw::held() & hw::K_C) hw::sleepMs(5); }

// Lizenzseite: Hoch/Runter blaettert, jede andere Taste zurueck
void infoPage() {
    int top = 0;
    play::Edges e;
    e.sync(play::localPad());
    for (;;) {
        uint8_t k = e.take(play::localPad());
        if (k & hw::K_UP) top = top > 0 ? top - 1 : 0;
        else if (k & hw::K_DOWN) top = top < INFO_LINES - INFO_ROWS ? top + 1 : top;
        else if (k) { hw::beep(500, 15); return; }
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_t0_11_tr);
        u8g2.setCursor(32 - u8g2.getStrWidth("Info") / 2, 15);
        u8g2.print("Info");
        u8g2.setFont(u8g2_font_5x7_tf);
        for (int i = 0; i < INFO_ROWS && top + i < INFO_LINES; i++)
            u8g2.drawStr(2, INFO_TOP + 6 + i * INFO_STEP, INFO[top + i]);
        // Rollbalken rechts
        int barH = 128 - INFO_TOP;
        int h = barH * INFO_ROWS / INFO_LINES;
        int y = INFO_TOP + (barH - h) * top / (INFO_LINES - INFO_ROWS);
        u8g2.drawBox(63, y, 1, h);
        u8g2.sendBuffer();
        starscreen::ledChase();
        hw::sleepMs(10);
    }
}

// Highscores loeschen - erst nach Rueckfrage (NO vorausgewaehlt)
void clearScores() {
    bool yes = false;
    play::Edges e;
    e.sync(play::localPad());
    for (;;) {
        uint8_t k = e.take(play::localPad());
        if (k & (hw::K_LEFT | hw::K_RIGHT)) { yes = !yes; hw::beep(500, 15); }
        if (k & hw::K_C) break;
        starscreen::begin();
        starscreen::centered("Settings", 15);
        starscreen::centered("CLEAR ALL", 60);
        starscreen::centered("HI-SCORES?", 75);
        starscreen::finish("Sure?", yes ? "YES" : "NO");
        starscreen::ledChase();
        hw::sleepMs(10);
    }
    if (!yes) { hw::beep(500, 15); return; }
    for (int a = 0; a < 10; a++) EEPROM.write(a, 0);   // PICTRIS, SCLANGE, TABMAN, W'n'R, Memor
    EEPROM.commit();
    hw::beep(1200, 80);
    uint32_t until = hw::ms() + 1200;
    while (hw::ms() < until) {
        starscreen::begin();
        starscreen::centered("Settings", 15);
        starscreen::centered("HI-SCORES", 60);
        starscreen::centered("CLEARED", 75);
        starscreen::finish("", nullptr);
        hw::sleepMs(10);
    }
}

void draw(int sel) {
    starscreen::begin();
    starscreen::centered("Settings", 15);
    if (sel == ITEM_SOUND) {
        starscreen::centered("SOUND", 60);
        starscreen::centered(sound ? "ON" : "OFF", 75);
    } else if (sel == ITEM_BRIGHT) {
        starscreen::centered("BRIGHTNESS", 60);
        // fuenf Kaestchen, gefuellt bis zur gewaehlten Stufe
        for (int i = 0; i < NUM_LEVELS; i++)
            starscreen::box(8 + i * 10, 66, 8, 8, i <= level);
    } else if (sel == ITEM_SCORES) {
        starscreen::centered("CLEAR", 60);
        starscreen::centered("HI-SCORES", 75);
    } else if (sel == ITEM_INFO) {
        starscreen::centered("LICENSES", 60);
    }
    starscreen::finish(sel == ITEM_SOUND || sel == ITEM_BRIGHT ? "Change" : "Select", NAMES[sel]);
}

}  // namespace

namespace settings {

uint8_t contrast() { return LEVELS[level]; }

void load() {
    if (EEPROM.read(EE_MAGIC) == MAGIC) {
        sound = EEPROM.read(EE_SOUND) != 0;
        uint8_t l = EEPROM.read(EE_BRIGHT);
        level = l < NUM_LEVELS ? l : DEFAULT_LEVEL;
    }
    apply();
}

void menu() {
    static int sel = 0;
    gfx::setOrient(gfx::PORT);                 // Tasten hochkant wie im Startmenue
    play::Edges e;
    e.sync(play::localPad());
    for (;;) {
        uint8_t k = e.take(play::localPad());
        if (k & hw::K_RIGHT) { sel = (sel + 1) % NUM_ITEMS; hw::beep(500, 15); }
        if (k & hw::K_LEFT) { sel = (sel + NUM_ITEMS - 1) % NUM_ITEMS; hw::beep(500, 15); }
        if (k & (hw::K_C | hw::K_UP | hw::K_DOWN)) {
            if (sel == ITEM_SOUND) {
                sound = !sound;
                apply();
                hw::beep(900, 50);             // nur hoerbar, wenn der Ton jetzt an ist
            } else if (sel == ITEM_BRIGHT) {
                if (k & hw::K_DOWN) level = level > 0 ? level - 1 : 0;
                else if (k & hw::K_UP) level = level < NUM_LEVELS - 1 ? level + 1 : NUM_LEVELS - 1;
                else level = (level + 1) % NUM_LEVELS;
                apply();
                hw::beep(500, 15);
            } else if (sel == ITEM_SCORES && (k & hw::K_C)) {
                hw::beep(900, 50);
                waitRelease();
                clearScores();
                waitRelease();
                e.sync(play::localPad());
            } else if (sel == ITEM_INFO && (k & hw::K_C)) {
                hw::beep(900, 50);
                waitRelease();
                infoPage();
                waitRelease();
                e.sync(play::localPad());
            } else if (k & hw::K_C) {
                save();
                starscreen::ledsOff();
                hw::beep(900, 50);
                return;
            }
        }
        starscreen::ledChase();
        draw(sel);
        hw::sleepMs(10);
    }
}

}  // namespace settings

uint8_t settingsContrast() { return settings::contrast(); }
