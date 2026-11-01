# PicoPac-Quellen

Die Spiele der PicoPac-Sammlung aus
[hcl1981/picoboy](https://github.com/hcl1981/picoboy) (`Arduino/PicoPac`,
Stand d7913ab). Sie laufen über eine dünne Arduino-/U8g2-Hülle
(`../src/picopac/`) auf dem Pico SDK und sehen aus und spielen sich wie im
Original. `PicoPac.ino.orig` ist die ursprüngliche Hauptdatei (ersetzt durch
`../src/picopac/picopac.cpp`).

Änderungen gegenüber dem Original:

* `Misc/starintro.h`: Eintrag 6 heißt MULTI, 7 ist Demo, 8 HWTest, 9 Settings;
  Conway ist entfallen. Bietet ein zweiter PicoBoy über USB ein Spiel an, verlässt
  das Menü sich selbst und steigt ein (eine Zeile am Schleifenanfang).
* `Demo/demo.h`, `Misc/starintro.h`: große lokale Felder sind `static` (der
  Stapel des Pico SDK ist kleiner als beim Arduino-Core). Am Verhalten ändert
  das nichts, alle Felder werden vor der Benutzung gefüllt.
* `HWTest/STK8BA58.h`: die beiden Bytes eines Messwerts werden in eigenen Zeilen
  gelesen. Im Original standen beide `Wire.read()` in einem Ausdruck, dessen
  Auswertungsreihenfolge C++ nicht festlegt. `Wire` selbst ist in
  `../src/picopac/Wire.h` nachgebildet.

Diese Quellen stehen hier wie die ganze Firmware unter der GPL v2+ (`../LICENSE`).
