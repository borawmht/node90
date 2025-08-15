/*
* Resources
* resources.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef RESOURCES_H
#define RESOURCES_H

#include "coap.h"

typedef struct {
    const char *path;    
    void (*init)(void);
    coap_resource_handler_t coap_server_handler;
} resource_t;

#define RESOURCE_JSON_STR_SIZE 2048
#define RESOURCE_E_JSON_STR_SIZE RESOURCE_JSON_STR_SIZE+10
#define RESOURCE_CBOR_BUFFER_SIZE RESOURCE_JSON_STR_SIZE
extern char resource_json_str[];
extern char resource_e_json_str[];
extern uint8_t resource_cbor_buffer[];
extern const resource_t resources[];

void resources_init(void);

#endif /* RESOURCES_H */