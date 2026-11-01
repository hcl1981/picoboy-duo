// Attrappe fuer den PC-Simulator: nur was gfx.cpp braucht.
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef unsigned int uint;
enum { GPIO_OUT = 1, GPIO_IN = 0, GPIO_FUNC_SPI = 1, GPIO_FUNC_I2C = 3 };
void gpio_init(uint);
void gpio_set_dir(uint, int);
void gpio_put(uint, int);
void gpio_set_function(uint, int);
void gpio_pull_up(uint);
void sleep_ms(uint32_t);
int gpio_get(uint);
void busy_wait_us_32(uint32_t);
uint64_t time_us_64(void);
typedef uint64_t absolute_time_t;
absolute_time_t get_absolute_time(void);
uint32_t to_ms_since_boot(absolute_time_t);
