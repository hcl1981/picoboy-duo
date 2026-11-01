// Hardware des (alten) PicoBoy: Joystick, LEDs, Lautsprecher, Zeit, Zufall.
// Das Display steckt in gfx.h.
//
//   Joystick  CENTER GP0  UP GP1  RIGHT GP2  DOWN GP3  LEFT GP4  (aktiv LOW)
//   LEDs      ROT GP5  GELB GP6  GRUEN GP7                      (aktiv HIGH)
//   Ton       GP15 (PWM ueber Tiefpass)
//
// Die Pin-Namen gelten fuer die QUERLAGE (so, wie MicroPython den PicoBoy
// benutzt). Hochkant (wie PicoPac/Tetris) ist das Geraet 90 Grad im
// Uhrzeigersinn gedreht; die Tasten werden dann mitgedreht (siehe hw.cpp).
#pragma once
#include <stdint.h>

namespace hw {

// Logische Tasten - bezogen auf die aktuelle Bildschirmlage (gfx::orient).
enum Key : uint8_t { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CENTER = 4 };
constexpr uint8_t K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8, K_C = 16;
constexpr uint8_t K_DIRS = K_UP | K_DOWN | K_LEFT | K_RIGHT;

enum Led : uint8_t { LED_RED = 0, LED_YELLOW = 1, LED_GREEN = 2 };

void init();

uint32_t ms();

// Gehaltene Tasten als Bitmaske (K_*), entprellt.
uint8_t held();
// Laufende Tastendruck-Zaehler je logischer Taste (UP..CENTER). Zaehler statt
// Flanken: so geht kein kurzer Druck verloren, auch wenn der Leser gerade
// langsam ist oder der Wert erst ueber die Leitung zum Host muss.
void counts(uint8_t out[5]);
// Seit wann die Taste gehalten wird (ms), 0 = nicht gedrueckt.
uint32_t heldMs(Key k);

void beep(uint16_t hz, uint16_t durMs);   // nicht blockierend
void setSound(bool on);
bool soundOn();

void led(Led which, bool on);
void ledLevel(Led which, uint8_t level);   // 0..255 per PWM (Lauflicht im Menue)

uint32_t rnd();                       // xorshift32
int rndRange(int lo, int hiIncl);
void seed(uint32_t s);

void sleepMs(uint32_t ms);

}  // namespace hw
