#include <initializer_list>
#include "hw.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

namespace hw {
namespace {

constexpr uint PIN_SPEAKER = 15;
constexpr uint PIN_LED[3] = { 5, 6, 7 };
constexpr int NPIN = 5;                      // GP0..GP4 = Joystick

// Logische Taste -> GPIO, je Bildschirmlage (UP, DOWN, LEFT, RIGHT, CENTER).
// Hochkant ist das Geraet im Uhrzeigersinn gedreht: der Pin "UP" zeigt dann
// nach rechts, "LEFT" nach oben (so benutzt es auch das Original-Tetris).
constexpr uint8_t PIN_OF[2][5] = {
    { 1, 3, 4, 2, 0 },    // quer
    { 4, 2, 3, 1, 0 },    // hochkant
};

// Von der Abtastung (Timer-Interrupt) geschrieben:
volatile uint8_t pinHeld;                    // Bit n = GPIO n entprellt gedrueckt
volatile uint8_t pinCount[NPIN];
volatile uint32_t pinSince[NPIN];
uint8_t rawPrev;
repeating_timer scanTimer;

alarm_id_t beepAlarm = 0;
bool sound = true;
uint32_t rngState = 0x9E3779B9u;

// Alle 4 ms abtasten; eine Aenderung zaehlt erst, wenn sie zweimal hintereinander
// gleich gelesen wurde (entprellt, ~8 ms Verzoegerung).
bool scan(repeating_timer*) {
    uint32_t in = gpio_get_all();
    uint8_t raw = 0;
    for (int i = 0; i < NPIN; i++)
        if (!(in & (1u << i))) raw |= 1u << i;
    uint8_t stable = (uint8_t)~(raw ^ rawPrev);
    rawPrev = raw;
    uint8_t now = (uint8_t)((pinHeld & ~stable) | (raw & stable));
    uint8_t pressed = now & ~pinHeld;
    if (pressed) {
        uint32_t t = to_ms_since_boot(get_absolute_time());
        for (int i = 0; i < NPIN; i++)
            if (pressed & (1u << i)) { pinCount[i]++; pinSince[i] = t; }
    }
    pinHeld = now;
    return true;
}

int64_t beepOff(alarm_id_t, void*) {
    uint slice = pwm_gpio_to_slice_num(PIN_SPEAKER);
    pwm_set_enabled(slice, false);
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_SIO);
    gpio_put(PIN_SPEAKER, 0);
    beepAlarm = 0;
    return 0;
}

}  // namespace

void init() {
    for (int i = 0; i < NPIN; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }
    // LEDs per PWM (GP5 = Slice 2 B, GP6/GP7 = Slice 3 A/B)
    for (uint slice : { 2u, 3u }) {
        pwm_set_wrap(slice, 255);
        pwm_set_clkdiv(slice, 8.0f);
        pwm_set_enabled(slice, true);
    }
    for (uint p : PIN_LED) {
        pwm_set_gpio_level(p, 0);
        gpio_set_function(p, GPIO_FUNC_PWM);
    }
    gpio_init(PIN_SPEAKER);
    gpio_set_dir(PIN_SPEAKER, GPIO_OUT);
    gpio_put(PIN_SPEAKER, 0);
    sleep_ms(2);                              // Pullups wirken lassen
    add_repeating_timer_ms(4, scan, nullptr, &scanTimer);
}

uint32_t ms() { return to_ms_since_boot(get_absolute_time()); }

uint8_t held() {
    uint8_t p = pinHeld, m = 0;
    const uint8_t* map = PIN_OF[gfx::orient()];
    for (int k = 0; k < 5; k++)
        if (p & (1u << map[k])) m |= 1u << k;
    return m;
}

void counts(uint8_t out[5]) {
    const uint8_t* map = PIN_OF[gfx::orient()];
    for (int k = 0; k < 5; k++) out[k] = pinCount[map[k]];
}

uint32_t heldMs(Key k) {
    uint8_t pin = PIN_OF[gfx::orient()][k];
    if (!(pinHeld & (1u << pin))) return 0;
    uint32_t d = ms() - pinSince[pin];
    return d ? d : 1;
}

void beep(uint16_t hz, uint16_t durMs) {
    if (!sound || hz == 0 || durMs == 0) return;
    if (beepAlarm) { cancel_alarm(beepAlarm); beepAlarm = 0; }
    uint32_t f = clock_get_hz(clk_sys);
    float div = (float)f / ((float)hz * 4096.0f);
    if (div < 1.0f) div = 1.0f;
    if (div > 255.0f) div = 255.0f;
    uint32_t wrap = (uint32_t)((float)f / (div * (float)hz));
    if (wrap < 2) wrap = 2;
    if (wrap > 65535) wrap = 65535;
    uint slice = pwm_gpio_to_slice_num(PIN_SPEAKER);
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);
    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, (uint16_t)(wrap - 1));
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_SPEAKER), (uint16_t)(wrap / 2));
    pwm_set_enabled(slice, true);
    beepAlarm = add_alarm_in_ms(durMs, beepOff, nullptr, true);
}

void setSound(bool on) { sound = on; }
bool soundOn() { return sound; }

void led(Led which, bool on) { ledLevel(which, on ? 255 : 0); }
void ledLevel(Led which, uint8_t level) { pwm_set_gpio_level(PIN_LED[which], level); }

uint32_t rnd() {
    uint32_t x = rngState;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rngState = x;
}

int rndRange(int lo, int hiIncl) {
    return lo + (int)(rnd() % (uint32_t)(hiIncl - lo + 1));
}

void seed(uint32_t s) {
    rngState ^= s * 2654435761u;
    if (!rngState) rngState = 0x9E3779B9u;
}

void sleepMs(uint32_t t) { sleep_ms(t); }

}  // namespace hw
