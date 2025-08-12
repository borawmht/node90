/*
* JSON to CBOR conversion utility header
* jsoncbor.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef JSONCBOR_H
#define JSONCBOR_H

#include "tinycbor/cbor.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert a JSON string to CBOR format
 * 
 * @param json_string The JSON string to convert
 * @param cbor_buffer Buffer to store the CBOR output
 * @param buffer_size Size of the CBOR buffer
 * @param encoded_size Pointer to store the actual size of encoded CBOR data
 * @return CborError status code
 */
CborError json_to_cbor(const char *json_string, uint8_t *cbor_buffer, size_t buffer_size, size_t *encoded_size);

/**
 * Convert JSON string to CBOR with dynamic buffer allocation
 * 
 * @param json_string The JSON string to convert
 * @param cbor_buffer Pointer to store the allocated CBOR buffer
 * @param encoded_size Pointer to store the actual size of encoded CBOR data
 * @return CborError status code
 */
CborError json_to_cbor_alloc(const char *json_string, uint8_t **cbor_buffer, size_t *encoded_size);

/**
 * Free allocated CBOR buffer
 */
void json_cbor_free(uint8_t *cbor_buffer);

#ifdef __cplusplus
}
#endif

#endif /* JSONCBOR_H */ 