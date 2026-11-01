// Umsetzung der Arduino-Nachbildung auf das Pico SDK (siehe Arduino.h).
// hw.h zuerst: Arduino.h definiert Makros wie UP/LEFT, die sonst die
// Aufzaehlung in hw.h treffen wuerden.
#include "hw.h"

#include "Arduino.h"
#include "U8g2lib.h"

#include "hardware/flash.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

SerialStub Serial;
EEPROMClass EEPROM;

// ---- Tasten, LEDs, Lautsprecher -------------------------------------------
// Die Joystick-Pins sind schon in hw::init() als Eingang mit Pullup gesetzt,
// die LEDs haengen dort an PWM - pinMode aendert daran nichts.
void pinMode(int, int) {}

int digitalRead(int pin) { return gpio_get(pin) ? HIGH : LOW; }

void digitalWrite(int pin, int value) {
    switch (pin) {
        case LEDR: hw::led(hw::LED_RED, value); break;
        case LEDY: hw::led(hw::LED_YELLOW, value); break;
        case LEDG: hw::led(hw::LED_GREEN, value); break;
        case SPEAKER:                       // Knacken per Hand (PICtris, SCLange)
            if (hw::soundOn()) gpio_put(SPEAKER, value);
            break;
    }
}

void analogWrite(int pin, int value) {
    uint8_t v = (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
    switch (pin) {
        case LEDR: hw::ledLevel(hw::LED_RED, v); break;
        case LEDY: hw::ledLevel(hw::LED_YELLOW, v); break;
        case LEDG: hw::ledLevel(hw::LED_GREEN, v); break;
    }
}

void tone(int, unsigned int freq, unsigned long durMs) { hw::beep((uint16_t)freq, (uint16_t)(durMs ? durMs : 1000)); }
void noTone(int) { hw::beep(0, 0); }

// ---- Zeit und Zufall ----------------------------------------------------------
unsigned long millis() { return to_ms_since_boot(get_absolute_time()); }
unsigned long micros() { return (unsigned long)time_us_64(); }
void delay(unsigned long ms) { sleep_ms(ms); }
void delayMicroseconds(unsigned int us) { busy_wait_us_32(us); }

long random(long max) { return max > 0 ? (long)(hw::rnd() % (uint32_t)max) : 0; }
long random(long min, long max) { return max > min ? min + random(max - min) : min; }
void randomSeed(unsigned long seed) { hw::seed((uint32_t)seed); }

// ---- EEPROM im letzten Flash-Sektor -------------------------------------------
namespace {
constexpr uint32_t EE_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
uint8_t ee[FLASH_PAGE_SIZE];
}

void EEPROMClass::begin(size_t) { memcpy(ee, (const void*)(XIP_BASE + EE_OFFSET), sizeof ee); }
uint8_t EEPROMClass::read(int a) { return (a >= 0 && a < (int)sizeof ee) ? ee[a] : 0xFF; }
void EEPROMClass::write(int a, uint8_t v) { if (a >= 0 && a < (int)sizeof ee) ee[a] = v; }

bool EEPROMClass::commit() {
    if (!memcmp(ee, (const void*)(XIP_BASE + EE_OFFSET), sizeof ee)) return true;
    // Waehrend Flash geloescht/beschrieben wird, darf nichts aus dem Flash laufen
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(EE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(EE_OFFSET, ee, sizeof ee);
    restore_interrupts(irq);
    return true;
}

// ---- U8g2 an SPI0 -------------------------------------------------------------
namespace {
uint8_t pinCs, pinDc, pinRst;

uint8_t byteCb(u8x8_t* u8x8, uint8_t msg, uint8_t n, void* data) {
    switch (msg) {
        case U8X8_MSG_BYTE_SEND: spi_write_blocking(spi0, (const uint8_t*)data, n); break;
        case U8X8_MSG_BYTE_INIT: gpio_put(pinCs, 1); break;
        case U8X8_MSG_BYTE_SET_DC: gpio_put(pinDc, n); break;
        case U8X8_MSG_BYTE_START_TRANSFER: gpio_put(pinCs, 0); break;
        case U8X8_MSG_BYTE_END_TRANSFER: gpio_put(pinCs, 1); break;
        default: return 0;
    }
    return 1;
}

uint8_t gpioCb(u8x8_t*, uint8_t msg, uint8_t n, void*) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT: break;       // SPI und Pins richtet gfx::init() ein
        case U8X8_MSG_DELAY_MILLI: sleep_ms(n); break;
        case U8X8_MSG_DELAY_10MICRO: busy_wait_us_32(10u * n); break;
        case U8X8_MSG_DELAY_100NANO: busy_wait_us_32(1); break;
        case U8X8_MSG_GPIO_RESET: gpio_put(pinRst, n); break;
        case U8X8_MSG_GPIO_CS: gpio_put(pinCs, n); break;
        case U8X8_MSG_GPIO_DC: gpio_put(pinDc, n); break;
        default: return 0;
    }
    return 1;
}
}  // namespace

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI::U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI(const u8g2_cb_t* rotation, uint8_t cs, uint8_t dc, uint8_t reset) {
    pinCs = cs; pinDc = dc; pinRst = reset;
    u8g2_Setup_sh1106_128x64_noname_f(&u8g2, rotation, byteCb, gpioCb);
}
