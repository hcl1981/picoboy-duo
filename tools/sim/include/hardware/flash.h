// Attrappe: Flash als RAM-Feld (fuer das EEPROM der PicoPac-Spiele)
#pragma once
#include <stdint.h>
#include <stddef.h>
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_PAGE_SIZE 256u
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2u * FLASH_SECTOR_SIZE)
#endif
extern uint8_t sim_flash[PICO_FLASH_SIZE_BYTES];
#define XIP_BASE ((uintptr_t)sim_flash)
void flash_range_erase(uint32_t off, size_t n);
void flash_range_program(uint32_t off, const uint8_t* d, size_t n);
