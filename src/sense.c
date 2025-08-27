/*
* Sense Module
* sense.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "sense.h"
#include "resources/actuators.h"
#include "definitions.h"

void sense_init(void){
    SYS_CONSOLE_PRINT("sense: init\r\n");
}

uint16_t sense_get_actuator_current(uint8_t channel){
    return 0;
}

uint16_t sense_get_actuator_voltage(uint8_t channel){
    return 0;
}

uint16_t sense_get_actuator_power(uint8_t channel){
    return 0;
}

uint16_t sense_get_sensor_voltage(uint8_t channel){
    return 0;
}


