/*
* Control Module
* control.h
* created by: Brad Oraw
* created on: 2025-08-26
*/

#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#define DIM_NO_CHANGE 101
#define DIM_MAX 100
#define DIM_MIN 0
#define COLOR_NO_CHANGE 256

void control_init(void);
void control_update_pwm_mode(uint8_t channel);
uint8_t control_getDimValue(uint8_t channel);
void control_setDimValue(uint8_t channel, uint8_t new_value);
uint16_t control_getATValue();
void control_setATValue(uint16_t new_value);
void control_setDimDuration(uint8_t channel, int32_t duration, uint8_t default_value);

#endif