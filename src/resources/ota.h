/*
* OTA Resource
* ota.h
* created by: Brad Oraw
* created on: 2025-08-26
*/

#ifndef OTA_H
#define OTA_H

#include "coap.h"
#include <stdbool.h>

#define OTA_BIN_URL_SIZE 128

typedef struct {
    char bin_url[OTA_BIN_URL_SIZE];
} ota_t;

void ota_init(void);
bool ota_set_bin_url(char * bin_url);
char * ota_get_bin_url(void);
bool ota_put_json_str(char * json_str);
char * ota_get_json_str(void);
bool ota_coap_handler(const coap_message_t *request, coap_message_t *response);

#endif