/*
* bearssl_custom.c
* Custom X.509 engine that accepts any certificate without validation
*/
#include "bearssl_custom.h"
#include <stdbool.h>

// Context to store the actual server public key
static br_x509_pkey server_pkey = {0};
static unsigned char server_modulus[512]; // Buffer for RSA modulus
static unsigned char server_exponent[8];  // Buffer for RSA exponent
static bool key_extracted = false;

// Certificate buffer to accumulate certificate data
static unsigned char cert_buffer[4096];
static size_t cert_buffer_len = 0;
static uint32_t cert_length = 0;

// DN callback function (required by br_x509_decoder_init)
static void dn_callback(void *ctx, const void *buf, size_t len) {
    (void)ctx;
    (void)buf;
    (void)len;
    // We don't need to process the DN, so this is a no-op
}

// Custom X.509 engine that accepts any certificate without validation
void no_validation_start_chain(const br_x509_class **ctx, const char *server_name) {
    (void)ctx;
    (void)server_name;
    // Reset key extraction flag and certificate buffer
    key_extracted = false;
    cert_buffer_len = 0;
    cert_length = 0;
    memset(&server_pkey, 0, sizeof(server_pkey));
}

void no_validation_start_cert(const br_x509_class **ctx, uint32_t length) {
    (void)ctx;
    cert_length = length;
    cert_buffer_len = 0;
    // Reset certificate buffer for new certificate
    if (length > sizeof(cert_buffer)) {
        // Certificate too large, truncate
        cert_length = sizeof(cert_buffer);
    }
}

void no_validation_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    (void)ctx;
    // Accumulate certificate data
    if (cert_buffer_len + len <= sizeof(cert_buffer)) {
        memcpy(cert_buffer + cert_buffer_len, buf, len);
        cert_buffer_len += len;
    }
}

void no_validation_end_cert(const br_x509_class **ctx) {
    (void)ctx;
    // Extract the public key from the accumulated certificate data
    if (cert_buffer_len > 0 && !key_extracted) {
        // Use BearSSL's X.509 decoder to extract the public key
        br_x509_decoder_context xc_decoder;
        br_x509_decoder_init(&xc_decoder, dn_callback, NULL);
        
        // Push the certificate data to the decoder
        br_x509_decoder_push(&xc_decoder, cert_buffer, cert_buffer_len);
        
        // Get the decoded public key
        const br_x509_pkey *decoded_pkey = br_x509_decoder_get_pkey(&xc_decoder);
        if (decoded_pkey != NULL) {
            // Copy the decoded public key to our server_pkey
            memcpy(&server_pkey, decoded_pkey, sizeof(server_pkey));
            
            // If it's an RSA key, copy the modulus and exponent to our buffers
            if (decoded_pkey->key_type == BR_KEYTYPE_RSA) {
                if (decoded_pkey->key.rsa.nlen <= sizeof(server_modulus)) {
                    memcpy(server_modulus, decoded_pkey->key.rsa.n, decoded_pkey->key.rsa.nlen);
                    server_pkey.key.rsa.n = server_modulus;
                    server_pkey.key.rsa.nlen = decoded_pkey->key.rsa.nlen;
                }
                if (decoded_pkey->key.rsa.elen <= sizeof(server_exponent)) {
                    memcpy(server_exponent, decoded_pkey->key.rsa.e, decoded_pkey->key.rsa.elen);
                    server_pkey.key.rsa.e = server_exponent;
                    server_pkey.key.rsa.elen = decoded_pkey->key.rsa.elen;
                }
            }
            
            key_extracted = true;
        }
    }
}

unsigned no_validation_end_chain(const br_x509_class **ctx) {
    (void)ctx;
    return 0; // Always return success (0)
}

const br_x509_pkey *no_validation_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    (void)ctx;
    
    if (!key_extracted) {
        // Fallback: create a dummy key if none was extracted
        // This should not happen if the certificate was properly processed
        memset(server_modulus, 0, sizeof(server_modulus));
        server_modulus[0] = 0x80;
        server_modulus[255] = 0x01;
        
        memset(server_exponent, 0, sizeof(server_exponent));
        server_exponent[0] = 0x01;
        server_exponent[1] = 0x00;
        server_exponent[2] = 0x01;
        
        server_pkey.key_type = BR_KEYTYPE_RSA;
        server_pkey.key.rsa.n = server_modulus;
        server_pkey.key.rsa.nlen = 256;
        server_pkey.key.rsa.e = server_exponent;
        server_pkey.key.rsa.elen = 3;
        
        key_extracted = true;
    }
    
    if (usages != NULL) {
        *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
    }
    
    return &server_pkey;
}

// Custom X.509 engine vtable
const br_x509_class no_validation_vtable = {
    sizeof(br_x509_custom_context), // Use our custom context size
    no_validation_start_chain,
    no_validation_start_cert,
    no_validation_append,
    no_validation_end_cert,
    no_validation_end_chain,
    no_validation_get_pkey
};
