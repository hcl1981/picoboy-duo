// Spieletabelle von PicoBoy Duo. Eigene Datei, damit andere Firmwares (z. B.
// das Unterprojekt jumpnbump/) den Rahmen mit einer eigenen Tabelle nutzen.
#include "play.h"
#include "games/games.h"

namespace jnb { void host(); void guest(); }   // Popbit (Jump 'n Bump), jumpnbump/src

namespace play {

// Name ('|' = Trennstelle fuer hochkant) und Kurzname fuers Menue (max. 8 Zeichen)
const Game GAMES[] = {
    { "LIGHTCYCLE",       "CYCLES",   gfx::LAND, tron::host,   tron::guest   },
    { "TENNIS",           "TENNIS",   gfx::PORT, pong::host,   pong::guest   },
    { "VECTOR DUEL",      "DUEL",     gfx::LAND, duel::host,   duel::guest   },
    { "POCKET TANKS",     "TANKS",    gfx::LAND, arty::host,   arty::guest   },
    { "BOMBING BOB",      "BOMB BOB", gfx::LAND, bomber::host, bomber::guest },
    { "CHESS",            "CHESS",    gfx::LAND, chess::host,  chess::guest  },
    { "W'N'R",            "W'N'R",    gfx::LAND, wnr::host,    wnr::guest    },
    { "QUADLINK",         "QUADLINK", gfx::LAND, c4::host,     c4::guest     },
    { "DOTS'N'|BOXES",    "DOTS",     gfx::PORT, dots::host,   dots::guest   },
    { "POPBIT",           "POPBIT",   gfx::LAND, jnb::host,    jnb::guest    },
};
const int NUM_GAMES = sizeof GAMES / sizeof GAMES[0];

}  // namespace play
