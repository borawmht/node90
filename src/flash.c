/*
* flash.c
* created by: Brad Oraw
* created on: 2025-08-12
*/

#include "flash.h"
#include "definitions.h"
#include <string.h>
#include "spi_coordinator.h"

static bool flash_initialized = false;
static DRV_HANDLE flash_handle = DRV_HANDLE_INVALID;

// Add this function to check SST26 driver details
void flash_diagnostic_info(void) {
    SYS_CONSOLE_PRINT("flash: diagnostic information\r\n");
    
    // Check driver status
    SYS_STATUS status = DRV_SST26_Status(DRV_SST26_INDEX);
    SYS_CONSOLE_PRINT("flash: SST26 driver status: %d\r\n", status);
    
    if (flash_handle != DRV_HANDLE_INVALID) {
        // Try to read JEDEC ID to verify communication
        uint32_t jedec_id = 0;
        if (DRV_SST26_ReadJedecId(flash_handle, &jedec_id)) {
            SYS_CONSOLE_PRINT("flash: JEDEC ID: 0x%08lx\r\n", jedec_id);
        } else {
            SYS_CONSOLE_PRINT("flash: failed to read JEDEC ID\r\n");
        }
        
        // Try to get geometry information
        DRV_SST26_GEOMETRY geometry;
        if (DRV_SST26_GeometryGet(flash_handle, &geometry)) {
            SYS_CONSOLE_PRINT("flash: geometry - read_block: %lu, write_block: %lu, erase_block: %lu\r\n", 
                             geometry.read_blockSize, geometry.write_blockSize, geometry.erase_blockSize);
        } else {
            SYS_CONSOLE_PRINT("flash: failed to get geometry\r\n");
        }
    }
}

// Modify the flash_init function to include diagnostics
void flash_init(void) {
    SYS_CONSOLE_PRINT("flash: init\r\n");
    
    if (flash_initialized) {
        SYS_CONSOLE_PRINT("flash: already initialized\r\n");
        return;
    }
    
    // Initialize SPI coordinator if not already done
    spi_coordinator_init();
    
    // Check if SST26 driver is ready
    // SYS_CONSOLE_PRINT("flash: checking SST26 driver status\r\n");
    if (DRV_SST26_Status(DRV_SST26_INDEX) != SYS_STATUS_READY) {
        SYS_CONSOLE_PRINT("flash: SST26 driver not ready\r\n");
        return;
    }
    
    // Open SST26 driver with read/write intent
    // SYS_CONSOLE_PRINT("flash: opening SST26 driver\r\n");
    flash_handle = DRV_SST26_Open(DRV_SST26_INDEX, DRV_IO_INTENT_READWRITE);
    if (flash_handle == DRV_HANDLE_INVALID) {
        SYS_CONSOLE_PRINT("flash: failed to open SST26 driver\r\n");
        return;
    }
    
    // SYS_CONSOLE_PRINT("flash: SST26 driver opened successfully\r\n");
       
    // Run diagnostics
    // flash_diagnostic_info();
    
    // Set initialized flag BEFORE checking magic number
    flash_initialized = true;
    
    // // Check if flash is initialized with magic number
    uint32_t current_magic = flash_get_magic();
    if (current_magic != FLASH_MAGIC_NUMBER) {
        SYS_CONSOLE_PRINT("flash: initializing magic number\r\n");
        if (!flash_set_magic(FLASH_MAGIC_NUMBER)) {
            SYS_CONSOLE_PRINT("flash: failed to set magic number!\r\n");
            DRV_SST26_Close(flash_handle);
            flash_handle = DRV_HANDLE_INVALID;
            flash_initialized = false;
            return;
        }
        current_magic = flash_get_magic();
        if(current_magic != FLASH_MAGIC_NUMBER){
            SYS_CONSOLE_PRINT("flash: magic number set failed\r\n");
            return;
        }
        // SYS_CONSOLE_PRINT("flash: magic number set successfully");
    } else {
        // SYS_CONSOLE_PRINT("flash: magic number already valid\r\n");
    }
    // SYS_CONSOLE_PRINT("flash: magic number: 0x%08lx\r\n", current_magic);
    
    // SYS_CONSOLE_PRINT("flash: init complete\r\n");
}

void flash_deinit(void) {
    SYS_CONSOLE_PRINT("flash: deinit\r\n");
    
    if (flash_handle != DRV_HANDLE_INVALID) {
        DRV_SST26_Close(flash_handle);
        flash_handle = DRV_HANDLE_INVALID;
    }
    
    flash_initialized = false;
    SYS_CONSOLE_PRINT("flash: deinit complete\r\n");
}

bool flash_is_initialized(void) {
    return flash_initialized && (flash_handle != DRV_HANDLE_INVALID);
}

bool flash_read(uint32_t address, uint8_t *data, uint16_t length) {
    if (!flash_is_initialized() || data == NULL) {
        SYS_CONSOLE_PRINT("flash: read failed - not initialized or invalid params\r\n");
        return false;
    }
    
    // Acquire SPI2 for flash use
    if (!spi_coordinator_acquire(SPI_DEVICE_FLASH)) {
        SYS_CONSOLE_PRINT("flash: failed to acquire SPI2\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("flash: read addr=0x%08lx len=%d\r\n", address, length);
    
    // Clear the buffer first to see if we're actually getting data
    memset(data, 0xFF, length);
    
    // Perform blocking read operation
    if (!DRV_SST26_Read(flash_handle, data, length, address)) {
        SYS_CONSOLE_PRINT("flash: read operation failed\r\n");
        spi_coordinator_release(); // Release on error
        return false;
    }

    while (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_BUSY) {
        // Wait for completion
    }
    
    bool success = (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_COMPLETED);
    
    // Release SPI2
    spi_coordinator_release();
    
    if (success) {
        // SYS_CONSOLE_PRINT("flash: read completed successfully\r\n");
        return true;
    } else {
        SYS_CONSOLE_PRINT("flash: read transfer failed\r\n");
        return false;
    }
}

bool flash_write(uint32_t address, const uint8_t *data, uint16_t length) {
    if (!flash_is_initialized() || data == NULL) {
        SYS_CONSOLE_PRINT("flash: write failed - not initialized or invalid params\r\n");
        return false;
    }
    
    // Acquire SPI2 for flash use
    if (!spi_coordinator_acquire(SPI_DEVICE_FLASH)) {
        SYS_CONSOLE_PRINT("flash: failed to acquire SPI2\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("flash: write addr=0x%08lx len=%d\r\n", address, length);
    
    // Write data page by page (SST26 page size is typically 256 bytes)
    uint32_t current_addr = address;
    uint16_t remaining = length;
    const uint8_t *current_data = data;
    
    while (remaining > 0) {
        // Calculate bytes to write in this page
        uint16_t page_offset = current_addr % 256;  // SST26 page size
        uint16_t bytes_in_page = 256 - page_offset;
        if (bytes_in_page > remaining) {
            bytes_in_page = remaining;
        }
        
        // SYS_CONSOLE_PRINT("flash: writing page at 0x%08lx, %d bytes\r\n", current_addr, bytes_in_page);
        
        // Perform page write operation
        if (!DRV_SST26_PageWrite(flash_handle, (void*)current_data, current_addr)) {
            SYS_CONSOLE_PRINT("flash: page write failed\r\n");
            spi_coordinator_release(); // Release on error
            return false;
        }
        
        // Wait for write to complete
        while (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_BUSY) {
            // Wait for completion
        }
        
        if (DRV_SST26_TransferStatusGet(flash_handle) != DRV_SST26_TRANSFER_COMPLETED) {
            SYS_CONSOLE_PRINT("flash: write transfer failed\r\n");
            spi_coordinator_release(); // Release on error
            return false;
        }
        
        current_addr += bytes_in_page;
        current_data += bytes_in_page;
        remaining -= bytes_in_page;
    }
    
    // Release SPI2
    spi_coordinator_release();
    
    // SYS_CONSOLE_PRINT("flash: write completed successfully\r\n");
    return true;
}

bool flash_erase_sector(uint32_t address) {
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("flash: erase sector failed - not initialized\r\n");
        return false;
    }
    
    // Acquire SPI2 for flash use
    if (!spi_coordinator_acquire(SPI_DEVICE_FLASH)) {
        SYS_CONSOLE_PRINT("flash: failed to acquire SPI2\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("flash: erasing sector at 0x%08lx\r\n", address);
    
    // Perform sector erase (4KB sectors)
    if (!DRV_SST26_SectorErase(flash_handle, address)) {
        SYS_CONSOLE_PRINT("flash: sector erase failed\r\n");
        spi_coordinator_release(); // Release on error
        return false;
    }
    
    // Wait for erase to complete
    while (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_BUSY) {
        // Wait for completion
    }
    
    bool success = (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_COMPLETED);
    
    // Release SPI2
    spi_coordinator_release();
    
    if (success) {
        // SYS_CONSOLE_PRINT("flash: sector erase completed successfully\r\n");
        return true;
    } else {
        SYS_CONSOLE_PRINT("flash: sector erase transfer failed\r\n");
        return false;
    }
}

bool flash_erase_chip(void) {
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("flash: chip erase failed - not initialized\r\n");
        return false;
    }
    
    // Acquire SPI2 for flash use
    if (!spi_coordinator_acquire(SPI_DEVICE_FLASH)) {
        SYS_CONSOLE_PRINT("flash: failed to acquire SPI2\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("flash: erasing entire chip\r\n");
    
    // Perform chip erase
    if (!DRV_SST26_ChipErase(flash_handle)) {
        SYS_CONSOLE_PRINT("flash: chip erase failed\r\n");
        spi_coordinator_release(); // Release on error
        return false;
    }
    
    // Wait for erase to complete
    while (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_BUSY) {
        // Wait for completion
    }
    
    bool success = (DRV_SST26_TransferStatusGet(flash_handle) == DRV_SST26_TRANSFER_COMPLETED);
    
    // Release SPI2
    spi_coordinator_release();
    
    if (success) {
        SYS_CONSOLE_PRINT("flash: chip erase completed successfully\r\n");
        return true;
    } else {
        SYS_CONSOLE_PRINT("flash: chip erase transfer failed\r\n");
        return false;
    }
}

uint32_t flash_get_magic(void) {
    uint32_t magic = 0;
    
    // Only check initialization if we're not in the middle of initialization
    if (!flash_initialized && flash_handle == DRV_HANDLE_INVALID) {
        SYS_CONSOLE_PRINT("flash: magic read failed - not initialized\r\n");
        return 0;
    }
    
    if (!flash_read(FLASH_MAGIC_OFFSET, (uint8_t*)&magic, sizeof(magic))) {
        SYS_CONSOLE_PRINT("flash: magic read failed\r\n");
        return 0;
    }

    // SYS_CONSOLE_PRINT("flash: magic read: 0x%08lx\r\n", magic);
    
    return magic;
}

bool flash_set_magic(uint32_t magic) {
    SYS_CONSOLE_PRINT("flash: setting magic number 0x%08lx\r\n", magic);
    
    // Only check initialization if we're not in the middle of initialization
    if (!flash_initialized && flash_handle == DRV_HANDLE_INVALID) {
        SYS_CONSOLE_PRINT("flash: magic set failed - not initialized\r\n");
        return false;
    }
    
    // Erase the sector containing magic number
    if (!flash_erase_sector(FLASH_MAGIC_OFFSET)) {
        SYS_CONSOLE_PRINT("flash: magic sector erase failed\r\n");
        return false;
    }
    
    // Write magic number
    bool result = flash_write(FLASH_MAGIC_OFFSET, (uint8_t*)&magic, sizeof(magic));
    
    if (result) {
        // SYS_CONSOLE_PRINT("flash: magic write successful\r\n");
        
        // Add a small delay before reading back
        for (volatile int i = 0; i < 10000; i++);
        
        // Read back immediately to verify
        uint32_t verify_magic = 0;
        if (flash_read(FLASH_MAGIC_OFFSET, (uint8_t*)&verify_magic, sizeof(verify_magic))) {
            // SYS_CONSOLE_PRINT("flash: immediate verify read: 0x%08lx\r\n", verify_magic);
            if (verify_magic != magic) {
                SYS_CONSOLE_PRINT("flash: WARNING - immediate verify failed!\r\n");
            }
        }
    } else {
        SYS_CONSOLE_PRINT("flash: magic write failed!\r\n");
    }
    
    return result;
}