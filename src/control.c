/*
* Control Module
* control.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "control.h"
#include "resources/actuators.h"
#include "definitions.h"

uint8_t dim_values[NUM_ACTUATORS];
int32_t dim_duration[NUM_ACTUATORS];
uint8_t dim_default[NUM_ACTUATORS];
uint16_t fade_times[NUM_ACTUATORS];
uint16_t at_value;

void control_update_actuator_LEDs(void){
    if(dim_values[0]>0) DRV1_LED_Set();
    else DRV1_LED_Clear();
    if(dim_values[1]>0) DRV2_LED_Set();
    else DRV2_LED_Clear();
}

void control_init(void){
    SYS_CONSOLE_PRINT("control: init\r\n");
}

uint8_t control_getDimValue(uint8_t channel){
    return dim_values[channel-1];
}

void control_setDimValue(uint8_t channel, uint8_t new_value){
    if(strncmp(actuators_actuator_get_pwm_mode(channel),"AT",2)==0){
        dim_values[0] = new_value;
        dim_values[1] = new_value;
        SYS_CONSOLE_PRINT("control: set dim value: %u, AT\r\n", new_value);
    }
    else{
        dim_values[channel-1] = new_value;
        SYS_CONSOLE_PRINT("control: set dim value: %u, actuator%u\r\n", new_value, channel);
    }
    control_update_actuator_LEDs();
}

uint16_t control_getATValue(void){
    return at_value;
}

void control_setATValue(uint16_t new_value){
    at_value = new_value;
    SYS_CONSOLE_PRINT("control: set AT value: %u\r\n", new_value);
}

void control_setDimDuration(uint8_t channel, int32_t duration, uint8_t default_value){
    dim_duration[channel-1] = duration;
    dim_default[channel-1] = default_value;
}

void control_setFadeTime(uint8_t channel, uint16_t new_value){
    fade_times[channel-1] = new_value;
}