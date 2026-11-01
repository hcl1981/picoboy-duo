// Bildschirmaufbau des PicoPac-Startmenues fuer eigene Menues (MULTI,
// Einstellungen): hochkant, Schrift t0_11, Titel oben, zwei Zeilen in der
// Mitte, unten blinkend "Select" und "< EINTRAG >", Sternenfeld, LED-Lauflicht.
#pragma once

namespace starscreen {
void begin();                                   // Bild leeren, Schrift setzen
void centered(const char* s, int baseline);     // waagrecht mittig (0..63)
void finish(const char* label, const char* item);   // unten (item 0: nichts), Sterne, senden
void box(int x, int y, int w, int h, bool filled);
void ledChase();
void ledsOff();
}
