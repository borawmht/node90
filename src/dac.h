/*
* DAC Module
* dac.h
* created by: Brad Oraw
* created on: 2025-08-28
*/

#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include <stdbool.h>

//#define MCP4728_ADDRESS 0xC0  // The address of MCP4728 when the AD0-2 are zero
#define MCP4728_ADDRESS 0x60    // 7bit address 
#define DS3911_ADDRESS  0x58    // 7bit address
#define DAC_NOT_FOUND_ADDRESS 0x00

#define WRITE_CHANNEL_A 0x40            // update immediately
#define WRITE_CHANNEL_B 0x42
#define WRITE_CHANNEL_C 0x44
#define WRITE_CHANNEL_D 0x46

void dac_init(void);
void dac_set_channel_level(uint8_t channel, uint8_t level);
void dac_set_010V_level(uint8_t level);
void dac_write(uint8_t reg_address, uint8_t dim_level);

#endif