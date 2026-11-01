// Kleine Arduino-Nachbildung fuer die PicoPac-Spiele (Pico SDK statt
// Arduino-Core). Enthaelt genau das, was die Spiele benutzen; das Verhalten
// entspricht dem Arduino-Core des RP2040.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

// set_sys_clock_khz(): TABman und Demo stellen den Takt selbst ein (im
// Original ueber Misc/helper.h, hier die gleichnamige SDK-Funktion)
#include "hardware/clocks.h"
#include <cstdlib>

using std::abs;

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

// Pins wie in PicoPac.ino
#define LEDR 5
#define LEDY 6
#define LEDG 7
#define KEY_RIGHT 2
#define KEY_DOWN 3
#define KEY_LEFT 4
#define KEY_UP 1
#define KEY_CENTER 0
#define SPEAKER 15

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
void analogWrite(int pin, int value);

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

void tone(int pin, unsigned int freq, unsigned long durMs = 0);
void noTone(int pin);

#if !defined(__NEWLIB__)
// glibc (PC-Simulator) kennt kein itoa
inline char* itoa(int v, char* s, int base) {
    if (base == 10) sprintf(s, "%d", v);
    else if (base == 16) sprintf(s, "%x", v);
    else s[0] = 0;
    return s;
}
#endif

// Die serielle Schnittstelle gibt es nicht (USB gehoert dem Spiele-Link):
// Ausgaben verschwinden stillschweigend.
struct SerialStub {
    void begin(unsigned long) {}
    template <class T> void print(const T&) {}
    template <class T> void println(const T&) {}
    void println() {}
};
extern SerialStub Serial;

// EEPROM wie beim Arduino-Core: 256 Byte im letzten Flash-Sektor. Gleiche
// Stelle, damit Highscores der Originalfirmware erhalten bleiben.
struct EEPROMClass {
    void begin(size_t size);
    uint8_t read(int addr);
    void write(int addr, uint8_t v);
    bool commit();
};
extern EEPROMClass EEPROM;
