// Attrappe: nur was die Wire-Nachbildung braucht (Sensor in sim.cpp)
#pragma once
#include "pico/stdlib.h"
typedef struct i2c_inst i2c_inst_t;
#define i2c0 ((i2c_inst_t*)0)
#define PICO_ERROR_GENERIC (-1)
static inline uint i2c_init(i2c_inst_t*, uint b) { return b; }
int i2c_write_timeout_us(i2c_inst_t*, uint8_t addr, const uint8_t* d, size_t n, bool nostop, uint us);
int i2c_read_timeout_us(i2c_inst_t*, uint8_t addr, uint8_t* d, size_t n, bool nostop, uint us);
