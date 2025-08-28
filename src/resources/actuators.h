/*
* Actuators Resource
* actuators.h
* created by: Brad Oraw
* created on: 2025-08-26
*/

#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdint.h>
#include <stdbool.h>
#include "resources.h"
#include "pwm.h"

#define NUM_ACTUATORS 2
#define ACTUATOR_MODE_SIZE 24
#define MAX_MAXA 2500

typedef struct {
    char ns[16];
    uint8_t channel;
    char cluster[CLUSTER_SIZE];
    char prphtag[TAG_SIZE];
    char mode[ACTUATOR_MODE_SIZE];
    uint16_t fadetime;
    char pwm_mode[PWM_MODE_SIZE];
    bool motion_enable;
    // uint8_t dim;
    uint8_t dim_els;
    bool cuv_enable;
    uint16_t cc;
    uint16_t cv;
    uint16_t cp;
    // uint16_t at;    
} actuator_t;

void actuators_init(void);
bool actuators_put_json_str(char * json_str);
char * actuators_get_json_str(void);
bool actuators_coap_handler(const coap_message_t *request, coap_message_t *response);

bool actuators_actuator_put_json_str(uint8_t channel, char * json_str);
char * actuators_actuator_get_json_str(uint8_t channel);
bool actuators_actuator_coap_handler(const coap_message_t *request, coap_message_t *response);
bool actuators_actuator_context_coap_handler(const coap_message_t *request, coap_message_t *response);

bool actuators_actuator_set_cluster(uint8_t channel, char * cluster);
bool actuators_actuator_set_prphtag(uint8_t channel, char * prphtag);
bool actuators_actuator_set_mode(uint8_t channel, char * mode);
bool actuators_actuator_set_fadetime(uint8_t channel, uint16_t fadetime);
bool actuators_actuator_set_pwm_mode(uint8_t channel, char * pwm_mode);
bool actuators_actuator_set_motion_enable(uint8_t channel, bool motion_enable);
bool actuators_actuator_set_dim_els(uint8_t channel, uint8_t dim_els);
bool actuators_actuator_set_cuv_enable(uint8_t channel, bool cuv_enable);
bool actuators_actuator_set_cc(uint8_t channel, uint16_t cc);
bool actuators_actuator_set_cv(uint8_t channel, uint16_t cv);
bool actuators_actuator_set_cp(uint8_t channel, uint16_t cp);
bool actuators_actuator_set_dim(uint8_t channel, uint8_t dim);
bool actuators_actuator_set_at(uint8_t channel, uint16_t at);
char * actuators_actuator_get_cluster(uint8_t channel);
char * actuators_actuator_get_prphtag(uint8_t channel);
char * actuators_actuator_get_mode(uint8_t channel);
uint16_t actuators_actuator_get_fadetime(uint8_t channel);
char * actuators_actuator_get_pwm_mode(uint8_t channel);
bool actuators_actuator_get_motion_enable(uint8_t channel);
uint8_t actuators_actuator_get_dim_els(uint8_t channel);
bool actuators_actuator_get_cuv_enable(uint8_t channel);
uint16_t actuators_actuator_get_cc(uint8_t channel);
uint16_t actuators_actuator_get_cv(uint8_t channel);
uint16_t actuators_actuator_get_cp(uint8_t channel);
uint8_t actuators_actuator_get_dim(uint8_t channel);
uint16_t actuators_actuator_get_at(uint8_t channel);
bool actuators_actuator_get_is_cc(uint8_t channel);
bool actuators_actuator_get_is_cv(uint8_t channel);
bool actuators_actuator_get_is_at(uint8_t channel);
bool actuators_actuator_get_is_els(uint8_t channel);

#endif // ACTUATORS_H