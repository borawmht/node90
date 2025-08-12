/*
* JSON to CBOR conversion utility
* jsoncbor.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "jsoncbor.h"
#include "cJSON.h"
#include "tinycbor/cbor.h"
#include <string.h>
#include <stdlib.h>

// Forward declarations
static CborError json_value_to_cbor(CborEncoder *encoder, const cJSON *json_item);
static CborError json_array_to_cbor(CborEncoder *encoder, const cJSON *json_array);
static CborError json_object_to_cbor(CborEncoder *encoder, const cJSON *json_object);

/**
 * Convert a JSON string to CBOR format
 * 
 * @param json_string The JSON string to convert
 * @param cbor_buffer Buffer to store the CBOR output
 * @param buffer_size Size of the CBOR buffer
 * @param encoded_size Pointer to store the actual size of encoded CBOR data
 * @return CborError status code
 */
CborError json_to_cbor(const char *json_string, uint8_t *cbor_buffer, size_t buffer_size, size_t *encoded_size) {
    if (!json_string || !cbor_buffer || !encoded_size) {
        return CborUnknownError;
    }
    
    // Parse JSON string
    cJSON *json_root = cJSON_Parse(json_string);
    if (!json_root) {
        return CborUnknownError;
    }
    
    // Initialize CBOR encoder
    CborEncoder encoder;
    cbor_encoder_init(&encoder, cbor_buffer, buffer_size, 0);
    
    // Convert JSON to CBOR
    CborError result = json_value_to_cbor(&encoder, json_root);
    
    // Get the encoded size
    if (result == CborNoError) {
        *encoded_size = cbor_encoder_get_buffer_size(&encoder, cbor_buffer);
    } else {
        *encoded_size = 0;
    }
    
    // Clean up
    cJSON_Delete(json_root);
    
    return result;
}

/**
 * Convert a cJSON item to CBOR format
 */
static CborError json_value_to_cbor(CborEncoder *encoder, const cJSON *json_item) {
    if (!json_item || !encoder) {
        return CborUnknownError;
    }
    
    CborError result = CborNoError;
    
    // Handle different JSON types
    if (cJSON_IsNull(json_item)) {
        result = cbor_encode_null(encoder);
    }
    else if (cJSON_IsTrue(json_item)) {
        result = cbor_encode_boolean(encoder, true);
    }
    else if (cJSON_IsFalse(json_item)) {
        result = cbor_encode_boolean(encoder, false);
    }
    else if (cJSON_IsNumber(json_item)) {
        double value = cJSON_GetNumberValue(json_item);
        
        // Check if it's an integer
        if (value == (double)((int64_t)value)) {
            // It's an integer
            if (value >= 0) {
                result = cbor_encode_uint(encoder, (uint64_t)value);
            } else {
                result = cbor_encode_negative_int(encoder, (uint64_t)(-value - 1));
            }
        } else {
            // It's a floating point number
            result = cbor_encode_double(encoder, value);
        }
    }
    else if (cJSON_IsString(json_item)) {
        char *str_value = cJSON_GetStringValue(json_item);
        if (str_value) {
            result = cbor_encode_text_stringz(encoder, str_value);
        } else {
            result = CborUnknownError;
        }
    }
    else if (cJSON_IsArray(json_item)) {
        result = json_array_to_cbor(encoder, json_item);
    }
    else if (cJSON_IsObject(json_item)) {
        result = json_object_to_cbor(encoder, json_item);
    }
    else {
        // Unknown or invalid type
        result = CborUnknownError;
    }
    
    return result;
}

/**
 * Convert a JSON array to CBOR array
 */
static CborError json_array_to_cbor(CborEncoder *encoder, const cJSON *json_array) {
    if (!json_array || !encoder) {
        return CborUnknownError;
    }
    
    int array_size = cJSON_GetArraySize(json_array);
    CborEncoder array_encoder;
    
    // Create CBOR array
    CborError result = cbor_encoder_create_array(encoder, &array_encoder, array_size);
    if (result != CborNoError) {
        return result;
    }
    
    // Add each array element
    for (int i = 0; i < array_size; i++) {
        cJSON *array_item = cJSON_GetArrayItem(json_array, i);
        if (array_item) {
            result = json_value_to_cbor(&array_encoder, array_item);
            if (result != CborNoError) {
                return result;
            }
        }
    }
    
    // Close the array
    return cbor_encoder_close_container(encoder, &array_encoder);
}

/**
 * Convert a JSON object to CBOR map
 */
static CborError json_object_to_cbor(CborEncoder *encoder, const cJSON *json_object) {
    if (!json_object || !encoder) {
        return CborUnknownError;
    }
    
    int object_size = cJSON_GetArraySize(json_object); // cJSON uses same function for objects
    CborEncoder map_encoder;
    
    // Create CBOR map
    CborError result = cbor_encoder_create_map(encoder, &map_encoder, object_size);
    if (result != CborNoError) {
        return result;
    }
    
    // Add each object key-value pair
    cJSON *current_item = json_object->child;
    while (current_item) {
        // Encode the key (must be a string in JSON)
        if (current_item->string) {
            result = cbor_encode_text_stringz(&map_encoder, current_item->string);
            if (result != CborNoError) {
                return result;
            }
        } else {
            return CborUnknownError;
        }
        
        // Encode the value
        result = json_value_to_cbor(&map_encoder, current_item);
        if (result != CborNoError) {
            return result;
        }
        
        current_item = current_item->next;
    }
    
    // Close the map
    return cbor_encoder_close_container(encoder, &map_encoder);
}

/**
 * Convert JSON string to CBOR with dynamic buffer allocation
 * 
 * @param json_string The JSON string to convert
 * @param cbor_buffer Pointer to store the allocated CBOR buffer
 * @param encoded_size Pointer to store the actual size of encoded CBOR data
 * @return CborError status code
 */
CborError json_to_cbor_alloc(const char *json_string, uint8_t **cbor_buffer, size_t *encoded_size) {
    if (!json_string || !cbor_buffer || !encoded_size) {
        return CborUnknownError;
    }
    
    // Parse JSON string
    cJSON *json_root = cJSON_Parse(json_string);
    if (!json_root) {
        return CborUnknownError;
    }
    
    // First pass: calculate required buffer size
    CborEncoder encoder;
    cbor_encoder_init(&encoder, NULL, 0, 0);
    
    CborError result = json_value_to_cbor(&encoder, json_root);
    if (result != CborNoError) {
        cJSON_Delete(json_root);
        return result;
    }
    
    size_t required_size = cbor_encoder_get_extra_bytes_needed(&encoder);
    
    // Allocate buffer
    *cbor_buffer = malloc(required_size);
    if (!*cbor_buffer) {
        cJSON_Delete(json_root);
        return CborErrorOutOfMemory;
    }
    
    // Second pass: encode to allocated buffer
    cbor_encoder_init(&encoder, *cbor_buffer, required_size, 0);
    result = json_value_to_cbor(&encoder, json_root);
    
    if (result == CborNoError) {
        *encoded_size = cbor_encoder_get_buffer_size(&encoder, *cbor_buffer);
    } else {
        free(*cbor_buffer);
        *cbor_buffer = NULL;
        *encoded_size = 0;
    }
    
    cJSON_Delete(json_root);
    return result;
}

/**
 * Free allocated CBOR buffer
 */
void json_cbor_free(uint8_t *cbor_buffer) {
    if (cbor_buffer) {
        free(cbor_buffer);
    }
} 