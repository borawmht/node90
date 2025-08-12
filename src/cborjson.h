/*
* CBOR to JSON conversion utility header
* cborjson.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef CBORJSON_H
#define CBORJSON_H

#include "tinycbor/cbor.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert CBOR data to JSON string
 * 
 * @param cbor_data Pointer to CBOR data
 * @param cbor_size Size of CBOR data
 * @param json_buffer Buffer to store JSON output
 * @param buffer_size Size of JSON buffer
 * @param json_length Pointer to store actual JSON string length
 * @param flags Conversion flags (use CborConvertDefaultFlags for default)
 * @return CborError status code
 */
CborError cbor_to_json_string(const uint8_t *cbor_data, size_t cbor_size,
                             char *json_buffer, size_t buffer_size, 
                             size_t *json_length, int flags);

/**
 * Convert CBOR data to JSON with dynamic allocation
 * 
 * @param cbor_data Pointer to CBOR data
 * @param cbor_size Size of CBOR data
 * @param json_string Pointer to store allocated JSON string
 * @param json_length Pointer to store JSON string length
 * @param flags Conversion flags
 * @return CborError status code
 */
CborError cbor_to_json_alloc(const uint8_t *cbor_data, size_t cbor_size,
                            char **json_string, size_t *json_length, int flags);

/**
 * Free allocated JSON string
 */
void cbor_json_free(char *json_string);

#ifdef __cplusplus
}
#endif

#endif /* CBORJSON_H */ 