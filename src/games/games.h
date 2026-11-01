// Einstiegspunkte der Spiele (je eine Schleife fuer Host und Gast).
#pragma once

#define PB_GAME(ns) namespace ns { void host(); void guest(); }
PB_GAME(tron)
PB_GAME(pong)
PB_GAME(duel)
PB_GAME(arty)
PB_GAME(bomber)
PB_GAME(chess)
PB_GAME(wnr)
PB_GAME(c4)
PB_GAME(dots)
#undef PB_GAME
