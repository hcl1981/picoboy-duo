# PicoBoy Duo – PicoPac plus Zweispieler über USB-C

Firmware für den **alten PicoBoy** (RP2040, 128×64-OLED, schwarz-weiß), gebaut
mit dem Pico SDK. Sie vereint die Einzelspieler-Sammlung **PicoPac**
(gleiches Aussehen, gleiches Spiel) mit neun neuen Spielen für zwei PicoBoys,
die über ein USB-C-Kabel verbunden sind (Umsetzung der
[i2collection](https://github.com/hcl1981/pbc-plus/tree/main/i2collection) des
PicoBoy Color Plus, dazu Jump 'n Bump als **POPBIT**). W'n'R aus PicoPac gibt es
zusätzlich zu zweit.

Die fertige Firmware steht unter
[Releases](https://github.com/hcl1981/picoboy-duo/releases/latest). Selbst bauen:

```bash
./build.sh          # -> dist/pbduo.uf2
```

Flashen: Beim Einstecken BOOTSEL halten, dann `pbduo.uf2` auf das Laufwerk
`RPI-RP2` kopieren. **Beide Geräte bekommen dieselbe Firmware.**

Der USB-Anschluss gehört dem Spiele-Link, der PC sieht das Gerät deshalb nicht
als serielle Schnittstelle. Auch zurück zu PicoPac, MicroPython oder Arduino geht
es nur über BOOTSEL.

![Übersicht](screenshots/overview.png)

## Hauptmenü

Das Sternenfeld-Menü von PicoPac (hochkant, Highscores, LED-Lauflicht),
Links/Rechts blättert, Mitte wählt:

1. PICTRIS  2. SCLANGE  3. TABMAN  4. W'n'R  5. Memor – wie in PicoPac
6. **MULTI** – die Zweispieler-Spiele
7. Demo
8. HWTest – wie in PicoPac: drei Pieptöne, alle LEDs, dann ein Würfel, der dem
   Beschleunigungssensor folgt, und ein Punkt je gedrückter Joystick-Richtung.
   Der Ton läuft auch bei „Sound off“. Beenden durch Ausschalten.
9. **Settings** – Links/Rechts wählt, Mitte ändert, BACK speichert:
   * **Sound** an/aus
   * **Display**: Helligkeit in 5 Stufen (Standard = Stufe 3 wie PicoPac;
     heller kostet Knopfzellenstrom), auch mit Hoch/Runter
   * **Scores**: alle Highscores löschen, nach Rückfrage (NO vorausgewählt)
   * **Info**: Lizenzhinweise, Hoch/Runter blättert

Conway ist entfallen. Die Einzelspieler-Spiele laufen wie im
Original bis zum Ausschalten; die Highscores liegen an derselben Flash-Stelle
wie bei der Originalfirmware und bleiben beim Umflashen erhalten.
Langes Halten der Mitte-Taste hat nirgends eine Sonderfunktion.

## MULTI

Gleicher Aufbau und gleiche Schrift wie das Hauptmenü: oben „MULTI“, wo dort
die Highscores stehen „CONNECT USB CABLE“. Links/Rechts blättert, Mitte wählt:

* ein **Spiel** wählen → dieses Gerät bietet es an (Host)
* **BACK** → zurück ins Hauptmenü

Eine Rolle muss man nicht wählen: Solange ein Gerät im Hauptmenü oder in MULTI
steht, lauscht es auf die Gegenseite. Wählt das andere Gerät ein Spiel, springt es automatisch
mit hinein. Wählen beide gleichzeitig ein Spiel, merken sie das und eines
weicht aus – beide landen im selben Spiel.

Der Startbildschirm eines Spiels zeigt nur den Namen und entweder
„WAITING FOR PARTNER" oder „PUSH!" – Mitte startet. Beim Host bricht Links ab.

## Die Spiele

| Spiel (Menü) | Lage | Steuerung | Anpassung an 128×64 s/w |
|---|---|---|---|
| LIGHTCYCLE (CYCLES) | quer | Steuerkreuz | Feld 62×30; P2-Spur gerastert |
| TENNIS | hochkant | Links/Rechts | eigener Schläger unten |
| VECTOR DUEL (DUEL) | quer | L/R drehen, Hoch Schub, Runter Schild, Mitte Feuer | P1 Umriss, P2 gefüllt |
| POCKET TANKS (TANKS) | quer | L/R Winkel, H/R Kraft; Mitte → Waffe wählen (L/R), Mitte → Feuer, H/R zurück | Höhenkarte; Winkel als gestrichelte Rohrverlängerung, Kraft als schmaler Balken, Waffe nur bei der Auswahl |
| BOMBING BOB | quer | Steuerkreuz, Mitte Bombe | Feld 15×7; Mauern voll, Ziegel als leere Kästchen |
| CHESS | quer | Steuerkreuz, Mitte wählen/ziehen | eigene 7×7-Figuren; Weiß gefüllt, Schwarz Umriss |
| W'N'R | quer | Steuerkreuz | Grafik wie W'n'R der PicoPac-Sammlung (WATCH / REPEAT / ADD) |
| QUADLINK | quer | L/R Spalte, Mitte einwerfen | Einwurfpfeil nur bei eigenem Zug; Gewinnreihe blinkt |
| DOTS'N'BOXES (DOTS) | hochkant | Steuerkreuz wählt einen Punkt, Mitte hält ihn fest, Richtung zieht den Strich (nochmal Mitte: abbrechen) | P1-Kästchen gefüllt, P2 gekreuzt |
| POPBIT | quer | L/R laufen, Hoch oder Mitte springen | Jump 'n Bump, siehe `jumpnbump/README.md` |

Der alte PicoBoy hat keine A/B-Tasten; überall übernimmt **Mitte** deren
Aufgabe. Nach Spielende geht es für beide zurück in die MULTI-Liste.

## Verbinden

1. Beide Geräte mit einem USB-C-Kabel verbinden. Wie beim PBC+ gilt:
   **D+, D− und GND verbinden, VBUS nicht** – beide Geräte laufen auf ihrer
   Knopfzelle, der USB-Anschluss ist ja belegt.
2. Ein Gerät wählt unter MULTI ein Spiel; das andere steigt von selbst ein,
   wenn es im Hauptmenü oder in MULTI steht (nicht aus einem laufenden
   Einzelspieler-Spiel heraus).
3. Beide drücken auf dem Startbildschirm Mitte.

Reißt die Verbindung ab, hält das Spiel an und zeigt „LINK LOST / CHECK
CABLE"; die rote LED leuchtet. Steckt das Kabel wieder, geht es weiter.

Kommt keine Verbindung zustande, liegt es meist am Kabel (viele USB-C-Kabel
sind reine Ladekabel ohne D+/D−) – alle vier Steckerorientierungen probieren.

## Wie die Verbindung funktioniert

Kein USB-Protokoll: der RP2040 kann seine USB-Leitungen per
`USBPHY_DIRECT`-Override direkt als Ein-/Ausgänge benutzen (die Register sind
bitgleich zum RP2350). Darauf läuft derselbe getaktete Halbduplex-Link wie beim
D+/D−-Link der PBC+-Projekte:

```
D−  = CLK    treibt nur der Host
D+  = DATA   Halbduplex
```

Beide Seiten arbeiten im Timer-Interrupt; die Spiele merken davon nichts. Der
Host rechnet das ganze Spiel und schickt alle ~15 ms den vollständigen
Zustand, der Gast schickt seine Tasten (als Druckzähler, damit kein kurzer
Druck verloren geht). Die zeitkritischen Bitschleifen liegen im RAM.

Gegenüber der i2collection entfallen damit TinyUSB, der Host-Modus, die
Treiber-Patches und die Neu-Enumeration.

| Datei | Inhalt |
|---|---|
| `src/main.cpp` | Hauptmenü (PicoPac) → Einzelspieler-Spiel oder MULTI |
| `src/multi.*` | MULTI-Liste |
| `src/settings.*` | Einstellungen (EEPROM ab Adresse 16) |
| `src/starscreen.*` | Menü-Aufbau wie PicoPac für MULTI und Settings |
| `picopac/` | Quellen der PicoPac-Sammlung (siehe `picopac/README.md`) |
| `src/picopac/` | dünne Arduino-/U8g2-Hülle aufs Pico SDK, EEPROM im Flash |
| `third_party/u8g2/` | U8g2-C-Bibliothek (BSD-2) mit den benötigten Schriften |
| `jumpnbump/src/` | POPBIT (Jump 'n Bump) |
| `src/link.*` | D+/D−-Link (Bitebene, Timer-Interrupts) |
| `src/play.*` | Sitzung: Paketkopf, Pause, Startbildschirm |
| `src/gfx.*`, `src/hw.*` | SH1106-Display, Tasten, Ton, LEDs |
| `src/games/*.cpp` | die neun i2collection-Spiele, `table.cpp` = Spieleliste |
| `tools/sim/` | PC-Simulator (siehe unten) |

## Simulator

`tools/sim` baut die echten Spiel- und Grafikquellen für den PC. Host und Gast
laufen als zwei Prozesse, verbunden über einen Socket statt des Kabels;
Tasten kommen aus einem Zufallsskript, das Display ist ein nachgebildeter
SH1106, der Beschleunigungssensor ebenfalls (`PBSIM_NOSENSOR=1`: kein Sensor). Beide Bildschirme werden als PGM gespeichert. Mit AddressSanitizer.

```bash
tools/sim/build.sh
tools/sim/pbsim <spiel 0..9> <sekunden> <ordner> [ms zwischen Bildern] [seed]
tools/sim/pbsim -1 <sekunden> <ordner>     # ganze Firmware, ein Geraet
tools/sim/pbsim -2 <sekunden> <ordner>     # ganze Firmware auf beiden Geraeten
```

Statt Zufallseingaben lassen sich feste Tastenfolgen vorgeben:
`PBSIM_KEYS_HOST` / `PBSIM_KEYS_GUEST`, ab 5 s alle 300 ms ein Zeichen
(`U D L R C`, `.` = nichts, `*` am Ende = wiederholen). `PBSIM_RAW=1`
speichert den Displayspeicher ungedreht (für Spiele, die selbst drehen).

Der Simulator prüft Spiellogik, Synchronisation und Bildaufbau – **nicht** die
Elektrik und das Timing des Kabel-Links.

## Stand

Die Spiele sind im Simulator und auf echten PicoBoys getestet. Die
Originalsoftware des PicoBoy läuft mit 50 MHz; das schont die Knopfzelle, und
auch der D+/D−-Link läuft mit diesem Takt.

HWTest ist nachträglich wieder dazugekommen. Im Simulator läuft es mit und ohne
Sensor; die Sensoranbindung (`src/picopac/Wire.h`) ist auf einem echten PicoBoy
mit einem Testprogramm geprüft, der vollständige HWTest noch nicht.


## Lizenz

**GNU General Public License, Version 2 oder (nach Wahl) jede spätere Version**
(`LICENSE`). Weil POPBIT (Jump 'n Bump, © 1998 Brainchild Design) unter der
GPL v2+ steht (`jumpnbump/LICENSE`), gilt das für die Firmware als Ganzes;
Weitergabe von `pbduo.uf2` setzt also voraus, dass auch der Quelltext dabei oder
zugänglich ist. Die eigenen Teile und die PicoPac-Quellen stehen unter derselben
Lizenz.

Enthaltene Fremdteile behalten ihre Lizenz:

| Teil | Lizenz | Datei |
|---|---|---|
| U8g2 | BSD-2 (Schriften je Schrift, siehe dort) | `third_party/u8g2/LICENSE` |
| 5×7-Schrift aus Adafruit GFX | BSD | `LICENSE-Adafruit-GFX.txt` |
| Pico SDK (wird nur gelinkt, nicht mitgeliefert) | BSD-3 | `LICENSE-pico-sdk.txt` |

## Nachbauen

Gebaut mit Pico SDK 2.2.0 (`a1438dff`) und arm-none-eabi-gcc 14.2.1
(Build Release). Mit diesen Versionen entsteht dieselbe `pbduo.uf2`; sie
unterscheidet sich nur im eingebauten Baudatum.
