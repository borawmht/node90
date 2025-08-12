/*
* eeprom.h
* created by: Brad Oraw
* created on: 2025-08-12
*/

#ifndef _EEPROM_H
#define _EEPROM_H

#include <stdbool.h>
#include <stdint.h>

// 25LC1024 specifications
#define EEPROM_SIZE             131072  // 128KB
#define EEPROM_PAGE_SIZE        256
#define EEPROM_MAGIC_NUMBER     0x12345678

// EEPROM storage layout
#define EEPROM_MAGIC_OFFSET     0x0000
#define EEPROM_HEADER_SIZE      16
#define EEPROM_DATA_START       0x0010

void eeprom_init(void);
bool eeprom_write(uint32_t address, const uint8_t *data, uint16_t length);
bool eeprom_read(uint32_t address, uint8_t *data, uint16_t length);
bool eeprom_erase_page(uint32_t address);
void eeprom_deinit(void);

// Internal functions for storage system
bool eeprom_is_initialized(void);
uint32_t eeprom_get_magic(void);
bool eeprom_set_magic(uint32_t magic);

#endif