/*
* event.h
* created by: Brad Oraw
* created on: 2025-08-27
*/

#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "resources.h"

void event_init(void);

bool event_put_json_str(char * json_str);
char * event_get_json_str(void);
bool event_coap_handler(const coap_message_t *request, coap_message_t *response);

#endif