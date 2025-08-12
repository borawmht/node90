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
} resource_t;

const resource_t resources[] = {
    {"/inx/network", &network_coap_handler},
    {NULL, false}
};

#endif /* RESOURCES_H */