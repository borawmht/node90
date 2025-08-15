/*
* resources.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "resources.h"
#include "definitions.h"

#include "resources/network.h"

char resource_json_str[RESOURCE_JSON_STR_SIZE];
char resource_e_json_str[RESOURCE_E_JSON_STR_SIZE];
uint8_t resource_cbor_buffer[RESOURCE_CBOR_BUFFER_SIZE];

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