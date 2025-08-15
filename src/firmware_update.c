/*
* firmware_update.c
* created by: Brad Oraw
* created on: 2025-08-15
*/

#include "firmware_update.h"
#include "definitions.h"
#include "flash.h"
#include "peripheral/nvm/plib_nvm.h"  // Add this for NVM_Read
#include "hash_fnv.h"
#include <stdlib.h>
#include <string.h>

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

bool firmware_update_download_binary_to_external_flash(const char * url){
    SYS_CONSOLE_PRINT("firmware_update: download binary to external flash\r\n");
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

char * firmware_update_get_external_name(void) {
    static char app_name[APPLICATION_NAME_SIZE];
    
    // Check if flash is initialized
    if (!flash_is_initialized()) {
        SYS_CONSOLE_PRINT("firmware_update: external flash not initialized\r\n");
        return NULL;
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read external firmware info\r\n");
        return NULL;
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
        SYS_CONSOLE_PRINT("firmware_update: external flash not initialized\r\n");
        return NULL;
    }
    
    // Read firmware info from external flash
    firmware_info_t external_fw_info;
    if (!flash_read(FLASH_INFO_OFFSET, (uint8_t*)&external_fw_info, sizeof(external_fw_info))) {
        SYS_CONSOLE_PRINT("firmware_update: failed to read external firmware info\r\n");
        return NULL;
    }
    
    // Copy the app version
    strncpy(app_version, external_fw_info.app_version, APPLICATION_VERSION_SIZE - 1);
    app_version[APPLICATION_VERSION_SIZE - 1] = '\0'; // Ensure null termination
    
    return app_version;
}

bool firmware_update_get_external_valid(void) {
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
    
    return external_fw_info.valid;
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
    if (!external_version) {
        SYS_CONSOLE_PRINT("firmware_update: failed to get external version\r\n");
        return true; // If no external version, internal is considered latest
    }
    
    // Check if external firmware is valid
    if (!firmware_update_get_external_valid()) {
        SYS_CONSOLE_PRINT("firmware_update: external firmware is not valid\r\n");
        return true; // If external is invalid, internal is considered latest
    }
    
    // Compare versions
    int comparison = compare_semantic_versions(internal_version, external_version);
    
    SYS_CONSOLE_PRINT("firmware_update: version comparison - internal: %s, external: %s, result: %d\r\n", 
                      internal_version, external_version, comparison);
    
    // Return true if internal version is greater than or equal to external
    return (comparison >= 0);
}