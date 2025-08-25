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

// Helper function to search for app_name and app_version in the download buffer
static bool find_app_info_in_buffer(uint8_t *buffer, uint32_t buffer_size, uint32_t download_offset, uint32_t *name_offset, uint32_t *version_offset) {
    const char *target_name = "node90";
    const char *target_version_prefix = "1."; // Assuming version starts with "1."
    
    SYS_CONSOLE_PRINT("firmware_update: searching for app info in buffer of size %lu\r\n", buffer_size);
    
    // Search for the app name "node90"
    bool name_found = false;
    uint32_t found_name_offset = 0;
    
    for (uint32_t i = 0; i <= buffer_size - strlen(target_name); i++) {
        bool match = true;
        for (uint32_t j = 0; j < strlen(target_name); j++) {
            if (buffer[i + j] != target_name[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            name_found = true;
            found_name_offset = download_offset + i;
            SYS_CONSOLE_PRINT("firmware_update: found 'node90' at offset %lu\r\n", found_name_offset);
            break;
        }
    }
    
    if (!name_found) {
        SYS_CONSOLE_PRINT("firmware_update: could not find 'node90' in buffer\r\n");
        return false;
    }
    
    // Search for version string (look for "1." pattern)
    bool version_found = false;
    uint32_t found_version_offset = 0;
    
    for (uint32_t i = 0; i <= buffer_size - strlen(target_version_prefix); i++) {
        bool match = true;
        for (uint32_t j = 0; j < strlen(target_version_prefix); j++) {
            if (buffer[i + j] != target_version_prefix[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            // Verify this looks like a version string by checking a few more characters
            bool looks_like_version = true;
            for (uint32_t k = strlen(target_version_prefix); k < strlen(target_version_prefix) + 10 && i + k < buffer_size; k++) {
                char c = buffer[i + k];
                if (c == '\0' || c == ' ' || c == '\r' || c == '\n') {
                    break; // End of version string
                }
                if (!((c >= '0' && c <= '9') || c == '.')) {
                    looks_like_version = false;
                    break;
                }
            }
            
            if (looks_like_version) {
                version_found = true;
                found_version_offset = download_offset + i;
                SYS_CONSOLE_PRINT("firmware_update: found version at offset %lu\r\n", found_version_offset);
                break;
            }
        }
    }
    
    if (!version_found) {
        SYS_CONSOLE_PRINT("firmware_update: could not find version string in buffer\r\n");
        return false;
    }
    
    // Compare with expected offsets
    SYS_CONSOLE_PRINT("firmware_update: found name at %lu (expected: 4128), version at %lu (expected: 4160)\r\n", 
                     found_name_offset, found_version_offset);
    
    if (found_name_offset != 4128) {
        SYS_CONSOLE_PRINT("firmware_update: WARNING - name offset differs from expected!\r\n");
    }
    
    if (found_version_offset != 4160) {
        SYS_CONSOLE_PRINT("firmware_update: WARNING - version offset differs from expected!\r\n");
    }
    
    *name_offset = found_name_offset;
    *version_offset = found_version_offset;
    
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
    
    // Initialize streaming HTTP client
    http_stream_t stream;
    if (!http_stream_open(url, &stream)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to open HTTP stream\r\n");
        return false;
    }
    
    // Prepare firmware info structure (initially with valid=false)
    firmware_info_t fw_info;
    memset(&fw_info, 0, sizeof(fw_info));
    strncpy(fw_info.app_name, "unassigned", APPLICATION_NAME_SIZE - 1);
    strncpy(fw_info.app_version, "unassigned", APPLICATION_VERSION_SIZE - 1);
    fw_info.file_size = stream.content_length > 0 ? stream.content_length : 0;
    fw_info.checksum = 0; // Will be calculated after download
    fw_info.valid = false; // Will be set to true after successful download and verification
    
    // Erase the info sector in external flash
    SYS_CONSOLE_PRINT("firmware_update: erasing info sector at 0x%08lx\r\n", FLASH_INFO_OFFSET);
    if (!flash_erase_sector(FLASH_INFO_OFFSET)) {
        SYS_CONSOLE_PRINT("firmware_update: failed to erase info sector\r\n");
        http_stream_close(&stream);
        return false;
    }
    
    // Write firmware info to external flash (initially with valid=false)
    if (!flash_write(FLASH_INFO_OFFSET, (uint8_t*)&fw_info, sizeof(fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to write firmware info\r\n");
        http_stream_close(&stream);
        return false;
    }
    
    // Pre-erase all sectors needed for the binary
    uint32_t binary_size = APPLICATION_END_ADDRESS - APPLICATION_START_ADDRESS;
    uint32_t sector_size = 4096; // 4KB sectors
    uint32_t total_sectors = (binary_size + sector_size - 1) / sector_size; // Round up
    SYS_CONSOLE_PRINT("firmware_update: erasing %lu sectors for binary data (binary size: %lu bytes)\r\n", total_sectors, binary_size);
    
    for (uint32_t sector = 0; sector < total_sectors; sector++) {
        uint32_t sector_addr = FLASH_BINARY_OFFSET + (sector * sector_size);
        SYS_CONSOLE_PRINT("firmware_update: erasing sector %lu at 0x%08lx\r\n", sector, sector_addr);
        
        if (!flash_erase_sector(sector_addr)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to erase sector %lu\r\n", sector);
            http_stream_close(&stream);
            return false;
        }
    }
    
    SYS_CONSOLE_PRINT("firmware_update: all sectors erased successfully\r\n");
    
    // Initialize the buffering system variables
    download_buffer_used = 0; // Reset buffer
    flash_write_address = FLASH_BINARY_OFFSET; // Start at binary offset
    uint32_t total_downloaded = 0;
    uint32_t checksum = FNV_32_INIT;
    
    SYS_CONSOLE_PRINT("firmware_update: starting download...\r\n");
    SYS_CONSOLE_PRINT("firmware_update: using download buffer size: %lu bytes\r\n", download_buffer_size);
    
    while (1) {
        // Calculate how much space is available in the buffer
        uint32_t available_space = download_buffer_size - download_buffer_used;
        
        if (available_space == 0) {
            // Buffer is full, flush it to flash
            SYS_CONSOLE_PRINT("firmware_update: buffer full, flushing to flash\r\n");
            if (!flush_download_buffer_to_flash(download_buffer, download_buffer_size, 
                                               &download_buffer_used, &flash_write_address)) {
                SYS_CONSOLE_PRINT("firmware_update: failed to flush buffer to flash\r\n");
                http_stream_close(&stream);
                return false;
            }
            available_space = download_buffer_size; // Buffer is now empty
        }
        
        // Read chunk from HTTP stream into the buffer
        //int bytes_read = http_stream_read(&stream, download_buffer + download_buffer_used, available_space);
        int bytes_read = http_stream_read(&stream, download_buffer + download_buffer_used, 513);
        
        if (bytes_read < 0) {
            SYS_CONSOLE_PRINT("firmware_update: error reading from HTTP stream\r\n");
            http_stream_close(&stream);
            return false;
        }
        
        if (bytes_read == 0) {
            // Download complete
            SYS_CONSOLE_PRINT("firmware_update: download complete\r\n");
            break;
        }
        
        // Update buffer usage and checksum
        uint32_t buffer_offset = download_buffer_used;
        download_buffer_used += bytes_read;
        total_downloaded += bytes_read;

        if(total_downloaded <= 0x1400){
            for(int i=0; i<bytes_read; i++){
                SYS_CONSOLE_PRINT("%c", download_buffer[buffer_offset+i]);
            }
            SYS_CONSOLE_PRINT("\r\n");
        }

        // find app_name and app_version
        // Search for app info in the buffer if we haven't found it yet
        static bool app_info_found = false;
        static uint32_t found_name_offset = 0;
        static uint32_t found_version_offset = 0;
        uint32_t download_offset = total_downloaded - bytes_read;
        
        if (!app_info_found) { 
            if (find_app_info_in_buffer(download_buffer, download_buffer_used, download_offset, &found_name_offset, &found_version_offset)) {
                app_info_found = true;
                SYS_CONSOLE_PRINT("firmware_update: app info found - name at %lu, version at %lu\r\n", 
                                 found_name_offset, found_version_offset);
            }
        }
        
        
        // Update checksum for the new data
        for (int i = 0; i < bytes_read; i++) {
            checksum ^= (uint32_t)download_buffer[download_buffer_used - bytes_read + i];
            checksum += (checksum << 1) + (checksum << 4) + 
                       (checksum << 7) + (checksum << 8) + 
                       (checksum << 24);
        }
        
        SYS_CONSOLE_PRINT("firmware_update: received %d bytes, buffer now has %lu bytes, total: %lu\r\n", 
                         bytes_read, download_buffer_used, total_downloaded);
        
        // Try to flush buffer to flash if we have enough data for complete pages
        if (!flush_download_buffer_to_flash(download_buffer, download_buffer_size, 
                                           &download_buffer_used, &flash_write_address)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to flush buffer to flash\r\n");
            http_stream_close(&stream);
            return false;
        }        
        
        // Check if we've downloaded more than expected binary size
        if (total_downloaded >= binary_size) {
            SYS_CONSOLE_PRINT("firmware_update: reached expected binary size\r\n");
            break;
        }
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
            http_stream_close(&stream);
            return false;
        }
        
        // Verify the final data
        uint8_t *verify_buffer = response_buffer + download_buffer_size;
        if (download_buffer_used > download_buffer_size) {
            SYS_CONSOLE_PRINT("firmware_update: final data too large for verification buffer\r\n");
            http_stream_close(&stream);
            return false;
        }
        
        if (!flash_read(flash_write_address, verify_buffer, download_buffer_used)) {
            SYS_CONSOLE_PRINT("firmware_update: failed to read back final data for verification\r\n");
            http_stream_close(&stream);
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
            http_stream_close(&stream);
            return false;
        }
        
        SYS_CONSOLE_PRINT("firmware_update: final data verified successfully\r\n");
    }
    
    http_stream_close(&stream);
    
    SYS_CONSOLE_PRINT("firmware_update: download completed successfully: %lu bytes\r\n", total_downloaded);
    SYS_CONSOLE_PRINT("firmware_update: calculated checksum: 0x%08lx\r\n", checksum);
    
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

    // Erase the info sector again before updating the valid flag
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