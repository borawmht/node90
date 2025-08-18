/*
* firmware_update.c
* created by: Brad Oraw
* created on: 2025-08-15
*/

#include "firmware_update.h"
#include "definitions.h"
#include "flash.h"
#include "http.h"
#include "peripheral/nvm/plib_nvm.h"  // Add this for NVM_Read
#include "hash_fnv.h"
#include <stdlib.h>
#include <string.h>

// Add these includes at the top of the file
#include "config/default/library/tcpip/tcp.h"
#include "config/default/library/tcpip/dns.h"
#include "config/default/library/tcpip/tcpip.h"
#include "ethernet.h"
#include "third_party/rtos/FreeRTOS/Source/include/FreeRTOS.h"
#include "third_party/rtos/FreeRTOS/Source/include/task.h"

// Structure to store firmware information
typedef struct {
    char app_name[APPLICATION_NAME_SIZE];
    char app_version[APPLICATION_VERSION_SIZE];
    uint32_t file_size;
    uint32_t checksum;
    bool valid;  // Indicates if binary was copied and verified successfully
} firmware_info_t;

// Add this debug function to help troubleshoot
void firmware_update_debug_checksum_calculation(void) {
    SYS_CONSOLE_PRINT("firmware_update: debugging checksum calculation\r\n");
    
    // Test with a small known data set
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t test_size = sizeof(test_data);
    
    // Method 1: Using fnv_32a_hash function
    uint32_t checksum1 = fnv_32a_hash(test_data, test_size);
    SYS_CONSOLE_PRINT("firmware_update: fnv_32a_hash result: 0x%08lx\r\n", checksum1);
    
    // Method 2: Manual FNV-32a implementation
    uint32_t checksum2 = FNV_32_INIT;
    for (uint32_t i = 0; i < test_size; i++) {
        checksum2 ^= (uint32_t)test_data[i];
        checksum2 += (checksum2 << 1) + (checksum2 << 4) + 
                    (checksum2 << 7) + (checksum2 << 8) + 
                    (checksum2 << 24);
    }
    SYS_CONSOLE_PRINT("firmware_update: manual FNV-32a result: 0x%08lx\r\n", checksum2);
    
    if (checksum1 == checksum2) {
        SYS_CONSOLE_PRINT("firmware_update: checksum methods match\r\n");
    } else {
        SYS_CONSOLE_PRINT("firmware_update: checksum methods differ!\r\n");
    }
}

void firmware_update_init(void) {
    SYS_CONSOLE_PRINT("firmware_update: init\r\n");
    // firmware_update_copy_internal_flash_to_external_flash();
    // firmware_update_compare_internal_flash_to_external_flash();
    // firmware_update_debug_checksum_calculation();
    firmware_update_show_version();
}

void firmware_update_show_version(void) {
    SYS_CONSOLE_PRINT("firmware_update: internal: %s %s %s\r\n", 
        firmware_update_get_internal_name(), 
        firmware_update_get_internal_version(),
        firmware_update_get_internal_latest() ? "latest ✓" : "not latest ✗");
    SYS_CONSOLE_PRINT("firmware_update: external: %s %s %s\r\n", 
        firmware_update_get_external_name(), 
        firmware_update_get_external_version(), 
        firmware_update_get_external_valid() ? "valid ✓" : "invalid ✗");
}

bool firmware_update_copy_internal_flash_to_external_flash(void){
    SYS_CONSOLE_PRINT("firmware_update: copy internal flash to external flash\r\n");
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("firmware_update: external flash not initialized\r\n");
        return false;
    }
    
    // Calculate binary size
    uint32_t binary_size = APPLICATION_END_ADDRESS - APPLICATION_START_ADDRESS;
    SYS_CONSOLE_PRINT("firmware_update: binary size: %lu bytes\r\n", binary_size);
    
    // Allocate buffer for binary data (use smaller chunks to save memory)
    uint8_t *binary_buffer = malloc(1024); // Reduced to 1KB chunks
    if (binary_buffer == NULL) {
        SYS_CONSOLE_PRINT("firmware_update: failed to allocate 1KB buffer\r\n");
        // Try even smaller buffer
        binary_buffer = malloc(512); // Try 512 bytes
        if (binary_buffer == NULL) {
            SYS_CONSOLE_PRINT("firmware_update: failed to allocate 512B buffer\r\n");
            return false;
        }
        SYS_CONSOLE_PRINT("firmware_update: using 512B buffer\r\n");
    }
    
    // Read application name from internal flash
    char app_name[APPLICATION_NAME_SIZE];
    if (!NVM_Read((uint32_t*)app_name, APPLICATION_NAME_SIZE, APPLICATION_NAME_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read app name\r\n");
        free(binary_buffer);
        return false;
    }
    
    // Read application version from internal flash
    char app_version[APPLICATION_VERSION_SIZE];
    if (!NVM_Read((uint32_t*)app_version, APPLICATION_VERSION_SIZE, APPLICATION_VERSION_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read app version\r\n");
        free(binary_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: app name: %s\r\n", app_name);
    SYS_CONSOLE_PRINT("firmware_update: app version: %s\r\n", app_version);
    
    // Calculate checksum of binary data using FNV-32a (same as compare function)
    uint32_t checksum = FNV_32_INIT;
    uint32_t current_addr = APPLICATION_START_ADDRESS;
    uint32_t remaining = binary_size;
    
    while (remaining > 0) {
        uint32_t max_chunk = (binary_buffer != NULL) ? 1024 : 512; // Use actual allocated size
        uint32_t chunk_size = (remaining > max_chunk) ? max_chunk : remaining;
        
        // Read chunk from internal flash
        if (!NVM_Read((uint32_t*)binary_buffer, chunk_size, current_addr)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read binary chunk at 0x%08lx\r\n", current_addr);
            free(binary_buffer);
            return false;
        }
        
        // Update checksum using FNV-32a algorithm (same as compare function)
        for (uint32_t i = 0; i < chunk_size; i++) {
            checksum ^= (uint32_t)binary_buffer[i];
            checksum += (checksum << 1) + (checksum << 4) + 
                       (checksum << 7) + (checksum << 8) + 
                       (checksum << 24);
        }
        
        current_addr += chunk_size;
        remaining -= chunk_size;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: calculated checksum: 0x%08lx\r\n", checksum);
    
    // Prepare firmware info structure
    firmware_info_t fw_info;
    memset(&fw_info, 0, sizeof(fw_info));  // Clear entire structure
    strncpy(fw_info.app_name, app_name, APPLICATION_NAME_SIZE - 1);
    strncpy(fw_info.app_version, app_version, APPLICATION_VERSION_SIZE - 1);
    fw_info.file_size = binary_size;
    fw_info.checksum = checksum;
    fw_info.valid = false;  // Will be set to true after successful copy and verification
    
    // Erase the info sector in external flash
    if (!flash_erase_sector(FLASH_INFO_OFFSET)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to erase info sector\r\n");
        free(binary_buffer);
        return false;
    }
    
    // Write firmware info to external flash (initially with valid=false)
    if (!flash_write(FLASH_INFO_OFFSET, (uint8_t*)&fw_info, sizeof(fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to write firmware info\r\n");
        free(binary_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: firmware info written successfully\r\n");
    
    // Verify the firmware info was written correctly
    firmware_info_t verify_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&verify_fw_info, sizeof(verify_fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read back firmware info for verification\r\n");
        free(binary_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: verifying firmware info - valid flag: %s\r\n", verify_fw_info.valid ? "true" : "false");
    
    // Pre-erase all sectors needed for the binary
    uint32_t sector_size = 4096; // 4KB sectors
    uint32_t total_sectors = (binary_size + sector_size - 1) / sector_size; // Round up
    SYS_CONSOLE_PRINT("firmware_update: erasing %lu sectors for binary data\r\n", total_sectors);
    
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        uint32_t sector_addr = FLASH_BINARY_OFFSET + (sector * sector_size);
        SYS_CONSOLE_PRINT("firmware_update: erasing sector %lu at 0x%08lx\r\n", sector, sector_addr);
        
        if (!flash_erase_sector(sector_addr)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to erase sector %lu\r\n", sector);
            free(binary_buffer);
            return false;
        }
    }
    
    SYS_CONSOLE_PRINT("firmware_update: all sectors erased successfully\r\n");
    
    // Write binary data to external flash in chunks with verify and retry
    current_addr = APPLICATION_START_ADDRESS;
    remaining = binary_size;
    uint32_t external_addr = FLASH_BINARY_OFFSET;
    uint32_t total_copied = 0;
    const uint32_t max_retries = 3;
    
    while (remaining > 0) {
        uint32_t max_chunk = (binary_buffer != NULL) ? 1024 : 512; // Use actual allocated size
        uint32_t chunk_size = (remaining > max_chunk) ? max_chunk : remaining;
        bool chunk_verified = false;
        uint32_t chunk_retries = 0;
        
        while (!chunk_verified && chunk_retries < max_retries) {
            // Read chunk from internal flash
            if (!NVM_Read((uint32_t*)binary_buffer, chunk_size, current_addr)) {
                SYS_CONSOLE_PRINT("firmware_update: failed to read binary chunk at 0x%08lx\r\n", current_addr);
                free(binary_buffer);
                return false;
            }
            
            // Write chunk to external flash
            if (!flash_write(external_addr, binary_buffer, chunk_size)) {
                SYS_CONSOLE_PRINT("firmware_update: failed to write binary chunk to external flash (retry %lu)\r\n", chunk_retries + 1);
                chunk_retries++;
                continue;
            }
            
            // Verify the written chunk
            uint8_t *verify_buffer = malloc(chunk_size);
            if (verify_buffer == NULL) {
                SYS_CONSOLE_PRINT("firmware_update: failed to allocate verify buffer\r\n");
                free(binary_buffer);
                return false;
            }
            
            if (!flash_read(external_addr, verify_buffer, chunk_size)) {
                SYS_CONSOLE_PRINT("firmware_update: failed to read back chunk for verification (retry %lu)\r\n", chunk_retries + 1);
                free(verify_buffer);
                chunk_retries++;
                continue;
            }
            
            // Compare written data with original
            bool verify_match = true;
            for (uint32_t i = 0; i < chunk_size; i++) {
                if (binary_buffer[i] != verify_buffer[i]) {
                    SYS_CONSOLE_PRINT("firmware_update: verification failed at offset %lu: expected=0x%02x, got=0x%02x (retry %lu)\r\n", 
                                     total_copied + i, binary_buffer[i], verify_buffer[i], chunk_retries + 1);
                    verify_match = false;
                    break;
                }
            }
            
            free(verify_buffer);
            
            if (verify_match) {
                chunk_verified = true;
                SYS_CONSOLE_PRINT("firmware_update: chunk verified successfully\r\n");
            } else {
                chunk_retries++;
                SYS_CONSOLE_PRINT("firmware_update: chunk verification failed, retrying...\r\n");
            }
        }
        
        if (!chunk_verified) {
            SYS_CONSOLE_PRINT("firmware_update: failed to write and verify chunk after %lu retries\r\n", max_retries);
            free(binary_buffer);
            return false;
        }
        
        current_addr += chunk_size;
        external_addr += chunk_size;
        remaining -= chunk_size;
        total_copied += chunk_size;
        
        SYS_CONSOLE_PRINT("firmware_update: copied and verified %lu bytes, %lu remaining\r\n", chunk_size, remaining);
    }
    
    free(binary_buffer);
    
    // Update firmware info to mark as valid
    fw_info.valid = true;
    SYS_CONSOLE_PRINT("firmware_update: updating firmware info valid flag to true\r\n");
    
    // We need to erase the info sector again before updating the valid flag
    if (!flash_erase_sector(FLASH_INFO_OFFSET)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to erase info sector for valid flag update\r\n");
        return false;
    }
    
    if (!flash_write(FLASH_INFO_OFFSET, (uint8_t*)&fw_info, sizeof(fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to update firmware info as valid\r\n");
        return false;
    }
    
    // Verify the update was successful
    firmware_info_t final_verify_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&final_verify_fw_info, sizeof(final_verify_fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read back final firmware info\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: final firmware info - valid flag: %s\r\n", final_verify_fw_info.valid ? "true" : "false");
    
    if (!final_verify_fw_info.valid) {
        SYS_CONSOLE_PRINT("firmware_update: WARNING - valid flag was not set correctly!\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: copy completed successfully with verification\r\n");
    return true;
}

bool firmware_update_copy_external_flash_to_internal_flash(void){
    SYS_CONSOLE_PRINT("firmware_update: copy external flash to internal flash\r\n");
    return true;
}

bool firmware_update_compare_internal_flash_to_external_flash(void){
    SYS_CONSOLE_PRINT("firmware_update: compare internal flash to external flash\r\n");
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("firmware_update: external flash not initialized\r\n");
        return false;
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read external firmware info\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: external app name: %s\r\n", external_fw_info.app_name);
    SYS_CONSOLE_PRINT("firmware_update: external app version: %s\r\n", external_fw_info.app_version);
    SYS_CONSOLE_PRINT("firmware_update: external file size: %lu bytes\r\n", external_fw_info.file_size);
    SYS_CONSOLE_PRINT("firmware_update: external checksum: 0x%08lx\r\n", external_fw_info.checksum);
    SYS_CONSOLE_PRINT("firmware_update: external valid flag: %s\r\n", external_fw_info.valid ? "true" : "false");
    
    // Check if the firmware info indicates a valid copy
    if (!external_fw_info.valid) {
        SYS_CONSOLE_PRINT("firmware_update: external firmware info indicates invalid/corrupted copy\r\n");
        return false;
    }
    
    // Read application name from internal flash
    char internal_app_name[APPLICATION_NAME_SIZE];
    if (!NVM_Read((uint32_t*)internal_app_name, APPLICATION_NAME_SIZE, APPLICATION_NAME_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read internal app name\r\n");
        return false;
    }
    
    // Read application version from internal flash
    char internal_app_version[APPLICATION_VERSION_SIZE];
    if (!NVM_Read((uint32_t*)internal_app_version, APPLICATION_VERSION_SIZE, APPLICATION_VERSION_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read internal app version\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: internal app name: %s\r\n", internal_app_name);
    SYS_CONSOLE_PRINT("firmware_update: internal app version: %s\r\n", internal_app_version);
    
    // Compare application name
    if (strncmp(internal_app_name, external_fw_info.app_name, APPLICATION_NAME_SIZE) != 0) {
        SYS_CONSOLE_PRINT("firmware_update: app name mismatch\r\n");
        return false;
    }
    
    // Compare application version
    if (strncmp(internal_app_version, external_fw_info.app_version, APPLICATION_VERSION_SIZE) != 0) {
        SYS_CONSOLE_PRINT("firmware_update: app version mismatch\r\n");
        return false;
    }
    
    // Calculate expected binary size
    uint32_t expected_binary_size = APPLICATION_END_ADDRESS - APPLICATION_START_ADDRESS;
    
    // Compare file size
    if (external_fw_info.file_size != expected_binary_size) {
        SYS_CONSOLE_PRINT("firmware_update: file size mismatch - expected: %lu, got: %lu\r\n", 
                         expected_binary_size, external_fw_info.file_size);
        return false;
    }
    
    // Allocate buffers for comparison
    uint8_t *internal_buffer = malloc(512);
    uint8_t *external_buffer = malloc(512);
    if (internal_buffer == NULL || external_buffer == NULL) {
        SYS_CONSOLE_PRINT("firmware_update: failed to allocate comparison buffers\r\n");
        if (internal_buffer) free(internal_buffer);
        if (external_buffer) free(external_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: starting byte-by-byte comparison\r\n");
    
    // STEP 1: Perform byte-by-byte comparison of binary data
    uint32_t current_addr = APPLICATION_START_ADDRESS;
    uint32_t remaining = expected_binary_size;
    uint32_t external_addr = FLASH_BINARY_OFFSET;
    uint32_t bytes_compared = 0;
    bool data_mismatch = false;
    
    while (remaining > 0 && !data_mismatch) {
        uint32_t chunk_size = (remaining > 512) ? 512 : remaining;
        
        // Read chunk from internal flash
        if (!NVM_Read((uint32_t*)internal_buffer, chunk_size, current_addr)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read internal binary chunk at 0x%08lx\r\n", current_addr);
            free(internal_buffer);
            free(external_buffer);
            return false;
        }
        
        // Read chunk from external flash
        if (!flash_read(external_addr, external_buffer, chunk_size)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read external binary chunk at 0x%08lx\r\n", external_addr);
            free(internal_buffer);
            free(external_buffer);
            return false;
        }
        
        // Debug: Show first chunk data
        if (bytes_compared == 0) {
            SYS_CONSOLE_PRINT("firmware_update: first 16 bytes comparison:\r\n");
            SYS_CONSOLE_PRINT("firmware_update: internal: ");
            for (int i = 0; i < 16 && i < chunk_size; i++) {
                SYS_CONSOLE_PRINT("0x%02x ", internal_buffer[i]);
            }
            SYS_CONSOLE_PRINT("\r\n");
            SYS_CONSOLE_PRINT("firmware_update: external: ");
            for (int i = 0; i < 16 && i < chunk_size; i++) {
                SYS_CONSOLE_PRINT("0x%02x ", external_buffer[i]);
            }
            SYS_CONSOLE_PRINT("\r\n");
        }
        
        // Compare chunks using manual byte-by-byte comparison
        bool chunk_mismatch = false;
        uint32_t mismatch_offset = 0;
        
        for (uint32_t i = 0; i < chunk_size; i++) {
            if (internal_buffer[i] != external_buffer[i]) {
                chunk_mismatch = true;
                mismatch_offset = i;
                break;
            }
        }
        
        if (chunk_mismatch) {
            SYS_CONSOLE_PRINT("firmware_update: binary data mismatch at offset %lu (chunk offset %lu)\r\n", 
                             bytes_compared + mismatch_offset, mismatch_offset);
            SYS_CONSOLE_PRINT("firmware_update: mismatch at byte %lu: internal=0x%02x, external=0x%02x\r\n", 
                             bytes_compared + mismatch_offset, 
                             internal_buffer[mismatch_offset], 
                             external_buffer[mismatch_offset]);
            data_mismatch = true;
        }
        
        current_addr += chunk_size;
        external_addr += chunk_size;
        remaining -= chunk_size;
        bytes_compared += chunk_size;
        
        if (bytes_compared % 16384 == 0) { // Show progress every 16KB
            SYS_CONSOLE_PRINT("firmware_update: compared %lu bytes, %lu remaining\r\n", bytes_compared, remaining);
        }
    }
    
    if (data_mismatch) {
        SYS_CONSOLE_PRINT("firmware_update: byte-by-byte comparison FAILED\r\n");
        free(internal_buffer);
        free(external_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: byte-by-byte comparison PASSED - all data matches\r\n");
    
    // STEP 2: Now compare checksums
    SYS_CONSOLE_PRINT("firmware_update: starting checksum comparison\r\n");
    
    // Calculate checksum of internal binary data using FNV-32a
    uint32_t internal_checksum = FNV_32_INIT;
    current_addr = APPLICATION_START_ADDRESS;
    remaining = expected_binary_size;
    
    while (remaining > 0) {
        uint32_t chunk_size = (remaining > 512) ? 512 : remaining;
        
        // Read chunk from internal flash
        if (!NVM_Read((uint32_t*)internal_buffer, chunk_size, current_addr)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read internal binary chunk for checksum\r\n");
            free(internal_buffer);
            free(external_buffer);
            return false;
        }
        
        // Update checksum using FNV-32a algorithm
        for (uint32_t i = 0; i < chunk_size; i++) {
            internal_checksum ^= (uint32_t)internal_buffer[i];
            internal_checksum += (internal_checksum << 1) + (internal_checksum << 4) + 
                               (internal_checksum << 7) + (internal_checksum << 8) + 
                               (internal_checksum << 24);
        }
        
        current_addr += chunk_size;
        remaining -= chunk_size;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: internal checksum: 0x%08lx\r\n", internal_checksum);
    SYS_CONSOLE_PRINT("firmware_update: external checksum: 0x%08lx\r\n", external_fw_info.checksum);
    
    // Compare checksums
    if (internal_checksum != external_fw_info.checksum) {
        SYS_CONSOLE_PRINT("firmware_update: checksum comparison FAILED\r\n");
        SYS_CONSOLE_PRINT("firmware_update: checksum mismatch - internal: 0x%08lx, external: 0x%08lx\r\n", 
                         internal_checksum, external_fw_info.checksum);
        free(internal_buffer);
        free(external_buffer);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: checksum comparison PASSED\r\n");
    
    free(internal_buffer);
    free(external_buffer);
    
    SYS_CONSOLE_PRINT("firmware_update: comparison completed successfully - all data and checksums match\r\n");
    return true;
}

extern char request[REQUEST_SIZE];
extern uint8_t response_buffer[RESPONSE_SIZE];

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Add these buffer management variables at the top of the download function
uint8_t *download_buffer = response_buffer; // Use first 1024 bytes for download buffering
uint32_t download_buffer_size = 1024; // First half of response_buffer
uint32_t download_buffer_used = 0; // How much data is currently in the buffer
uint32_t flash_write_address = FLASH_BINARY_OFFSET; // Next address to write to flash

// Helper function to flush buffer to flash when we have enough data
static bool flush_download_buffer_to_flash(uint8_t *buffer, uint32_t buffer_size, uint32_t *buffer_used, uint32_t *flash_addr) {
    if (*buffer_used == 0) {
        return true; // Nothing to flush
    }
    
    // Calculate how many complete 256-byte pages we can write
    uint32_t complete_pages = *buffer_used / 256;
    uint32_t bytes_to_write = complete_pages * 256;
    
    if (bytes_to_write > 0) {
        SYS_CONSOLE_PRINT("firmware_update: flushing %lu bytes (%lu complete pages) to flash at 0x%08lx\r\n", 
                         bytes_to_write, complete_pages, *flash_addr);
        
        // Write complete pages to flash
        if (!flash_write(*flash_addr, buffer, bytes_to_write)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to write buffered data to flash\r\n");
            return false;
        }
        
        // Verify the written data
        uint8_t *verify_buffer = response_buffer + download_buffer_size;
        if (bytes_to_write > download_buffer_size) {
            SYS_CONSOLE_PRINT("firmware_update: buffered data too large for verification buffer\r\n");
            return false;
        }
        
        if (!flash_read(*flash_addr, verify_buffer, bytes_to_write)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read back buffered data for verification\r\n");
            return false;
        }
        
        // Compare written data with original
        bool verify_match = true;
        for (uint32_t i = 0; i < bytes_to_write; i++) {
            if (buffer[i] != verify_buffer[i]) {
                SYS_CONSOLE_PRINT("firmware_update: buffered data verification failed at offset %lu: expected=0x%02x, got=0x%02x\r\n", 
                                 i, buffer[i], verify_buffer[i]);
                verify_match = false;
                break;
            }
        }
        
        if (!verify_match) {
            SYS_CONSOLE_PRINT("firmware_update: buffered data verification failed\r\n");
            return false;
        }
        
        SYS_CONSOLE_PRINT("firmware_update: buffered data verified successfully\r\n");
        
        // Move remaining data to the beginning of the buffer
        uint32_t remaining = *buffer_used - bytes_to_write;
        if (remaining > 0) {
            memmove(buffer, buffer + bytes_to_write, remaining);
        }
        
        *buffer_used = remaining;
        *flash_addr += bytes_to_write;
    }
    
    return true;
}

bool firmware_update_download_binary_to_external_flash(const char *url) {
    SYS_CONSOLE_PRINT("firmware_update: download binary to external flash from %s\r\n", url);
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("firmware_update: external flash not initialized\r\n");
        return false;
    }
    
    // Check if we have network connectivity
    if (!ethernet_is_ready()) {
        SYS_CONSOLE_PRINT("firmware_update: network not ready\r\n");
        return false;
    }
    
    // Parse URL using shared helper function
    char hostname[64];
    uint16_t port;
    char path[128];
    bool is_https;
    
    if (!parse_url(url, hostname, &port, path, &is_https)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to parse URL\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: hostname=%s, port=%d, path=%s, https=%s\r\n", 
                     hostname, port, path, is_https ? "yes" : "no");
    
    // For HTTPS, we would need to implement HTTPS download
    // For now, only support HTTP
    if (is_https) {
        SYS_CONSOLE_PRINT("firmware_update: HTTPS not yet supported for binary download\r\n");
        return false;
    }
    
    // Resolve hostname to IP address using shared helper function
    IPV4_ADDR server_ip;
    if (!resolve_hostname(hostname, &server_ip)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to resolve hostname\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: connecting to %d.%d.%d.%d:%d\r\n", 
                     server_ip.v[0], server_ip.v[1], server_ip.v[2], server_ip.v[3], port);
    
    // Create direct TCP socket
    TCP_SOCKET tcp_socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, port, NULL);
    if (tcp_socket == INVALID_SOCKET) {
        SYS_CONSOLE_PRINT("firmware_update: failed to create TCP socket\r\n");
        return false;
    }
    
    // Bind remote address
    IP_MULTI_ADDRESS remote_addr;
    remote_addr.v4Add = server_ip;
    
    if (!TCPIP_TCP_RemoteBind(tcp_socket, IP_ADDRESS_TYPE_IPV4, port, &remote_addr)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to bind remote address\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Connect using direct TCP
    if (!TCPIP_TCP_Connect(tcp_socket)) {
        SYS_CONSOLE_PRINT("firmware_update: TCP connect failed\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Wait for connection to be established
    uint32_t connect_timeout = 0;
    while (!TCPIP_TCP_IsConnected(tcp_socket) && connect_timeout < 3000) {
        vTaskDelay(1);
        connect_timeout++;
        
        if (connect_timeout % 1000 == 0) {
            SYS_CONSOLE_PRINT("firmware_update: waiting for TCP connection... %d\r\n", connect_timeout / 1000);
        }
    }
    
    if (!TCPIP_TCP_IsConnected(tcp_socket)) {
        SYS_CONSOLE_PRINT("firmware_update: TCP connection timeout\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    SYS_CONSOLE_PRINT("firmware_update: TCP connection established!\r\n");
    
    // Build HTTP GET request
    snprintf(request, sizeof(request),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, hostname);
    
    SYS_CONSOLE_PRINT("firmware_update: sending request:\r\n%s\r\n", request);
    
    // Send HTTP request
    const char *request_ptr = request;
    size_t request_len = strlen(request);
    size_t sent_total = 0;
    
    while (sent_total < request_len) {
        uint16_t write_ready = TCPIP_TCP_PutIsReady(tcp_socket);
        if (write_ready == 0) {
            SYS_CONSOLE_PRINT("firmware_update: socket not ready for writing\r\n");
            TCPIP_TCP_Close(tcp_socket);
            return false;
        }
        
        size_t to_send = (write_ready < (request_len - sent_total)) ? write_ready : (request_len - sent_total);
        uint16_t sent = TCPIP_TCP_ArrayPut(tcp_socket, (const uint8_t*)(request_ptr + sent_total), to_send);
        if (sent == 0) {
            SYS_CONSOLE_PRINT("firmware_update: failed to send data chunk\r\n");
            TCPIP_TCP_Close(tcp_socket);
            return false;
        }
        
        sent_total += sent;
        SYS_CONSOLE_PRINT("firmware_update: sent chunk %d bytes, total %d/%d\r\n", sent, sent_total, request_len);
        vTaskDelay(1);
    }
    
    SYS_CONSOLE_PRINT("firmware_update: request sent completely\r\n");
    
    // Wait a moment for data to be transmitted
    vTaskDelay(10);
    
    // Allocate buffer for receiving data
    uint8_t *receive_buffer = response_buffer;
    size_t receive_buffer_size = RESPONSE_SIZE;
    
    // Variables for tracking download
    uint32_t total_downloaded = 0;
    uint32_t flash_address = FLASH_BINARY_OFFSET;
    bool headers_received = false;
    bool download_complete = false;
    uint32_t content_length = 0;
    uint32_t receive_timeout = 0;
    
    // First, receive and parse headers
    uint8_t * header_buffer = response_buffer;
    size_t header_buffer_size = RESPONSE_SIZE;
    uint16_t header_received = 0;
    bool headers_complete = false;
    
    SYS_CONSOLE_PRINT("firmware_update: receiving headers...\r\n");
    
    while (!headers_complete && header_received < header_buffer_size - 1 && receive_timeout < 5000) {
        uint16_t available = TCPIP_TCP_GetIsReady(tcp_socket);
        if (available > 0) {
            uint16_t to_read = (available > header_buffer_size - header_received - 1) ? 
                              (header_buffer_size - header_received - 1) : available;
            uint16_t read = TCPIP_TCP_ArrayGet(tcp_socket, header_buffer + header_received, to_read);
            header_received += read;
            
            // Check for end of headers (double CRLF)
            header_buffer[header_received] = '\0';
            const char *body_start = strstr((char*)header_buffer, "\r\n\r\n");
            if (body_start) {
                headers_complete = true;
                headers_received = true;
                
                // Parse Content-Length header
                const char *content_length_header = strstr((char*)header_buffer, "Content-Length:");
                if (content_length_header) {
                    content_length = atoi(content_length_header + 15);
                    SYS_CONSOLE_PRINT("firmware_update: Content-Length: %lu bytes\r\n", content_length);
                } else {
                    SYS_CONSOLE_PRINT("firmware_update: No Content-Length header found\r\n");
                }
                
                // Check for Transfer-Encoding header
                const char *transfer_encoding_header = strstr((char*)header_buffer, "Transfer-Encoding:");
                if (transfer_encoding_header) {
                    SYS_CONSOLE_PRINT("firmware_update: Transfer-Encoding header found\r\n");
                    // For chunked transfer encoding, we would need special handling
                    if (strstr(transfer_encoding_header, "chunked")) {
                        SYS_CONSOLE_PRINT("firmware_update: WARNING - chunked transfer encoding not supported\r\n");
                    }
                }
                
                // Check for Connection header
                const char *connection_header = strstr((char*)header_buffer, "Connection:");
                if (connection_header) {
                    SYS_CONSOLE_PRINT("firmware_update: Connection header found\r\n");
                    if (strstr(connection_header, "close")) {
                        SYS_CONSOLE_PRINT("firmware_update: Connection: close - server will close connection after response\r\n");
                    }
                }
                
                // Debug: Print first few lines of headers for troubleshooting
                SYS_CONSOLE_PRINT("firmware_update: HTTP headers received:\r\n");
                char *header_line = strtok((char*)header_buffer, "\r\n");
                int line_count = 0;
                while (header_line && line_count < 5) { // Print first 5 lines
                    SYS_CONSOLE_PRINT("firmware_update:   %s\r\n", header_line);
                    header_line = strtok(NULL, "\r\n");
                    line_count++;
                }
                
                // Parse HTTP status code using shared helper function
                int status_code = 0;
                if (parse_http_response_status((char*)header_buffer, &status_code)) {
                    SYS_CONSOLE_PRINT("firmware_update: HTTP status: %d\r\n", status_code);
                    if (status_code != 200) {
                        SYS_CONSOLE_PRINT("firmware_update: HTTP error status %d\r\n", status_code);
                        TCPIP_TCP_Close(tcp_socket);
                        return false;
                    }
                }
                
                // Calculate body start position
                size_t headers_size = body_start - (char*)header_buffer + 4;
                size_t body_data_size = header_received - headers_size;
                
                SYS_CONSOLE_PRINT("firmware_update: headers size: %d, body data in header: %d\r\n", 
                                 headers_size, body_data_size);
                
                // 1. Prepare firmware info structure (initially with valid=false)
                firmware_info_t fw_info;
                memset(&fw_info, 0, sizeof(fw_info));
                strncpy(fw_info.app_name, "unassigned", APPLICATION_NAME_SIZE - 1);
                strncpy(fw_info.app_version, "unassigned", APPLICATION_VERSION_SIZE - 1);
                fw_info.file_size = content_length > 0 ? content_length : 0; // Will be updated after download
                fw_info.checksum = 0; // Will be calculated after download
                fw_info.valid = false; // Will be set to true after successful download and verification
                
                // 2. Erase the info sector in external flash
                SYS_CONSOLE_PRINT("firmware_update: erasing info sector at 0x%08lx\r\n", FLASH_INFO_OFFSET);
                if (!flash_erase_sector(FLASH_INFO_OFFSET)) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to erase info sector\r\n");
                    TCPIP_TCP_Close(tcp_socket);
                    return false;
                }
                
                // 3. Write firmware info to external flash (initially with valid=false)
                if (!flash_write(FLASH_INFO_OFFSET, (uint8_t*)&fw_info, sizeof(fw_info))) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to write firmware info\r\n");
                    TCPIP_TCP_Close(tcp_socket);
                    return false;
                }
                
                SYS_CONSOLE_PRINT("firmware_update: firmware info written successfully\r\n");
                
                // 4. Verify the firmware info was written correctly
                firmware_info_t verify_fw_info;
                if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&verify_fw_info, sizeof(verify_fw_info))) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to read back firmware info for verification\r\n");
                    TCPIP_TCP_Close(tcp_socket);
                    return false;
                }
                
                SYS_CONSOLE_PRINT("firmware_update: verifying firmware info - valid flag: %s\r\n", verify_fw_info.valid ? "true" : "false");
                
                // 5. Pre-erase all sectors needed for the binary (SAME AS COPY FUNCTION)
                uint32_t binary_size = APPLICATION_END_ADDRESS - APPLICATION_START_ADDRESS; // Use same calculation as copy function
                uint32_t sector_size = 4096; // 4KB sectors
                uint32_t total_sectors = (binary_size + sector_size - 1) / sector_size; // Round up
                SYS_CONSOLE_PRINT("firmware_update: erasing %lu sectors for binary data (binary size: %lu bytes)\r\n", total_sectors, binary_size);
                
                for (uint32_t sector = 0; sector < total_sectors; sector++) {
                    uint32_t sector_addr = FLASH_BINARY_OFFSET + (sector * sector_size);
                    SYS_CONSOLE_PRINT("firmware_update: erasing sector %lu at 0x%08lx\r\n", sector, sector_addr);
                    
                    if (!flash_erase_sector(sector_addr)) {
                        SYS_CONSOLE_PRINT("firmware_update: failed to erase sector %lu\r\n", sector);
                        TCPIP_TCP_Close(tcp_socket);
                        return false;
                    }
                }
                
                SYS_CONSOLE_PRINT("firmware_update: all sectors erased successfully\r\n");
                
                // 6. Initialize download tracking
                uint32_t total_downloaded = 0;
                uint32_t flash_address = FLASH_BINARY_OFFSET; // Start at binary offset
                bool download_complete = false;
                uint32_t receive_timeout = 0;

                // Initialize the buffering system variables
                download_buffer_used = 0; // Reset buffer
                flash_write_address = FLASH_BINARY_OFFSET; // Start at binary offset

                // Show expected binary size for reference
                uint32_t expected_binary_size = APPLICATION_END_ADDRESS - APPLICATION_START_ADDRESS;
                SYS_CONSOLE_PRINT("firmware_update: expected binary size: %lu bytes\r\n", expected_binary_size);

                // HANDLE INITIAL BODY DATA BY PUTTING IT IN THE BUFFER FIRST
                if (body_data_size > 0) {
                    SYS_CONSOLE_PRINT("firmware_update: putting %d bytes of initial body data into buffer\r\n", body_data_size);
                    
                    // Get pointer to the body data in the header buffer
                    uint8_t *body_data = header_buffer + headers_size;
                    
                    // Copy initial body data to the download buffer
                    if (body_data_size <= download_buffer_size) {
                        memcpy(download_buffer, body_data, body_data_size);
                        download_buffer_used = body_data_size;
                        total_downloaded = body_data_size;
                        
                        SYS_CONSOLE_PRINT("firmware_update: initial body data added to buffer: %d bytes\r\n", body_data_size);
                    } else {
                        SYS_CONSOLE_PRINT("firmware_update: initial body data too large for buffer\r\n");
                        TCPIP_TCP_Close(tcp_socket);
                        return false;
                    }
                }

                SYS_CONSOLE_PRINT("firmware_update: headers received, starting body download...\r\n");
                
                // 7. Download the remaining body data with verification and retry logic
                receive_timeout = 0;
                const uint32_t max_retries = 3;
                uint32_t no_data_timeout = 0; // Track time without receiving data
                const uint32_t max_no_data_timeout = 5000; // 5 seconds without data
                
                while (receive_timeout < 30000 && !download_complete) { // 30 second timeout for body
                    uint16_t available = TCPIP_TCP_GetIsReady(tcp_socket);
                    
                    // Process any new data from the network
                    if (available > 0) {
                        uint16_t to_read = MIN(available, download_buffer_size - download_buffer_used);
                        
                        if (to_read > 0) {
                            uint16_t read = TCPIP_TCP_ArrayGet(tcp_socket, download_buffer + download_buffer_used, to_read);
                            
                            if (read > 0) {
                                download_buffer_used += read;
                                total_downloaded += read;
                                
                                SYS_CONSOLE_PRINT("firmware_update: received %d bytes, buffer now has %lu bytes\r\n", 
                                                 read, download_buffer_used);
                                
                                // Try to flush buffer to flash if we have enough data
                                if (!flush_download_buffer_to_flash(download_buffer, download_buffer_size, 
                                                                   &download_buffer_used, &flash_write_address)) {
                                    SYS_CONSOLE_PRINT("firmware_update: failed to flush buffer to flash\r\n");
                                    TCPIP_TCP_Close(tcp_socket);
                                    return false;
                                }
                                
                                // debug what has been downloaded for the first 5120 bytes
                                if(total_downloaded < 5120){
                                    SYS_CONSOLE_PRINT("firmware_update: debug: first %d bytes of downloaded data:\r\n", read);
                                    for(int i = 0; i < read; i++){
                                        SYS_CONSOLE_PRINT("%c", download_buffer[download_buffer_used - read + i]);
                                    }
                                    SYS_CONSOLE_PRINT("\r\n");

                                    for (int i = 0; i < read - 5; i++) {
                                        if (download_buffer[download_buffer_used - read + i] == 'n' && 
                                            download_buffer[download_buffer_used - read + i+1] == 'o' && 
                                            download_buffer[download_buffer_used - read + i+2] == 'd' && 
                                            download_buffer[download_buffer_used - read + i+3] == 'e' && 
                                            download_buffer[download_buffer_used - read + i+4] == '9' && 
                                            download_buffer[download_buffer_used - read + i+5] == '0') {
                                            uint32_t name_offset = total_downloaded - read + i;
                                            SYS_CONSOLE_PRINT("firmware_update: found 'node90' at offset %lu (expected: 4128)\r\n", name_offset);
                                        }
                                    }
                                    
                                    for (int i = 0; i < read - 4; i++) {
                                        if (download_buffer[download_buffer_used - read + i] == '1' && 
                                            download_buffer[download_buffer_used - read + i+1] == '.' && 
                                            download_buffer[download_buffer_used - read + i+2] == '0' && 
                                            download_buffer[download_buffer_used - read + i+3] == '.' && 
                                            download_buffer[download_buffer_used - read + i+4] == '1') {
                                            uint32_t version_offset = total_downloaded - read + i;
                                            SYS_CONSOLE_PRINT("firmware_update: found '1.0.1' at offset %lu (expected: 4160)\r\n", version_offset);
                                        }
                                    }
                                }
                                
                                // Check if download is complete based on Content-Length
                                if (content_length > 0 && total_downloaded >= content_length) {
                                    download_complete = true;
                                    SYS_CONSOLE_PRINT("firmware_update: download complete based on Content-Length (%lu >= %lu)\r\n", 
                                                     total_downloaded, content_length);
                                    break;
                                }
                                
                                // Check if we've downloaded more than expected binary size (indicates extra data)
                                if (total_downloaded >= expected_binary_size) {
                                    SYS_CONSOLE_PRINT("firmware_update: download complete - reached expected binary size (%lu >= %lu)\r\n", 
                                                     total_downloaded, expected_binary_size);
                                    download_complete = true;
                                    break;
                                }
                                
                                // Reset timeouts since we received data
                                receive_timeout = 0;
                                no_data_timeout = 0;
                            }
                        } else {
                            // Buffer is full, try to flush it
                            SYS_CONSOLE_PRINT("firmware_update: buffer full (%lu bytes), forcing flush\r\n", download_buffer_used);
                            if (!flush_download_buffer_to_flash(download_buffer, download_buffer_size, 
                                                                       &download_buffer_used, &flash_write_address)) {
                                SYS_CONSOLE_PRINT("firmware_update: failed to flush full buffer to flash\r\n");
                                TCPIP_TCP_Close(tcp_socket);
                                return false;
                            }
                        }
                    } else {
                        // No data available, increment no-data timeout
                        no_data_timeout++;
                        
                        // If no data for too long and we have some data, assume download is complete
                        if (no_data_timeout >= max_no_data_timeout && total_downloaded > 0) {
                            // Check if we have a reasonable amount of data (at least 1KB)
                            if (total_downloaded >= 1024) {
                                SYS_CONSOLE_PRINT("firmware_update: no data received for %d seconds, assuming download complete (got %lu bytes)\r\n", 
                                                 max_no_data_timeout / 1000, total_downloaded);
                                download_complete = true;
                                break;
                            } else {
                                SYS_CONSOLE_PRINT("firmware_update: no data received for %d seconds, but only got %lu bytes - continuing to wait\r\n", 
                                                 max_no_data_timeout / 1000, total_downloaded);
                                no_data_timeout = 0; // Reset and continue waiting
                            }
                        }
                    }
                    
                    // Check if connection is still active
                    if (!TCPIP_TCP_IsConnected(tcp_socket)) {
                        SYS_CONSOLE_PRINT("firmware_update: connection closed by server\r\n");
                        if (content_length > 0) {
                            // If we have Content-Length, check if we got all the data
                            if (total_downloaded >= content_length) {
                                download_complete = true;
                                SYS_CONSOLE_PRINT("firmware_update: download complete - got all expected data\r\n");
                            } else {
                                SYS_CONSOLE_PRINT("firmware_update: connection closed but incomplete download (%lu < %lu)\r\n", 
                                                 total_downloaded, content_length);
                                return false;
                            }
                        } else {
                            // No Content-Length, assume download is complete
                            download_complete = true;
                            SYS_CONSOLE_PRINT("firmware_update: download complete - no Content-Length specified\r\n");
                        }
                        break;
                    }
                    
                    vTaskDelay(1);
                    receive_timeout++;
                    
                    if (receive_timeout % 5000 == 0) {
                        SYS_CONSOLE_PRINT("firmware_update: download progress: %lu bytes, buffer: %lu bytes, timeout: %d seconds, no-data: %d seconds\r\n", 
                                         total_downloaded, download_buffer_used, receive_timeout / 1000, no_data_timeout / 1000);
                    }
                }
                
                TCPIP_TCP_Close(tcp_socket);
                
                if (receive_timeout >= 30000) {
                    SYS_CONSOLE_PRINT("firmware_update: download timeout after %d seconds\r\n", receive_timeout / 1000);
                    SYS_CONSOLE_PRINT("firmware_update: final download status: %lu bytes, expected: %lu\r\n", 
                                     total_downloaded, content_length);
                    return false;
                }
                
                // Flush any remaining data in the buffer
                if (download_buffer_used > 0) {
                    SYS_CONSOLE_PRINT("firmware_update: flushing final %lu bytes to flash\r\n", download_buffer_used);
                    
                    // Pad the remaining data to a 256-byte boundary if needed
                    uint32_t padding_needed = 256 - (download_buffer_used % 256);
                    if (padding_needed < 256) {
                        memset(download_buffer + download_buffer_used, 0xFF, padding_needed);
                        download_buffer_used += padding_needed;
                        SYS_CONSOLE_PRINT("firmware_update: padded with %lu bytes to complete page\r\n", padding_needed);
                    }
                    
                    if (!flash_write(flash_write_address, download_buffer, download_buffer_used)) {
                        SYS_CONSOLE_PRINT("firmware_update: failed to write final buffered data to flash\r\n");
                        return false;
                    }
                    
                    // Verify the final data
                    uint8_t *verify_buffer = response_buffer + receive_buffer_size/2;
                    if (download_buffer_used > receive_buffer_size/2) {
                        SYS_CONSOLE_PRINT("firmware_update: final data too large for verification buffer\r\n");
                        return false;
                    }
                    
                    if (!flash_read(flash_write_address, verify_buffer, download_buffer_used)) {
                        SYS_CONSOLE_PRINT("firmware_update: failed to read back final data for verification\r\n");
                        return false;
                    }
                    
                    // Compare written data with original
                    bool verify_match = true;
                    for (uint32_t i = 0; i < download_buffer_used; i++) {
                        if (download_buffer[i] != verify_buffer[i]) {
                            SYS_CONSOLE_PRINT("firmware_update: final data verification failed at offset %lu: expected=0x%02x, got=0x%02x\r\n", 
                                             i, download_buffer[i], verify_buffer[i]);
                            verify_match = false;
                            break;
                        }
                    }
                    
                    if (!verify_match) {
                        SYS_CONSOLE_PRINT("firmware_update: final data verification failed\r\n");
                        return false;
                    }
                    
                    SYS_CONSOLE_PRINT("firmware_update: final data verified successfully\r\n");
                }
                
                SYS_CONSOLE_PRINT("firmware_update: download completed successfully: %lu bytes\r\n", total_downloaded);
                
                // 8. Calculate checksum of downloaded data (same as copy function)
                uint32_t checksum = FNV_32_INIT;
                uint32_t verify_addr = FLASH_BINARY_OFFSET;
                uint32_t remaining = total_downloaded;
                
                // Allocate buffer for checksum calculation
                uint8_t *checksum_buffer = malloc(1024);
                if (checksum_buffer == NULL) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to allocate checksum buffer\r\n");
                    return false;
                }
                
                while (remaining > 0) {
                    uint32_t chunk_size = (remaining > 1024) ? 1024 : remaining;
                    
                    if (!flash_read(verify_addr, checksum_buffer, chunk_size)) {
                        SYS_CONSOLE_PRINT("firmware_update: failed to read data for checksum calculation\r\n");
                        free(checksum_buffer);
                        return false;
                    }
                    
                    // Update checksum using FNV-32a algorithm
                    for (uint32_t i = 0; i < chunk_size; i++) {
                        checksum ^= (uint32_t)checksum_buffer[i];
                        checksum += (checksum << 1) + (checksum << 4) + 
                                   (checksum << 7) + (checksum << 8) + 
                                   (checksum << 24);
                    }
                    
                    verify_addr += chunk_size;
                    remaining -= chunk_size;
                }
                
                free(checksum_buffer);
                
                SYS_CONSOLE_PRINT("firmware_update: calculated checksum: 0x%08lx\r\n", checksum);
                
                // 9. Update firmware info with final data and mark as valid (same as copy function)
                // Extract name and version from the downloaded binary data
                char extracted_name[APPLICATION_NAME_SIZE];
                char extracted_version[APPLICATION_VERSION_SIZE];

                // Read the name from the downloaded binary at offset 4128
                if (!flash_read(FLASH_BINARY_OFFSET + 4128, (uint8_t*)extracted_name, APPLICATION_NAME_SIZE)) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to read name from downloaded binary\r\n");
                    return false;
                }
                extracted_name[APPLICATION_NAME_SIZE - 1] = '\0'; // Ensure null termination

                // Read the version from the downloaded binary at offset 4160
                if (!flash_read(FLASH_BINARY_OFFSET + 4160, (uint8_t*)extracted_version, APPLICATION_VERSION_SIZE)) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to read version from downloaded binary\r\n");
                    return false;
                }
                extracted_version[APPLICATION_VERSION_SIZE - 1] = '\0'; // Ensure null termination

                SYS_CONSOLE_PRINT("firmware_update: extracted name: '%s', version: '%s'\r\n", extracted_name, extracted_version);

                // Update the firmware info with the extracted data
                strncpy(fw_info.app_name, extracted_name, APPLICATION_NAME_SIZE - 1);
                strncpy(fw_info.app_version, extracted_version, APPLICATION_VERSION_SIZE - 1);
                fw_info.file_size = total_downloaded;
                fw_info.checksum = checksum;
                fw_info.valid = true;

                SYS_CONSOLE_PRINT("firmware_update: updating firmware info valid flag to true\r\n");

                // We need to erase the info sector again before updating the valid flag
                if (!flash_erase_sector(FLASH_INFO_OFFSET)) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to erase info sector for valid flag update\r\n");
                    return false;
                }
                
                if (!flash_write(FLASH_INFO_OFFSET, (uint8_t*)&fw_info, sizeof(fw_info))) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to update firmware info as valid\r\n");
                    return false;
                }
                
                // Verify the update was successful
                firmware_info_t final_verify_fw_info;
                if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&final_verify_fw_info, sizeof(final_verify_fw_info))) {
                    SYS_CONSOLE_PRINT("firmware_update: failed to read back final firmware info\r\n");
                    return false;
                }
                
                SYS_CONSOLE_PRINT("firmware_update: final firmware info - valid flag: %s\r\n", final_verify_fw_info.valid ? "true" : "false");
                
                if (!final_verify_fw_info.valid) {
                    SYS_CONSOLE_PRINT("firmware_update: WARNING - valid flag was not set correctly!\r\n");
                    return false;
                }
                
                SYS_CONSOLE_PRINT("firmware_update: download and verification completed successfully\r\n");
                return true;
                
                break;
            }
        }
        
        if (!TCPIP_TCP_IsConnected(tcp_socket)) {
            SYS_CONSOLE_PRINT("firmware_update: connection closed during header reception\r\n");
            break;
        }
        
        vTaskDelay(1);
        receive_timeout++;
    }
    
    if (!headers_received) {
        SYS_CONSOLE_PRINT("firmware_update: failed to receive complete headers\r\n");        
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    return false; // Should never reach here
}

char * firmware_update_get_internal_name(void) {
    static char app_name[APPLICATION_NAME_SIZE];
    
    // Read application name from internal flash
    if (!NVM_Read((uint32_t*)app_name, APPLICATION_NAME_SIZE, APPLICATION_NAME_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read internal app name\r\n");
        return NULL;
    }
    
    // Ensure null termination
    app_name[APPLICATION_NAME_SIZE - 1] = '\0';
    
    return app_name;
}

char * firmware_update_get_internal_version(void) {
    static char app_version[APPLICATION_VERSION_SIZE];
    
    // Read application version from internal flash
    if (!NVM_Read((uint32_t*)app_version, APPLICATION_VERSION_SIZE, APPLICATION_VERSION_ADDRESS)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read internal app version\r\n");
        return NULL;
    }
    
    // Ensure null termination
    app_version[APPLICATION_VERSION_SIZE - 1] = '\0';
    
    return app_version;
}

// Helper function to check if a string contains valid printable characters
static bool is_valid_string(const char *str, size_t max_len) {
    if (!str) return false;
    
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') break;
        if (str[i] < 32 || str[i] > 126) { // Check for printable ASCII
            return false;
        }
    }
    return true;
}

char * firmware_update_get_external_name(void) {
    static char app_name[APPLICATION_NAME_SIZE];
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        return "none";
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    bool success = flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info));
    // // debug what is actually saved in flash
    // SYS_CONSOLE_PRINT("firmware_update: debug: external app name: ");
    // for(int i = 0; i < APPLICATION_NAME_SIZE; i++){
    //     SYS_CONSOLE_PRINT("%c", external_fw_info.app_name[i]);
    // }
    // SYS_CONSOLE_PRINT("\r\n");
    if (!success) {
        return "none";
    }
    
    // Check if the firmware info is valid (has valid strings and valid flag)
    if (!external_fw_info.valid || !is_valid_string(external_fw_info.app_name, APPLICATION_NAME_SIZE)) {
        return "none";
    }
    
    // Copy the app name
    strncpy(app_name, external_fw_info.app_name, APPLICATION_NAME_SIZE - 1);
    app_name[APPLICATION_NAME_SIZE - 1] = '\0'; // Ensure null termination    
    
    return app_name;
}

char * firmware_update_get_external_version(void) {
    static char app_version[APPLICATION_VERSION_SIZE];
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        return "none";
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    bool success = flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info));
    // // debug what is actually saved in flash
    // SYS_CONSOLE_PRINT("firmware_update: debug: external app version: ");
    // for(int i = 0; i < APPLICATION_VERSION_SIZE; i++){
    //     SYS_CONSOLE_PRINT("%c", external_fw_info.app_version[i]);
    // }
    // SYS_CONSOLE_PRINT("\r\n");
    if (!success) {
        return "none";
    }
    
    // Check if the firmware info is valid (has valid strings and valid flag)
    if (!external_fw_info.valid || !is_valid_string(external_fw_info.app_version, APPLICATION_VERSION_SIZE)) {
        return "none";
    }
    
    // Copy the app version
    strncpy(app_version, external_fw_info.app_version, APPLICATION_VERSION_SIZE - 1);
    app_version[APPLICATION_VERSION_SIZE - 1] = '\0'; // Ensure null termination    
    
    return app_version;
}

bool firmware_update_get_external_valid(void) {
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        return false;
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info))) {
        return false;
    }
    
    // Check if the firmware info is valid (has valid strings and valid flag)
    if (!external_fw_info.valid || 
        !is_valid_string(external_fw_info.app_name, APPLICATION_NAME_SIZE) ||
        !is_valid_string(external_fw_info.app_version, APPLICATION_VERSION_SIZE)) {
        return false;
    }
    
    // Get internal app name for comparison
    char *internal_name = firmware_update_get_internal_name();
    if (!internal_name) {
        return false;
    }
    
    // Check if app names match
    if (strncmp(internal_name, external_fw_info.app_name, APPLICATION_NAME_SIZE) != 0) {
        // SYS_CONSOLE_PRINT("firmware_update: app name mismatch - internal: %s, external: %s\r\n", 
        //                   internal_name, external_fw_info.app_name);
        return false;
    }
    
    return true;
}

// Helper function to parse semantic version components
static bool parse_semantic_version(const char *version, int *major, int *minor, int *patch) {
    if (!version || !major || !minor || !patch) {
        return false;
    }
    
    // Initialize to 0
    *major = 0;
    *minor = 0;
    *patch = 0;
    
    // Parse major.minor.patch format
    if (sscanf(version, "%d.%d.%d", major, minor, patch) == 3) {
        return true;
    }
    
    // Try major.minor format
    if (sscanf(version, "%d.%d", major, minor) == 2) {
        *patch = 0;
        return true;
    }
    
    // Try just major format
    if (sscanf(version, "%d", major) == 1) {
        *minor = 0;
        *patch = 0;
        return true;
    }
    
    return false;
}

// Helper function to compare semantic versions
static int compare_semantic_versions(const char *version1, const char *version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    // Parse both versions
    if (!parse_semantic_version(version1, &major1, &minor1, &patch1)) {
        return -1; // version1 is invalid
    }
    
    if (!parse_semantic_version(version2, &major2, &minor2, &patch2)) {
        return 1; // version2 is invalid
    }
    
    // Compare major version
    if (major1 > major2) return 1;
    if (major1 < major2) return -1;
    
    // Major versions are equal, compare minor version
    if (minor1 > minor2) return 1;
    if (minor1 < minor2) return -1;
    
    // Minor versions are equal, compare patch version
    if (patch1 > patch2) return 1;
    if (patch1 < patch2) return -1;
    
    // All components are equal
    return 0;
}

bool firmware_update_get_internal_latest(void) {
    // Get internal version
    char *internal_version = firmware_update_get_internal_version();
    if (!internal_version) {
        SYS_CONSOLE_PRINT("firmware_update: failed to get internal version\r\n");
        return false;
    }
    
    // Get external version
    char *external_version = firmware_update_get_external_version();
    if (!external_version || strcmp(external_version, "none") == 0) {
        // SYS_CONSOLE_PRINT("firmware_update: no valid external version available\r\n");
        return true; // If no external version, internal is considered latest
    }
    
    // Check if external firmware is valid
    if (!firmware_update_get_external_valid()) {
        // SYS_CONSOLE_PRINT("firmware_update: external firmware is not valid\r\n");
        return true; // If external is invalid, internal is considered latest
    }
    
    // Compare versions
    int comparison = compare_semantic_versions(internal_version, external_version);
    
    // SYS_CONSOLE_PRINT("firmware_update: version comparison - internal: %s, external: %s, result: %d\r\n", 
    //                   internal_version, external_version, comparison);
    
    // Return true if internal version is greater than or equal to external
    return (comparison >= 0);
}