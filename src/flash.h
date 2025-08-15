/*
* flash.h
* created by: Brad Oraw
* created on: 2025-08-14
*/

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>

// Flash constants
#define FLASH_MAGIC_NUMBER    0x12345678
#define FLASH_MAGIC_OFFSET    0x00000000
#define FLASH_TEST_OFFSET     0x00001000  // 4KB offset for test data
#define FLASH_TEST_SIZE       256         // Test data size

// Function prototypes
void flash_init(void);
void flash_deinit(void);
bool flash_is_initialized(void);

// Basic flash operations
bool flash_read(uint32_t address, uint8_t *data, uint16_t length);
bool flash_write(uint32_t address, const uint8_t *data, uint16_t length);
bool flash_erase_sector(uint32_t address);
bool flash_erase_chip(void);

// Test functions

// Utility functions
uint32_t flash_get_magic(void);
bool flash_set_magic(uint32_t magic);

#endif // FLASH_H 