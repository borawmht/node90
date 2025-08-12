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

void network_init(void);
bool network_coap_handler(const coap_message_t *request, coap_message_t *response);

#endif /* NETWORK_H */