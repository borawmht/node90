/*
* Sense Module
* sense.h
* created by: Brad Oraw
* created on: 2025-08-26
*/

#ifndef SENSE_H
#define SENSE_H

#include <stdint.h>
#include <stdbool.h>

void sense_init(void);
void sense_adc_task(void);
void sense_app_task(void);
uint16_t sense_get_actuator_current(uint8_t channel);
uint16_t sense_get_actuator_voltage(uint8_t channel);
uint16_t sense_get_actuator_power(uint8_t channel);
uint16_t sense_get_sensor_voltage(uint8_t channel);
int16_t sense_get_temperature(void);
uint16_t sense_get_poe_voltage(void);
uint16_t sense_get_v48v_voltage(void);
uint16_t sense_get_v48v_ext_voltage(void);
bool sense_get_external_power_running(void);

#endif
