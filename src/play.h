// Gemeinsamer Rahmen fuer alle Zweispieler-Spiele.
//
// Aufteilung wie in der i2collection: der HOST rechnet das komplette Spiel und
// schickt bei jedem Austausch den vollstaendigen Zustand; der GAST schickt nur
// seine Tasten und zeichnet, was ankommt. Dadurch kann nichts auseinanderlaufen.
//
// Paket Host -> Gast:  [MAGIC_H][Spiel][Sitzung][Spielzustand ...]
// Paket Gast -> Host:  [MAGIC_G][gehalten][5 Tastenzaehler][Menue-Zaehler][Spiel][Sitzung]
//
// Spiel 0 heisst "Host ist im Menue". Die Sitzungsnummer steigt bei jedem
// Spielstart; daran erkennt der Gast einen Neubeginn auch beim selben Spiel.
//
// Typische Schleifen:
//   Host:  while (play::running()) { if (play::live()) { ...rechnen...; play::send(...); }
//                                    ...zeichnen...; play::present(); }
//   Gast:  while (play::running()) { if (play::receive(buf, &n)) ...uebernehmen...;
//                                    ...zeichnen...; play::present(); }
#pragma once
#include <stdint.h>
#include "gfx.h"
#include "hw.h"
#include "link.h"

namespace play {

constexpr int PAYLOAD_MAX = usblink::M2S_MAX - 3;

struct Pad {
    uint8_t held = 0;           // hw::K_* bits
    uint8_t cnt[5] = {};        // Druckzaehler UP, DOWN, LEFT, RIGHT, CENTER
};

// Tastendruecke seit dem letzten take() - aus den Zaehlern, daher geht kein
// Druck verloren, auch wenn ein Austausch ausfaellt.
struct Edges {
    uint8_t last[5] = {};
    void sync(const Pad& p);
    uint8_t take(const Pad& p);
};

struct Game {
    const char* name;           // Titel; '|' = Umbruchstelle hochkant
    const char* menu;           // Kurzname fuers Menue (max. 8 Zeichen)
    gfx::Orient orient;
    void (*host)();
    void (*guest)();
};
extern const Game GAMES[];
// Name ohne die Trennstelle '|' (die nur hochkant fuer den Zeilenumbruch da ist)
void displayName(const Game& g, char* out, int cap);
extern const int NUM_GAMES;

// ---- Verbindung ---------------------------------------------------------
void beginHost();
void beginGuest();
void end();
bool isHost();
bool linkUp();                  // fehlerfreier Austausch in den letzten 300 ms

// Host: im Menue (Spiel 0) - Gast wartet
void hostIdle();
bool guestPresent();            // gueltiges Gast-Paket in der letzten Sekunde
// Gast: welches Spiel der Host gerade anbietet (0 = keins / Menue)
bool hostOffer(uint8_t* game, uint8_t* session);
// Gast im Wartebildschirm: Tasten melden (fuer den Verbindungstest egal)
void guestIdle();

// ---- Spielrahmen --------------------------------------------------------
// Host: startet Spiel idx (Sitzung wird hochgezaehlt), zeigt den Startbildschirm
// und ruft die Spielschleife. Gast: dasselbe fuer das vom Host angebotene Spiel.
// Liefert true, wenn die Gegenseite ebenfalls Host war (beide haben gleichzeitig
// ein Spiel gewaehlt) - dann muss einer ausweichen, siehe main.cpp.
bool runHost(int idx);
void runGuest(int idx, uint8_t session);

bool running();                 // pro Bild aufrufen; false -> Spiel verlassen
bool live();                    // Host: Spiel darf weiterlaufen (Gast da, Link ok)
void present();                 // Hinweise einblenden + Bild ausgeben
bool host();                    // dieses Geraet ist Spieler 1
Pad pad(int player);            // Host: 1 = eigene Tasten, 2 = die des Gasts
Pad localPad();
void send(const uint8_t* p, int n);          // Host: Spielzustand
bool receive(uint8_t* p, int* n);            // Gast: neuer Zustand da?
uint32_t sinceStart();                       // ms seit Spielbeginn

// ---- Kleine Helfer fuer die Spiele --------------------------------------
inline void put16(uint8_t*& p, int v) { *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8); }
inline int get16(const uint8_t*& p) { int v = (int16_t)(p[0] | (p[1] << 8)); p += 2; return v; }

// Ergebnis-Kasten: "YOU WIN!" / "YOU LOSE" / "DRAW",
// darunter der Stand aus Sicht dieses Geraets und der Hinweis zum Weiterspielen.
// winner: 1 = P1 (Host), 2 = P2 (Gast), 3 = unentschieden
void resultBox(int winner, int s1, int s2, const char* extra = nullptr);

}  // namespace play
