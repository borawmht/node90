/*
* ADC Module
* adc.h
* created by: Brad Oraw
* created on: 2025-08-28
*/

#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

#define ADC_V48V_EXT 2
#define ADC_VLED1 3
#define ADC_VLED2 4
#define ADC_VPOE 5
#define ADC_AUX_AN1 7
#define ADC_ILED2 10
#define ADC_ILED1 11
#define ADC_V48V 12
#define ADC_V010V 13
#define ADC_VNTC 14

void adc_init(void);
uint16_t adc_read(uint8_t channel);

#endif