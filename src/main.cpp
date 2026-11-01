// PicoBoy Duo - die Spielesammlung fuer den (alten) PicoBoy.
//
// Startmenue und Einzelspieler-Spiele sind die der PicoPac-Sammlung
// (PICTRIS, SCLANGE, TABMAN, W'n'R, Memor, Demo, HWTest). Eintrag MULTI fuehrt zu den
// Zweispieler-Spielen ueber ein USB-C-Kabel (multi.cpp).

#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "gfx.h"
#include "hw.h"
#include "multi.h"
#include "play.h"
#include "settings.h"
#include "picopac/picopac.h"

int main() {
    // 50 MHz wie die PicoBoy-Originalsoftware: reicht fuer alle Spiele und
    // schont die Knopfzelle - beim Spielen zu zweit sind beide Geraete ohne
    // USB-Strom, der Anschluss ist ja belegt.
    set_sys_clock_khz(50000, true);
    hw::init();
    gfx::init();
    hw::seed(hw::ms());
    picopac::begin();
    settings::load();

    for (;;) {
        multi::listen();                // auch im Startmenue steigt man von selbst ein
        // Das Startmenue springt sofort los, wenn Mitte noch gehalten ist (vom
        // BACK in MULTI/Settings) - erst loslassen abwarten
        while (hw::held() & hw::K_C) hw::sleepMs(5);
        int id = picopac::menu();
        if (id == picopac::JOIN) multi::join();
        else if (id == picopac::MULTI) multi::menu();
        else if (id == picopac::SETTINGS) settings::menu();
        else {
            play::end();                // Einzelspieler: laeuft bis zum Ausschalten
            picopac::run(id);
        }
    }
}
