// Einzelspieler-Sammlung PicoPac (siehe picopac.cpp)
#pragma once

namespace picopac {
constexpr int MULTI = 6;     // Menueeintrag, der zu den Zweispieler-Spielen fuehrt
constexpr int SETTINGS = 8;  // letzter Menueeintrag: Einstellungen
constexpr int JOIN = 100;    // menu(): der andere PicoBoy bietet ein Spiel an

bool joinPending();          // vom Startmenue je Bild gefragt (multi.cpp)

void begin();                // EEPROM und Display wie in PicoPac.ino
int menu();                  // Sternenfeld-Startmenue, liefert 1..8 oder JOIN
void run(int id);            // startet ein Spiel (kehrt wie im Original nicht zurueck)
}
