/*
* PWM Module
* pwm.h
* created by: Brad Oraw
* created on: 2025-08-28
*/

#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PWM_MODE_SIZE 16

#define PWM_DUTY_CYCLE_MAX 1000

#define AT_MIN 3000
#define AT_MAX 5000
#define AT_RANGE (AT_MAX-AT_MIN)

void pwm_init(void);
void pwm_fade_task(void);
void pwm_set_dim(uint8_t channel, uint8_t value);
void pwm_set_at(uint16_t value);
uint16_t pwm_get_at(void);
void pwm_set_present_duty_cycle(uint8_t channel, uint16_t value);

#endif // PWM_H