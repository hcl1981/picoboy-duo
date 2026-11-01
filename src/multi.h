// Zweispieler-Spiele ueber USB-C (Liste hinter MULTI), siehe multi.cpp
#pragma once

namespace multi {
void menu();     // kehrt ueber den Eintrag BACK zurueck
void listen();   // im Startmenue als Gast auf ein Angebot lauschen
void join();     // nach picopac::JOIN: dem angebotenen Spiel beitreten
}
