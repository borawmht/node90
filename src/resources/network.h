/*
* Network Resource
* network.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef NETWORK_H
#define NETWORK_H

#include "coap.h"
#include <stdbool.h>

typedef struct {
    char serial_number[16];
    char tag[16];
    char inx_ip[20];
} network_t;

void network_init(void);
bool network_set_tag(char * tag);
bool network_set_inx_ip(char * inx_ip);
bool network_set_serial_number(char * serial_number);
char * network_get_tag(void);
char * network_get_inx_ip(void);
char * network_get_serial_number(void);
bool network_put_json_str(char * json_str);
char * network_get_json_str(void);
bool network_coap_handler(const coap_message_t *request, coap_message_t *response);

#endif /* NETWORK_H */