/*
* spi_coordinator.c
* created by: Brad Oraw
* created on: 2025-08-15
*/

#include "spi_coordinator.h"
#include "definitions.h"
#include <string.h>

static spi_device_t current_device = SPI_DEVICE_NONE;
static bool coordinator_initialized = false;

void spi_coordinator_init(void) {
    if (coordinator_initialized) {
        return;
    }

    SYS_CONSOLE_PRINT("spi_coordinator: init\r\n");
    
    // Ensure both chip selects are high (inactive) initially
    CS_FLASH_Set();
    CS_EEPROM_Set();
    
    current_device = SPI_DEVICE_NONE;
    coordinator_initialized = true;
    
    // SYS_CONSOLE_PRINT("spi_coordinator: initialized\r\n");
}

bool spi_coordinator_acquire(spi_device_t device) {
    if (!coordinator_initialized) {
        spi_coordinator_init();
    }
    
    if (current_device != SPI_DEVICE_NONE) {
        SYS_CONSOLE_PRINT("spi_coordinator: SPI2 busy with device %d\r\n", current_device);
        return false;
    }
    
    current_device = device;
    // SYS_CONSOLE_PRINT("spi_coordinator: acquired by device %d\r\n", device);
    return true;
}

void spi_coordinator_release(void) {
    if (current_device != SPI_DEVICE_NONE) {
        // SYS_CONSOLE_PRINT("spi_coordinator: released by device %d\r\n", current_device);
        
        // Ensure both chip selects are high when releasing
        CS_FLASH_Set();
        CS_EEPROM_Set();
        
        current_device = SPI_DEVICE_NONE;
    }
}

spi_device_t spi_coordinator_get_current_device(void) {
    return current_device;
}

bool spi_coordinator_is_available(void) {
    return (current_device == SPI_DEVICE_NONE);
}

void spi_coordinator_cs_flash_low(void) {
    if (current_device == SPI_DEVICE_FLASH) {
        CS_FLASH_Clear();
        // Add a small delay to ensure the pin state has settled
        for (volatile int i = 0; i < 100; i++);
    }
}

void spi_coordinator_cs_flash_high(void) {
    if (current_device == SPI_DEVICE_FLASH) {
        CS_FLASH_Set();
        // Add a small delay to ensure the pin state has settled
        for (volatile int i = 0; i < 100; i++);
    }
}

void spi_coordinator_cs_eeprom_low(void) {
    if (current_device == SPI_DEVICE_EEPROM) {
        CS_EEPROM_Clear();
        // Add a small delay to ensure the pin state has settled
        for (volatile int i = 0; i < 100; i++);
    }
}

void spi_coordinator_cs_eeprom_high(void) {
    if (current_device == SPI_DEVICE_EEPROM) {
        CS_EEPROM_Set();
        // Add a small delay to ensure the pin state has settled
        for (volatile int i = 0; i < 100; i++);
    }
} 