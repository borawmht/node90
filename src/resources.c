/*
* resources.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "resources.h"

// Define the array here
const resource_t resources[] = {
    {"/inx/network", &network_coap_handler, &network_init},
    {NULL, NULL, NULL}
}; 

void resources_init(void) {
    for(int i = 0; resources[i].path != NULL; i++) {
        resources[i].init();
    }
}