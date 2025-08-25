/*
* spi_coordinator.h
* created by: Brad Oraw
* created on: 2025-08-15
*/

#ifndef SPI_COORDINATOR_H
#define SPI_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

// SPI device types
typedef enum {
    SPI_DEVICE_FLASH,
    SPI_DEVICE_EEPROM,
    SPI_DEVICE_NONE
} spi_device_t;

// Function prototypes
void spi_coordinator_init(void);
bool spi_coordinator_acquire(spi_device_t device);
void spi_coordinator_release(void);
spi_device_t spi_coordinator_get_current_device(void);
bool spi_coordinator_is_available(void);

// Helper functions for chip select management
void spi_coordinator_cs_flash_low(void);
void spi_coordinator_cs_flash_high(void);
void spi_coordinator_cs_eeprom_low(void);
void spi_coordinator_cs_eeprom_high(void);

#endif // SPI_COORDINATOR_H 