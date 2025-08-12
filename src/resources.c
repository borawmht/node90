/*
* resources.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "resources.h"
#include "definitions.h"

// Define the array here
const resource_t resources[] = {
    {"/inx/network", &network_init, &network_coap_handler},
    {NULL, NULL, NULL}
}; 

void resources_init(void) {
    SYS_CONSOLE_PRINT("resources: init\r\n");
    for(int i = 0; resources[i].path != NULL; i++) {
        resources[i].init();
    }
}