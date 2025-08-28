/*
* PWM Module
* pwm.c
* created by: Brad Oraw
* created on: 2025-08-28
*/

#include "pwm.h"
#include "control.h"
#include "io_expander.h"
#include "resources/actuators.h"
#include "definitions.h"

uint16_t duty_cycles[NUM_ACTUATORS];
uint16_t present_duty_cycles[NUM_ACTUATORS];
uint16_t at_value;
uint16_t pwm_frequency;

const char * PWM_MODES[] = {
    "DIM_CC",
    "DIM_CV",
    "AT_CC",
    "AT_CV",
    NULL
};

#define PWM_FADE_TIME_STEP 10 // 10ms using CMD task

void pwm_set_duty_cycle(uint8_t channel, uint16_t value){    
    uint32_t value_max_watt = actuators_actuator_get_cp(channel) * value / 713;
    if(value_max_watt > PWM_DUTY_CYCLE_MAX) value = PWM_DUTY_CYCLE_MAX;
    else value = (uint16_t)value_max_watt;
    if(channel == 1){
        OC1RS = value;
    }
    if(channel == 2){
        OC5RS = PWM_DUTY_CYCLE_MAX - value; // channel 2 is inverted
    }
}

void pwm_fade_task(void){
    uint32_t duty_cycle_step;
    for(int i=0;i<NUM_ACTUATORS;i++){
        if(present_duty_cycles[i] == duty_cycles[i]) continue;
        if(actuators_actuator_get_fadetime(i+1)==0){
            present_duty_cycles[i] = duty_cycles[i];
        }
        else{
            duty_cycle_step = PWM_DUTY_CYCLE_MAX * PWM_FADE_TIME_STEP / actuators_actuator_get_fadetime(i+1);
            if(duty_cycle_step==0) duty_cycle_step = 1;
            if(present_duty_cycles[i] < duty_cycles[i]){
                if(present_duty_cycles[i]+duty_cycle_step > duty_cycles[i]){
                    present_duty_cycles[i] = duty_cycles[i];
                }
                else{
                    present_duty_cycles[i] += duty_cycle_step;
                }
            }
            else{
                if(present_duty_cycles[i]-duty_cycle_step < duty_cycles[i]){
                    present_duty_cycles[i] = duty_cycles[i];
                }
                else{
                    present_duty_cycles[i] -= duty_cycle_step;
                }
            }            
        }
        // set the duty cycle
        pwm_set_duty_cycle(i+1, present_duty_cycles[i]);
    }
}

void pwm_init(void){
    SYS_CONSOLE_PRINT("pwm: init\r\n");    
    // Use Timer2
    pwm_frequency = 20000; // 010 = 1:4 prescale value
    T2CONbits.TCKPS = 0b010; // 1:4 prescaler 
    // pwm_frequency = 1250; // 110 = 1:64 prescale value
    // T2CONbits.TCKPS = 0b110; // 1:64 prescaler 
    T2CONbits.TON = 1;
    // PR2 = CPU_CLOCK_FREQUENCY / 4 / pwm_frequency; // 80MHz / 4 / 20kHz = 1000
    PR2 = PWM_DUTY_CYCLE_MAX - 1; // period = PR2 + 1 = PWM_DUTY_CYCLE_MAX
    // PWM1 OC1
    OC1CONbits.OCTSEL = 0b00; // Timer2
    OC1CONbits.OCM = 0b110; // PWM mode, Fault pin disabled
    OC1CONbits.ON = 1;
    // PWM2 OC5
    OC5CONbits.OCTSEL = 0b00; // Timer2
    OC5CONbits.OCM = 0b110; // PWM mode, Fault pin disabled
    OC5CONbits.ON = 1;
    at_value = 4000;
    for(int i=0;i<NUM_ACTUATORS;i++){
        duty_cycles[i] = 0;
        present_duty_cycles[i] = 0;   
        pwm_set_duty_cycle(i+1, 0);
        pwm_update_mode(i+1);
    }        
}

void pwm_set_dim(uint8_t channel, uint8_t value){
    // SYS_CONSOLE_PRINT("pwm: set dim: %u, %u\r\n", channel, value);
    uint32_t new_duty_cycle = PWM_DUTY_CYCLE_MAX * value / 100;
    if(new_duty_cycle > PWM_DUTY_CYCLE_MAX) new_duty_cycle = PWM_DUTY_CYCLE_MAX;
    if(actuators_actuator_get_is_at(channel)){
        duty_cycles[0] = new_duty_cycle * (AT_MAX - at_value) / AT_RANGE;
        duty_cycles[1] = new_duty_cycle * (at_value - AT_MIN) / AT_RANGE;
    }
    else{        
        duty_cycles[channel-1] = new_duty_cycle;
    }
}

void pwm_set_at(uint16_t value){
    // SYS_CONSOLE_PRINT("pwm: set at: %u\r\n", value);
    if(value < AT_MIN) value = AT_MIN;
    if(value > AT_MAX) value = AT_MAX;
    at_value = value;
    pwm_set_dim(1, control_get_dim_value(1)); // update duty cycles for this color
}

uint16_t pwm_get_at(void){
    return at_value;
}

void pwm_update_mode(uint8_t channel){
    if(actuators_actuator_get_is_cc(channel)){
        if(channel == 1){
            io_expander_set(VCV1_EN, 0);
            io_expander_set(VCC1_EN, 1);
        }
        if(channel == 2){
            io_expander_set(VCV2_EN, 0);
            io_expander_set(VCC2_EN, 1);
        }
    }
    else{
        if(channel == 1){
            io_expander_set(VCC1_EN, 0);
            io_expander_set(VCV1_EN, 1);
        }
        if(channel == 2){
            io_expander_set(VCC2_EN, 0);
            io_expander_set(VCV2_EN, 1);
        }
    }
}
