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
uint16_t sense_get_actuator_current(uint8_t channel);
uint16_t sense_get_actuator_voltage(uint8_t channel);
uint16_t sense_get_actuator_power(uint8_t channel);

#endif
