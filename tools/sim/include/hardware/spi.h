#pragma once
#include "pico/stdlib.h"
typedef struct spi_inst spi_inst_t;
#define spi0 ((spi_inst_t*)0)
enum { SPI_CPOL_0 = 0, SPI_CPHA_0 = 0, SPI_MSB_FIRST = 1 };
uint spi_init(spi_inst_t*, uint);
void spi_set_format(spi_inst_t*, uint, int, int, int);
int spi_write_blocking(spi_inst_t*, const uint8_t*, size_t);
