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
#define DIM_DURATION_DISABLED -1

void control_init(void);
void control_output_enable_task(void);
void control_output_enable(uint8_t channel, bool enable);
void control_output_protection_delay(int32_t delay);
void control_update_pwm_mode(uint8_t channel);
uint8_t control_get_dim_value(uint8_t channel);
void control_set_dim_value(uint8_t channel, uint8_t new_value);
uint16_t control_get_at_value();
void control_set_at_value(uint16_t new_value);
void control_set_dim_duration(uint8_t channel, int32_t duration, uint8_t default_value);
void control_set_voltage(uint8_t channel, uint16_t voltage);

#endif