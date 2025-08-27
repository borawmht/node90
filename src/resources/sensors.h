/*
* Sensors Resource
* sensors.h
* created by: Brad Oraw
* created on: 2025-08-27
*/

#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>
#include "resources.h"

#define NUM_SENSORS 1
#define SENSOR_TYPE_SIZE 16
#define EVENT_SIZE 128

#define NUM_WALLSWITCHES 1

typedef struct {
    char ns[16];
    uint8_t channel;
    char cluster[CLUSTER_SIZE];
    char prphtag[TAG_SIZE];    
    char type[SENSOR_TYPE_SIZE];  
    char eventlh[EVENT_SIZE];
    char eventhl[EVENT_SIZE];
    uint16_t holdtime;
    uint16_t occupiedtimeout;
    uint16_t vaccanttimeout;
    uint16_t high_threshold;
    uint16_t low_threshold; 
    bool input_state;
    bool logical_state;   
} sensor_t;

typedef struct {
    char ns[16];
    uint8_t channel;
    char cluster[CLUSTER_SIZE];
    char prphtag[TAG_SIZE]; 
} wallswitch_t;

void sensors_init(void);
bool sensors_put_json_str(char * json_str);
char * sensors_get_json_str(void);
bool sensors_coap_handler(const coap_message_t *request, coap_message_t *response);

bool sensors_sensor_put_json_str(uint8_t channel, char * json_str);
char * sensors_sensor_get_json_str(uint8_t channel);
bool sensors_sensor_coap_handler(const coap_message_t *request, coap_message_t *response);
bool sensors_sensor_context_coap_handler(const coap_message_t *request, coap_message_t *response);

bool sensors_sensor_set_cluster(uint8_t channel, char * cluster);
bool sensors_sensor_set_prphtag(uint8_t channel, char * prphtag);
bool sensors_sensor_set_type(uint8_t channel, char * type);
bool sensors_sensor_set_eventlh(uint8_t channel, char * eventlh);
bool sensors_sensor_set_eventhl(uint8_t channel, char * eventhl);
bool sensors_sensor_set_holdtime(uint8_t channel, uint16_t holdtime);
bool sensors_sensor_set_occupiedtimeout(uint8_t channel, uint16_t occupiedtimeout);
bool sensors_sensor_set_vaccanttimeout(uint8_t channel, uint16_t vaccanttimeout);
bool sensors_sensor_set_high_threshold(uint8_t channel, uint16_t high_threshold);
bool sensors_sensor_set_low_threshold(uint8_t channel, uint16_t low_threshold);
bool sensors_sensor_set_input_state(uint8_t channel, bool input_state);
bool sensors_sensor_set_logical_state(uint8_t channel, bool logical_state);
char * sensors_sensor_get_cluster(uint8_t channel);
char * sensors_sensor_get_prphtag(uint8_t channel);
char * sensors_sensor_get_type(uint8_t channel);
char * sensors_sensor_get_eventlh(uint8_t channel);
char * sensors_sensor_get_eventhl(uint8_t channel);
uint16_t sensors_sensor_get_holdtime(uint8_t channel);
uint16_t sensors_sensor_get_occupiedtimeout(uint8_t channel);
uint16_t sensors_sensor_get_vaccanttimeout(uint8_t channel);
uint16_t sensors_sensor_get_high_threshold(uint8_t channel);
uint16_t sensors_sensor_get_low_threshold(uint8_t channel);
bool sensors_sensor_get_input_state(uint8_t channel);
bool sensors_sensor_get_logical_state(uint8_t channel);

bool sensors_wallswitch_put_json_str(uint8_t channel, char * json_str);
char * sensors_wallswitch_get_json_str(uint8_t channel);
bool sensors_wallswitch_coap_handler(const coap_message_t *request, coap_message_t *response);
bool sensors_wallswitch_context_coap_handler(const coap_message_t *request, coap_message_t *response);

bool sensors_wallswitch_set_cluster(uint8_t channel, char * cluster);
bool sensors_wallswitch_set_prphtag(uint8_t channel, char * prphtag);
char * sensors_wallswitch_get_cluster(uint8_t channel);
char * sensors_wallswitch_get_prphtag(uint8_t channel);

#endif // SENSORS_H