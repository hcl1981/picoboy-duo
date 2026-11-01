// Punkt-zu-Punkt-Verbindung zweier PicoBoys ueber ein USB-C-Kabel.
//
// Kein USB-Protokoll: der RP2040 laesst seine USB-Leitungen per
// USBPHY_DIRECT-Override direkt als Ein-/Ausgaenge benutzen. Darauf laeuft ein
// getakteter Halbduplex-Link (dasselbe Verfahren wie der D+/D--Link der
// PBC+-Projekte doom/tyrian, hier fuer RP2040 und Zeitgeber-Interrupts):
//
//     D-  = CLK   treibt nur der Host (Master)
//     D+  = DATA  Halbduplex, die Richtung ergibt sich aus der Phase
//     GND gemeinsam, VBUS NICHT verbinden (beide Geraete eigenversorgt)
//
// Austausch (vom Host angestossen, im Timer-Interrupt beider Seiten):
//   1. Host zieht CLK hoch            (Anklopfen)
//   2. Gast antwortet mit DATA hoch   (bereit)
//   3. Host senkt CLK, Gast gibt DATA frei
//   4. Host -> Gast:  [0xA5][len][Daten][crc16]   Gast tastet auf CLK-Flanke
//   5. Richtungswechsel
//   6. Gast -> Host:  [0xA5][len][Daten][crc16]   Host taktet weiter
//
// Die Anwendung sieht davon nur zwei Puffer: setTx() legt fest, was bei jedem
// Austausch rausgeht (Host: Spielzustand, Gast: Tasten), getRx() liefert das
// zuletzt fehlerfrei Empfangene. Geht ein Austausch schief, gilt beim
// naechsten einfach wieder der neueste Stand - die Spiele senden ihren
// Zustand immer vollstaendig, nie nur Aenderungen.
#pragma once
#include <stdint.h>

namespace usblink {

constexpr int M2S_MAX = 100;     // Nutzdaten Host -> Gast
constexpr int S2M_MAX = 16;      // Nutzdaten Gast -> Host

void start(bool host);           // Leitungen uebernehmen, Timer starten
void stop();                     // Timer aus, Leitungen freigeben
bool running();
bool isHost();

void setTx(const uint8_t* d, int n);
// Kopiert die zuletzt empfangenen Nutzdaten, liefert deren Laenge (0 = noch nichts).
int getRx(uint8_t* d, int cap);
uint32_t rxCount();              // steigt mit jedem fehlerfreien Austausch
uint32_t msSinceOk();            // Zeit seit dem letzten fehlerfreien Austausch

void setHalfBitNs(uint32_t ns);  // Taktrate des Hosts (Gast braucht keine)
uint32_t halfBitNs();

struct Stats {
    uint32_t ok;          // fehlerfreie Austausche
    uint32_t noPeer;      // Host: niemand hat auf das Anklopfen geantwortet
    uint32_t timeout;     // Taktflanke/Antwort ausgeblieben mitten im Austausch
    uint32_t bad;         // Sync-, Laengen- oder CRC-Fehler
    uint32_t conflict;    // Host: CLK war schon hoch -> Gegenseite ist auch Host
    uint8_t lastFail;     // Code des letzten Fehlers (siehe link.cpp), 0 = ok
    uint8_t lines;        // Bit1 = CLK hoch, Bit0 = DATA hoch
};
Stats stats();
void resetStats();

}  // namespace usblink
