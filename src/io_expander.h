/*
* IO Expander Module
* io_expander.h
* created by: Brad Oraw
* created on: 2025-08-28
*/

#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include <stdint.h>
#include <stdbool.h>

#define TCA9534_ADDRESS 0x20
#define IO_EXPANDER_ALTERNATE_ADDRESS 0x38
#define IO_EXPANDER_NOT_FOUND_ADDRESS 0x00

enum {VCV1_EN, VCC1_EN, VCV1_36V, VCV1_24V, VCV2_24V, VCV2_36V, VCC2_EN, VCV2_EN};

void io_expander_init(void);
void io_expander_set(uint8_t pin, bool value);
uint8_t io_expander_read(uint8_t reg);
void io_expander_write(uint8_t reg, uint8_t val);

#endif