// Einstellungen (letzter Eintrag im Startmenue): Ton und Displayhelligkeit.
// Gespeichert im EEPROM-Bereich der PicoPac-Spiele, hinter den Highscores.
#pragma once
#include <stdint.h>

namespace settings {
void load();                 // nach picopac::begin(): lesen und anwenden
void menu();                 // kehrt ueber BACK zurueck
uint8_t contrast();          // gewaehlter Kontrast (Standard 31 wie PicoPac)
}
