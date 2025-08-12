/*
* Resources
* resources.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef RESOURCES_H
#define RESOURCES_H

#include "resources/network.h"
#include "coap.h"
#include "stddef.h"

typedef struct {
    const char *path;
    coap_resource_handler_t coap_server_handler;
    void (*init)(void);
} resource_t;

// Declare the array as extern (no definition)
extern const resource_t resources[];

void resources_init(void);

#endif /* RESOURCES_H */