// Die Einzelspieler-Sammlung PicoPac (Original: picoboy/Arduino/PicoPac) im
// Pico SDK. Entspricht PicoPac.ino: gleiche Einbindung der Spiele, gleiche
// EEPROM-Pruefung beim Start, gleiche Displayeinstellungen. Entfallen sind
// Conway; Eintrag 6 des Startmenues fuehrt zu den Zweispieler-Spielen
// (main.cpp).
//
// Die Spielobjekte sind statisch statt lokal in setup(): einige tragen grosse
// Felder (W'n'R merkt sich 10000 Zuege) - der Stapel des Pico SDK ist klein.
#include "hw.h"

#include "picopac.h"
#include "Arduino.h"
#include "U8g2lib.h"

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R3, /* cs=*/10, /* dc=*/8, /* reset=*/9);

// Aus Misc/helper.h (Matthias Busse 5.2014); die Takt-Funktionen von dort
// braucht es nicht, das Pico SDK bringt set_sys_clock_khz() selbst mit.
void eepromWriteInt(int adr, int wert) {
    byte low, high;
    low = wert & 0xFF;
    high = (wert >> 8) & 0xFF;
    EEPROM.write(adr, low);
    EEPROM.write(adr + 1, high);
    EEPROM.commit();
}

int eepromReadInt(int adr) {
    byte low, high;
    low = EEPROM.read(adr);
    high = EEPROM.read(adr + 1);
    return low + ((high << 8) & 0xFF00);
}

#include "TABman/tabman.h"
#include "SCLange/sclange.h"
#include "PICtris/pictris.h"
#include "Misc/starintro.h"
#include "Demo/demo.h"
#include "Memor/memor.h"
#include "Sensy/sensy.h"
#include "HWTest/hwtest.h"           // nach Demo: benutzt deren xAxis, yAxis, zAxis

namespace picopac {

void begin() {
    EEPROM.begin(256);
    for (int a = 0; a <= 8; a += 2)
        if (eepromReadInt(a) >= 65500) eepromWriteInt(a, 0);
    u8g2.begin();
    u8g2.setContrast(31);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

int menu() {
    static StarIntro intro(u8g2);          // behaelt die Auswahl beim Zurueckkehren
    return intro.starIntro();
}

void run(int id) {
    randomSeed(millis());
    switch (id) {
        case 1: { static PICtris g(u8g2); g.run(); break; }
        case 2: { static SCLange g(u8g2); g.run(); break; }
        case 3: { static TABman g(u8g2); g.run(); break; }
        case 4: { static Sensy g(u8g2); g.run(); break; }
        case 5: { static Memor g(u8g2); g.run(); break; }
        case 7: { static Demo g(u8g2); g.run(); break; }
        // Der Lautsprechertest soll auch bei "Sound off" hoerbar sein. Gespeichert
        // wird das nicht; HWTest laeuft wie im Original bis zum Ausschalten.
        case 8: { hw::setSound(true); static HWTest g(u8g2); g.run(); break; }
    }
}

}  // namespace picopac
