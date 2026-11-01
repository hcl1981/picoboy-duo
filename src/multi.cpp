// Zweispieler-Spiele (PicoBoy Duo): die Liste hinter MULTI im Startmenue.
//
// Umsetzung der i2collection des PicoBoy Color Plus fuer den PicoBoy mit
// 128x64-OLED, plus Popbit (Jump 'n Bump). Wer ein Spiel waehlt, ist Host; das
// andere Geraet steigt von selbst ein, wenn es in dieser Liste oder im
// Startmenue steht (picopac::joinPending).
// Aufbau wie das PicoPac-Menue: hochkant, Sternenfeld, unten "Select <NAME>".

#include "pico/stdlib.h"
#include <stdio.h>

#include "gfx.h"
#include "hw.h"
#include "link.h"
#include "multi.h"
#include "play.h"
#include "picopac/picopac.h"
#include "starscreen.h"

namespace {

using starscreen::ledChase;
using starscreen::ledsOff;

// Aufbau wie das PicoPac-Startmenue: oben der Titel, in der Mitte statt der
// Highscores der Hinweis zum Kabel, unten "Select" und der Eintrag.
void starScreen(const char* item) {
    starscreen::begin();
    starscreen::centered("MULTI", 15);
    starscreen::centered("CONNECT", 60);
    starscreen::centered("USB CABLE", 75);
    starscreen::finish("Select", item);
}

// Eintraege: die Spiele, dann BACK (zurueck ins Startmenue)
enum { ITEM_BACK = 100, ITEM_OFFER };
int itemAt(int i) { return i < play::NUM_GAMES ? i : ITEM_BACK; }
const int NUM_ITEMS = play::NUM_GAMES + 1;

// In der Liste lauscht das Geraet als Gast: waehlt das andere ein Spiel, geht es
// von selbst mit (Rueckgabe ITEM_OFFER). Wer selbst ein Spiel waehlt, wird Host.
int list(int& sel, uint8_t& offerGame, uint8_t& offerSession) {
    gfx::setOrient(gfx::PORT);
    if (!usblink::running() || usblink::isHost()) play::beginGuest();
    play::Edges e;
    e.sync(play::localPad());
    for (;;) {
        play::guestIdle();
        if (play::hostOffer(&offerGame, &offerSession) && offerGame >= 1 && offerGame <= play::NUM_GAMES) {
            ledsOff();
            hw::beep(1200, 60);
            return ITEM_OFFER;
        }
        uint8_t k = e.take(play::localPad());
        // alle vier Richtungen blaettern (hochkant: Rechts/Runter = weiter)
        if (k & (hw::K_RIGHT | hw::K_DOWN)) { sel = (sel + 1) % NUM_ITEMS; hw::beep(500, 15); }
        if (k & (hw::K_LEFT | hw::K_UP)) { sel = (sel + NUM_ITEMS - 1) % NUM_ITEMS; hw::beep(500, 15); }
        if (k & hw::K_C) { ledsOff(); hw::beep(900, 50); return itemAt(sel); }
        ledChase();

        int it = itemAt(sel);
        const char* name = it < play::NUM_GAMES ? play::GAMES[it].menu : "BACK";
        starScreen(name);
        hw::sleepMs(10);
    }
}

// Spiel anbieten. Haben beide gleichzeitig ein Spiel gewaehlt (beide Host),
// weicht jede Seite eine zufaellige Zeit lang in die Gastrolle aus; wer dabei
// das Angebot der anderen sieht, spielt dort mit.
void hostGame(int idx) {
    for (;;) {
        play::beginHost();
        if (!play::runHost(idx)) return;
        play::beginGuest();
        uint32_t until = hw::ms() + (uint32_t)hw::rndRange(300, 1500);
        while (hw::ms() < until) {
            play::guestIdle();
            uint8_t g, s;
            if (play::hostOffer(&g, &s) && g >= 1 && g <= play::NUM_GAMES) {
                play::runGuest(g - 1, s);
                return;
            }
            hw::sleepMs(10);
        }
    }
}

uint8_t joinGame, joinSession;          // Angebot, das das Startmenue gesehen hat

}  // namespace

bool picopac::joinPending() {
    play::guestIdle();
    return play::hostOffer(&joinGame, &joinSession) && joinGame >= 1 && joinGame <= play::NUM_GAMES;
}

namespace multi {

void listen() {
    if (!usblink::running() || usblink::isHost()) play::beginGuest();
}

void join() {
    ledsOff();
    hw::beep(1200, 60);
    play::runGuest(joinGame - 1, joinSession);
}

void menu() {
    static int sel = 0;
    for (;;) {
        uint8_t og = 0, os = 0;
        int it = list(sel, og, os);
        hw::seed(hw::ms() * 7919u + sel);
        if (it == ITEM_OFFER) play::runGuest(og - 1, os);
        else if (it < play::NUM_GAMES) hostGame(it);
        else {                                   // BACK
            play::end();
            ledsOff();
            return;
        }
    }
}

}  // namespace multi
