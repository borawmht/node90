/*
* CBOR to JSON conversion utility
* cborjson.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "cborjson.h"
#include "tinycbor/cbor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static CborError cbor_value_to_json_string_recursive(CborValue *value, char *buffer, size_t buffer_size, size_t *position, bool advance_after);

CborError cbor_to_json_string(const uint8_t *cbor_data, size_t cbor_size,
                             char *json_buffer, size_t buffer_size, 
                             size_t *json_length, int flags) {
    if (!cbor_data || !json_buffer || !json_length) {
        return CborUnknownError;
    }
    
    // Initialize CBOR parser
    CborParser parser;
    CborValue value;
    CborError error = cbor_parser_init(cbor_data, cbor_size, 0, &parser, &value);
    if (error != CborNoError) {
        return error;
    }
    
    size_t position = 0;
    error = cbor_value_to_json_string_recursive(&value, json_buffer, buffer_size, &position, true);
    
    if (error == CborNoError) {
        *json_length = position;
        // Ensure null termination
        if (position < buffer_size) {
            json_buffer[position] = '\0';
        }
    } else {
        *json_length = 0;
    }
    
    return error;
}

static CborError cbor_value_to_json_string_recursive(CborValue *value, char *buffer, size_t buffer_size, size_t *position, bool advance_after) {
    if (!value || !buffer || !position) {
        return CborUnknownError;
    }
    
    // Check if the value is valid
    if (!cbor_value_is_valid(value)) {
        return CborUnknownError;
    }
    
    CborType type = cbor_value_get_type(value);
    
    CborError error = CborNoError;
    
    switch (type) {
        case CborNullType: {
            const char *null_str = "null";
            size_t len = strlen(null_str);
            if (*position + len >= buffer_size) return CborErrorDataTooLarge;
            memcpy(buffer + *position, null_str, len);
            *position += len;
            break;
        }
        
        case CborBooleanType: {
            bool bool_val;
            error = cbor_value_get_boolean(value, &bool_val);
            if (error != CborNoError) return error;
            
            const char *bool_str = bool_val ? "true" : "false";
            size_t len = strlen(bool_str);
            if (*position + len >= buffer_size) return CborErrorDataTooLarge;
            memcpy(buffer + *position, bool_str, len);
            *position += len;
            break;
        }
        
        case CborIntegerType: {
            if (cbor_value_is_unsigned_integer(value)) {
                uint64_t uint_val;
                error = cbor_value_get_uint64(value, &uint_val);
                if (error != CborNoError) return error;
                
                char num_str[32];
                int written = snprintf(num_str, sizeof(num_str), "%llu", (unsigned long long)uint_val);
                if (written < 0 || (size_t)written >= sizeof(num_str)) return CborErrorDataTooLarge;
                
                if (*position + written >= buffer_size) return CborErrorDataTooLarge;
                memcpy(buffer + *position, num_str, written);
                *position += written;
            } else {
                int64_t int_val;
                error = cbor_value_get_int64(value, &int_val);
                if (error != CborNoError) return error;
                
                char num_str[32];
                int written = snprintf(num_str, sizeof(num_str), "%lld", (long long)int_val);
                if (written < 0 || (size_t)written >= sizeof(num_str)) return CborErrorDataTooLarge;
                
                if (*position + written >= buffer_size) return CborErrorDataTooLarge;
                memcpy(buffer + *position, num_str, written);
                *position += written;
            }
            break;
        }
        
        case CborFloatType: {
            float float_val;
            error = cbor_value_get_float(value, &float_val);
            if (error != CborNoError) return error;
            
            char num_str[32];
            int written = snprintf(num_str, sizeof(num_str), "%.6g", float_val);
            if (written < 0 || (size_t)written >= sizeof(num_str)) return CborErrorDataTooLarge;
            
            if (*position + written >= buffer_size) return CborErrorDataTooLarge;
            memcpy(buffer + *position, num_str, written);
            *position += written;
            break;
        }
        
        case CborDoubleType: {
            double double_val;
            error = cbor_value_get_double(value, &double_val);
            if (error != CborNoError) return error;
            
            char num_str[32];
            int written = snprintf(num_str, sizeof(num_str), "%.15g", double_val);
            if (written < 0 || (size_t)written >= sizeof(num_str)) return CborErrorDataTooLarge;
            
            if (*position + written >= buffer_size) return CborErrorDataTooLarge;
            memcpy(buffer + *position, num_str, written);
            *position += written;
            break;
        }
        
        case CborTextStringType: {
            size_t string_len;
            error = cbor_value_get_string_length(value, &string_len);
            if (error != CborNoError) return error;
            
            // Add opening quote
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = '"';
            *position += 1;
            
            // Copy string content (simplified - doesn't handle escaping)
            if (string_len > 0) {
                if (*position + string_len >= buffer_size) return CborErrorDataTooLarge;
                
                char *string_buffer = malloc(string_len + 1);
                if (!string_buffer) return CborErrorOutOfMemory;
                
                CborValue next;
                error = cbor_value_copy_text_string(value, string_buffer, &string_len, &next);
                if (error == CborNoError) {
                    memcpy(buffer + *position, string_buffer, string_len);
                    *position += string_len;
                }
                
                free(string_buffer);
                if (error != CborNoError) return error;
            }
            
            // Add closing quote
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = '"';
            *position += 1;
            break;
        }
        
        case CborArrayType: {
            size_t array_len;
            error = cbor_value_get_array_length(value, &array_len);
            if (error != CborNoError) return CborErrorDataTooLarge;
            
            // Add opening bracket
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = '[';
            *position += 1;
            
            // Process array elements
            CborValue recursed;
            error = cbor_value_enter_container(value, &recursed);
            if (error != CborNoError) return error;
            
            for (size_t i = 0; i < array_len; i++) {
                if (i > 0) {
                    if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
                    buffer[*position] = ',';
                    *position += 1;
                }
                
                error = cbor_value_to_json_string_recursive(&recursed, buffer, buffer_size, position, true);
                if (error != CborNoError) return error;
            }
            
            error = cbor_value_leave_container(value, &recursed);
            if (error != CborNoError) return error;
            
            // Add closing bracket
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = ']';
            *position += 1;
            break;
        }
        
        case CborMapType: {
            size_t map_len;
            error = cbor_value_get_map_length(value, &map_len);
            if (error != CborNoError) return CborErrorDataTooLarge;
            
            // Add opening brace
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = '{';
            *position += 1;
            
            // Process map elements
            CborValue recursed;
            error = cbor_value_enter_container(value, &recursed);
            if (error != CborNoError) {
                return error;
            }
            
            for (size_t i = 0; i < map_len; i++) {
                if (i > 0) {
                    if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
                    buffer[*position] = ',';
                    *position += 1;
                }
                
                // Check if we're at the end
                if (cbor_value_at_end(&recursed)) {
                    break;
                }
                
                // Process key (don't advance after processing)
                error = cbor_value_to_json_string_recursive(&recursed, buffer, buffer_size, position, false);
                if (error != CborNoError) {
                    return error;
                }
                
                // Add colon
                if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
                buffer[*position] = ':';
                *position += 1;
                
                // Advance to value
                error = cbor_value_advance(&recursed);
                if (error != CborNoError) {
                    return error;
                }
                
                // Check if we're at the end
                if (cbor_value_at_end(&recursed)) {
                    break;
                }
                
                // Process value (don't advance after processing)
                error = cbor_value_to_json_string_recursive(&recursed, buffer, buffer_size, position, false);
                if (error != CborNoError) {
                    return error;
                }
                
                // Advance after processing the value, but only if we're not at the end
                if (!cbor_value_at_end(&recursed)) {
                    error = cbor_value_advance(&recursed);
                    if (error != CborNoError) {
                        return error;
                    }
                }
            }
            
            error = cbor_value_leave_container(value, &recursed);
            if (error != CborNoError) {
                return error;
            }
            
            // Add closing brace
            if (*position + 1 >= buffer_size) return CborErrorDataTooLarge;
            buffer[*position] = '}';
            *position += 1;
            break;
        }
        
        default: {
            // For unsupported types, output a placeholder
            const char *placeholder = "\"<unsupported_type>\"";
            size_t len = strlen(placeholder);
            if (*position + len >= buffer_size) return CborErrorDataTooLarge;
            memcpy(buffer + *position, placeholder, len);
            *position += len;
            break;
        }
    }
    
    // Only advance if requested and this is a simple type (not a container)
    if (advance_after && type != CborArrayType && type != CborMapType) {
        error = cbor_value_advance(value);
        if (error != CborNoError) {
            return error;
        }
    }
    
    return error;
}

CborError cbor_to_json_alloc(const uint8_t *cbor_data, size_t cbor_size,
                            char **json_string, size_t *json_length, int flags) {
    if (!cbor_data || !json_string || !json_length) {
        return CborUnknownError;
    }
    
    // Use a temporary buffer to get the JSON
    char temp_buffer[2048];
    size_t temp_length;
    CborError error = cbor_to_json_string(cbor_data, cbor_size, temp_buffer, 
                                         sizeof(temp_buffer), &temp_length, flags);
    
    if (error != CborNoError) {
        return error;
    }
    
    // Allocate buffer
    *json_string = malloc(temp_length + 1);
    if (!*json_string) {
        return CborErrorOutOfMemory;
    }
    
    // Copy the result
    memcpy(*json_string, temp_buffer, temp_length + 1);
    *json_length = temp_length;
    
    return CborNoError;
}

void cbor_json_free(char *json_string) {
    if (json_string) {
        free(json_string);
    }
} 