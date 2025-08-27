/*
* resources.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "resources.h"
#include "definitions.h"

#include "resources/network.h"
#include "resources/version.h"
#include "resources/ota.h"
#include "resources/actuators.h"
#include "resources/sensors.h"
#include "resources/policy.h"

char resource_json_str[RESOURCE_JSON_STR_SIZE];
char resource_e_json_str[RESOURCE_E_JSON_STR_SIZE];
uint8_t resource_cbor_buffer[RESOURCE_CBOR_BUFFER_SIZE];

// Define the array here
const resource_t resources[] = {
    {"/inx/network", &network_init, &network_coap_handler},
    {"/inx/version", &version_init, &version_coap_handler},
    {"/inx/ota", &ota_init, &ota_coap_handler},
    {"/inx/actuators", &actuators_init, &actuators_coap_handler},
    {"/inx/actuators/actuator1", NULL, &actuators_actuator_coap_handler},
    {"/inx/actuators/actuator2", NULL, &actuators_actuator_coap_handler},
    {"/inx/actuators/actuator1/context", NULL, &actuators_actuator_context_coap_handler},
    {"/inx/actuators/actuator2/context", NULL, &actuators_actuator_context_coap_handler},
    {"/inx/sensors", &sensors_init, &sensors_coap_handler},
    {"/inx/sensors/sensor1", NULL, &sensors_sensor_coap_handler},
    {"/inx/sensors/WallSwitch", NULL, &sensors_wallswitch_coap_handler},
    {"/inx/sensors/sensor1/context", NULL, &sensors_sensor_context_coap_handler},
    {"/inx/sensors/WallSwitch/context", NULL, &sensors_wallswitch_context_coap_handler},
    {"/inx/policy", &policy_init, &policy_coap_handler},
    {"/inx/policy/policy1", NULL, &policy_coap_handler},
    {NULL, NULL, NULL}
}; 

void resources_init(void) {
    SYS_CONSOLE_PRINT("resources: init\r\n");
    for(int i = 0; resources[i].path != NULL; i++) {
        if(resources[i].init != NULL){
            resources[i].init();
        }
    }
}