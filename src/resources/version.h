/*
* Version Resource
* version.h
* created by: Brad Oraw
* created on: 2025-08-26
*/

#ifndef VERSION_H
#define VERSION_H

#include "coap.h"
#include <stdbool.h>

void version_init(void);
char * version_get_json_str(void);
bool version_coap_get_handler(coap_message_t *response);
bool version_coap_put_handler(const coap_message_t *request, coap_message_t *response);
bool version_coap_handler(const coap_message_t *request, coap_message_t *response);
char * version_get_name(void);
char * version_get_version(void);
char * version_get_date(void);
char * version_get_time(void);

#endif