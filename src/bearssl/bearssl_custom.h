/*
* bearssl_custom.h
* Custom X.509 engine that accepts any certificate without validation
*/

#ifndef BEARSSL_CUSTOM_H
#define BEARSSL_CUSTOM_H

#include "bearssl.h"

// Custom X.509 engine context structure
typedef struct {
    const br_x509_class *vtable;
    br_x509_pkey pkey;
} br_x509_custom_context;

// Function declarations for the custom X.509 engine
void no_validation_start_chain(const br_x509_class **ctx, const char *server_name);
void no_validation_start_cert(const br_x509_class **ctx, uint32_t length);
void no_validation_append(const br_x509_class **ctx, const unsigned char *buf, size_t len);
void no_validation_end_cert(const br_x509_class **ctx);
unsigned no_validation_end_chain(const br_x509_class **ctx);
const br_x509_pkey *no_validation_get_pkey(const br_x509_class *const *ctx, unsigned *usages);

// Custom X.509 engine vtable
extern const br_x509_class no_validation_vtable;

#endif // BEARSSL_CUSTOM_H