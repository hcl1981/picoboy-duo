// Kleine Nachbildung der Arduino-Klasse Wire fuer HWTest (Pico SDK statt
// Arduino-Core): nur das, was STK8BA58.h benutzt. SDA/SCL 20/21 liegen auf I2C0.
#pragma once
#include <stdint.h>

class TwoWire {
public:
    void setSDA(uint8_t pin) { sda = pin; }
    void setSCL(uint8_t pin) { scl = pin; }
    void begin();
    void beginTransmission(uint8_t addr);
    void write(uint8_t v);
    uint8_t endTransmission();           // 0 = erfolgreich
    uint8_t requestFrom(uint8_t addr, uint8_t n);
    int read();                          // -1, wenn nichts mehr da ist

private:
    uint8_t sda = 20, scl = 21, addr = 0;
    uint8_t tx[8], txLen = 0;
    uint8_t rx[8], rxLen = 0, rxPos = 0;
};

extern TwoWire Wire;
