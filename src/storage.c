/*
* storage.c - PIC32 Port of ESP32 NVS Storage API
* created by: Brad Oraw
* created on: 2025-08-12
*/

#include "storage.h"
#include "eeprom.h"
#include "definitions.h"
#include <string.h>

static const char *TAG = "storage";

// Storage entry structure
typedef struct {
    uint32_t magic;           // Magic number for validation
    uint16_t namespace_len;   // Length of namespace string
    uint16_t key_len;         // Length of key string
    uint16_t data_len;        // Length of data
    uint8_t data_type;        // Type of data (bool, u8, i8, u16, i16, u32, i32, str, blob)
    uint8_t reserved;         // Reserved for alignment
} storage_entry_t;

// Data type definitions
#define STORAGE_TYPE_BOOL     1
#define STORAGE_TYPE_U8       2
#define STORAGE_TYPE_I8       3
#define STORAGE_TYPE_U16      4
#define STORAGE_TYPE_I16      5
#define STORAGE_TYPE_U32      6
#define STORAGE_TYPE_I32      7
#define STORAGE_TYPE_STR      8
#define STORAGE_TYPE_BLOB     9

// Storage entry magic number
#define STORAGE_ENTRY_MAGIC   0xABCD1234

// Calculate hash for namespace + key
static uint32_t storage_hash(const char *namespace, const char *key) {
    uint32_t hash = 5381;
    int c;
    
    // Hash namespace
    while ((c = *namespace++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Hash key
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

// Find storage entry
static bool storage_find_entry(const char *namespace, const char *key, uint8_t data_type, 
                              storage_entry_t *entry, uint32_t *entry_addr) {
    uint32_t search_addr = EEPROM_DATA_START;
    storage_entry_t temp_entry;
    
    while (search_addr < EEPROM_SIZE - sizeof(storage_entry_t)) {
        if (!eeprom_read(search_addr, (uint8_t*)&temp_entry, sizeof(storage_entry_t))) {
            break;
        }
        
        // Check if this is a valid entry
        if (temp_entry.magic == STORAGE_ENTRY_MAGIC) {
            // Read namespace and key
            char entry_namespace[32];
            char entry_key[32];
            
            if (temp_entry.namespace_len < sizeof(entry_namespace) && 
                temp_entry.key_len < sizeof(entry_key)) {
                
                eeprom_read(search_addr + sizeof(storage_entry_t), 
                           (uint8_t*)entry_namespace, temp_entry.namespace_len);
                entry_namespace[temp_entry.namespace_len] = '\0';
                
                eeprom_read(search_addr + sizeof(storage_entry_t) + temp_entry.namespace_len,
                           (uint8_t*)entry_key, temp_entry.key_len);
                entry_key[temp_entry.key_len] = '\0';
                
                // Check if this is the entry we're looking for
                if (strcmp(entry_namespace, namespace) == 0 && 
                    strcmp(entry_key, key) == 0 && 
                    temp_entry.data_type == data_type) {
                    
                    *entry = temp_entry;
                    *entry_addr = search_addr;
                    return true;
                }
            }
            
            // Move to next entry
            search_addr += sizeof(storage_entry_t) + temp_entry.namespace_len + 
                          temp_entry.key_len + temp_entry.data_len;
        } else {
            // Invalid entry, stop searching
            break;
        }
    }
    
    return false;
}

// Write storage entry
static bool storage_write_entry(const char *namespace, const char *key, uint8_t data_type,
                               const void *data, uint16_t data_len) {
    storage_entry_t entry;
    uint32_t entry_addr = EEPROM_DATA_START;
    
    // Find existing entry or end of storage
    storage_entry_t temp_entry;
    while (entry_addr < EEPROM_SIZE - sizeof(storage_entry_t)) {
        if (!eeprom_read(entry_addr, (uint8_t*)&temp_entry, sizeof(storage_entry_t))) {
            break;
        }
        
        if (temp_entry.magic == STORAGE_ENTRY_MAGIC) {
            // Check if this is the entry we want to update
            char entry_namespace[32];
            char entry_key[32];
            
            if (temp_entry.namespace_len < sizeof(entry_namespace) && 
                temp_entry.key_len < sizeof(entry_key)) {
                
                eeprom_read(entry_addr + sizeof(storage_entry_t), 
                           (uint8_t*)entry_namespace, temp_entry.namespace_len);
                entry_namespace[temp_entry.namespace_len] = '\0';
                
                eeprom_read(entry_addr + sizeof(storage_entry_t) + temp_entry.namespace_len,
                           (uint8_t*)entry_key, temp_entry.key_len);
                entry_key[temp_entry.key_len] = '\0';
                
                if (strcmp(entry_namespace, namespace) == 0 && 
                    strcmp(entry_key, key) == 0 && 
                    temp_entry.data_type == data_type) {
                    
                    // Update existing entry
                    break;
                }
            }
            
            // Move to next entry
            entry_addr += sizeof(storage_entry_t) + temp_entry.namespace_len + 
                         temp_entry.key_len + temp_entry.data_len;
        } else {
            // Found free space
            break;
        }
    }
    
    // Prepare entry
    entry.magic = STORAGE_ENTRY_MAGIC;
    entry.namespace_len = strlen(namespace);
    entry.key_len = strlen(key);
    entry.data_len = data_len;
    entry.data_type = data_type;
    entry.reserved = 0;
    
    // Calculate total size needed
    uint16_t total_size = sizeof(storage_entry_t) + entry.namespace_len + 
                         entry.key_len + entry.data_len;
    
    if (entry_addr + total_size > EEPROM_SIZE) {
        SYS_CONSOLE_PRINT("storage: insufficient space\r\n");
        return false;
    }
    
    // Write entry header
    if (!eeprom_write(entry_addr, (uint8_t*)&entry, sizeof(storage_entry_t))) {
        return false;
    }
    
    // Write namespace
    if (!eeprom_write(entry_addr + sizeof(storage_entry_t), 
                     (uint8_t*)namespace, entry.namespace_len)) {
        return false;
    }
    
    // Write key
    if (!eeprom_write(entry_addr + sizeof(storage_entry_t) + entry.namespace_len,
                     (uint8_t*)key, entry.key_len)) {
        return false;
    }
    
    // Write data
    if (!eeprom_write(entry_addr + sizeof(storage_entry_t) + entry.namespace_len + entry.key_len,
                     (uint8_t*)data, entry.data_len)) {
        return false;
    }
    
    return true;
}

// Read storage entry
static bool storage_read_entry(const char *namespace, const char *key, uint8_t data_type,
                              void *data, uint16_t *data_len) {
    storage_entry_t entry;
    uint32_t entry_addr;
    
    if (!storage_find_entry(namespace, key, data_type, &entry, &entry_addr)) {
        return false;
    }
    
    if (*data_len < entry.data_len) {
        return false;
    }
    
    // Read data
    if (!eeprom_read(entry_addr + sizeof(storage_entry_t) + entry.namespace_len + entry.key_len,
                    (uint8_t*)data, entry.data_len)) {
        return false;
    }
    
    *data_len = entry.data_len;
    return true;
}

// Bool functions
bool storage_getBool(const char *namespace, const char *key, bool *value) {
    uint16_t data_len = sizeof(bool);
    return storage_read_entry(namespace, key, STORAGE_TYPE_BOOL, value, &data_len);
}

bool storage_setBool(const char *namespace, const char *key, bool value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_BOOL, &value, sizeof(bool));
}

bool storage_loadBool(const char *namespace, const char *key, bool *b, bool default_value, bool (*func)(bool)) {
    bool value = default_value;
    *b = value;
    bool ret = storage_getBool(namespace, key, &value);
    if (!ret) {
        ret = storage_setBool(namespace, key, value); // save default value
    } else {
        *b = value; // set value if load OK
    }
    if (func != NULL) {
        func(*b);
    }
    return ret;
}

// U8 functions
bool storage_getU8(const char *namespace, const char *key, uint8_t *value) {
    uint16_t data_len = sizeof(uint8_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_U8, value, &data_len);
}

bool storage_setU8(const char *namespace, const char *key, uint8_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_U8, &value, sizeof(uint8_t));
}

bool storage_loadU8(const char *namespace, const char *key, uint8_t *u8, uint8_t default_value, bool (*func)(uint8_t)) {
    uint8_t value = default_value;
    *u8 = value;
    bool ret = storage_getU8(namespace, key, &value);
    if (!ret) {
        ret = storage_setU8(namespace, key, value); // save default value
    } else {
        *u8 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*u8);
    }
    return ret;
}

// I8 functions
bool storage_getI8(const char *namespace, const char *key, int8_t *value) {
    uint16_t data_len = sizeof(int8_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_I8, value, &data_len);
}

bool storage_setI8(const char *namespace, const char *key, int8_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_I8, &value, sizeof(int8_t));
}

bool storage_loadI8(const char *namespace, const char *key, int8_t *i8, int8_t default_value, bool (*func)(int8_t)) {
    int8_t value = default_value;
    *i8 = value;
    bool ret = storage_getI8(namespace, key, &value);
    if (!ret) {
        ret = storage_setI8(namespace, key, value); // save default value
    } else {
        *i8 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*i8);
    }
    return ret;
}

// U16 functions
bool storage_getU16(const char *namespace, const char *key, uint16_t *value) {
    uint16_t data_len = sizeof(uint16_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_U16, value, &data_len);
}

bool storage_setU16(const char *namespace, const char *key, uint16_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_U16, &value, sizeof(uint16_t));
}

bool storage_loadU16(const char *namespace, const char *key, uint16_t *u16, uint16_t default_value, bool (*func)(uint16_t)) {
    uint16_t value = default_value;
    *u16 = value;
    bool ret = storage_getU16(namespace, key, &value);
    if (!ret) {
        ret = storage_setU16(namespace, key, value); // save default value
    } else {
        *u16 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*u16);
    }
    return ret;
}

// I16 functions
bool storage_getI16(const char *namespace, const char *key, int16_t *value) {
    uint16_t data_len = sizeof(int16_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_I16, value, &data_len);
}

bool storage_setI16(const char *namespace, const char *key, int16_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_I16, &value, sizeof(int16_t));
}

bool storage_loadI16(const char *namespace, const char *key, int16_t *i16, int16_t default_value, bool (*func)(int16_t)) {
    int16_t value = default_value;
    *i16 = value;
    bool ret = storage_getI16(namespace, key, &value);
    if (!ret) {
        ret = storage_setI16(namespace, key, value); // save default value
    } else {
        *i16 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*i16);
    }
    return ret;
}

// U32 functions
bool storage_getU32(const char *namespace, const char *key, uint32_t *value) {
    uint16_t data_len = sizeof(uint32_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_U32, value, &data_len);
}

bool storage_setU32(const char *namespace, const char *key, uint32_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_U32, &value, sizeof(uint32_t));
}

bool storage_loadU32(const char *namespace, const char *key, uint32_t *u32, uint32_t default_value, bool (*func)(uint32_t)) {
    uint32_t value = default_value;
    *u32 = value;
    bool ret = storage_getU32(namespace, key, &value);
    if (!ret) {
        ret = storage_setU32(namespace, key, value); // save default value
    } else {
        *u32 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*u32);
    }
    return ret;
}

// I32 functions
bool storage_getI32(const char *namespace, const char *key, int32_t *value) {
    uint16_t data_len = sizeof(int32_t);
    return storage_read_entry(namespace, key, STORAGE_TYPE_I32, value, &data_len);
}

bool storage_setI32(const char *namespace, const char *key, int32_t value) {
    return storage_write_entry(namespace, key, STORAGE_TYPE_I32, &value, sizeof(int32_t));
}

bool storage_loadI32(const char *namespace, const char *key, int32_t *i32, int32_t default_value, bool (*func)(int32_t)) {
    int32_t value = default_value;
    *i32 = value;
    bool ret = storage_getI32(namespace, key, &value);
    if (!ret) {
        ret = storage_setI32(namespace, key, value); // save default value
    } else {
        *i32 = value; // set value if load OK
    }
    if (func != NULL) {
        func(*i32);
    }
    return ret;
}

// String functions
bool storage_getStr(const char *namespace, const char *key, char *value) {
    uint16_t data_len = STORAGE_STR_LENGTH_MAX;
    return storage_read_entry(namespace, key, STORAGE_TYPE_STR, value, &data_len);
}

bool storage_setStr(const char *namespace, const char *key, char *value) {
    uint16_t len = strlen(value);
    if (len > STORAGE_STR_LENGTH_MAX) {
        len = STORAGE_STR_LENGTH_MAX;
    }
    return storage_write_entry(namespace, key, STORAGE_TYPE_STR, value, len);
}

bool storage_loadStr(const char *namespace, const char *key, char *str, const char *default_value, bool (*func)(char *)) {
    char value[STORAGE_STR_LENGTH_MAX + 1];
    strcpy(value, default_value);
    strcpy(str, value);
    bool ret = storage_getStr(namespace, key, value);
    if (!ret) {
        ret = storage_setStr(namespace, key, value); // save default value
    } else {
        strcpy(str, value); // set value if load OK
    }
    if (func != NULL) {
        func(str);
    }
    return ret;
}

bool storage_loadStrIndex(const char *namespace, const char *key, char *value, const char *default_value, uint16_t index, bool (*func)(uint16_t, char *)) {
    char str[STORAGE_STR_LENGTH_MAX + 1];
    strcpy(str, default_value);
    strcpy(value, str);
    bool ret = storage_getStr(namespace, key, str);
    if (!ret) {
        ret = storage_setStr(namespace, key, str); // save default value
    } else {
        strcpy(value, str); // set value if load OK
    }
    if (func != NULL) {
        func(index, value);
    }
    return ret;
}

// Blob functions
bool storage_getBlob(const char *namespace, const char *key, void *value, size_t *length) {
    uint16_t data_len = *length;
    bool ret = storage_read_entry(namespace, key, STORAGE_TYPE_BLOB, value, &data_len);
    if (ret) {
        *length = data_len;
    }
    return ret;
}

bool storage_setBlob(const char *namespace, const char *key, const void *value, size_t length) {
    if (length > 65535) { // Max uint16_t
        return false;
    }
    return storage_write_entry(namespace, key, STORAGE_TYPE_BLOB, (void*)value, (uint16_t)length);
}

bool storage_loadBlob(const char *namespace, const char *key, void *value, size_t max_length, 
                     const void *default_value, size_t default_length, bool (*func)(void *)) {
    size_t length = max_length;
    bool ret = storage_getBlob(namespace, key, value, &length);
    if (!ret) {
        // Load failed, use default value
        if (default_length <= max_length) {
            memcpy(value, default_value, default_length);
            length = default_length;
            ret = storage_setBlob(namespace, key, value, length); // save default value
        } else {
            ret = false;
        }
    }
    if (func != NULL) {
        func(value);
    }
    return ret;
}