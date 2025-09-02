/*
* Control Module
* control.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "control.h"
#include "pwm.h"
#include "io_expander.h"
#include "dac.h"
#include "resources/actuators.h"
#include "definitions.h"

uint8_t dim_values[NUM_ACTUATORS];
int32_t dim_duration[NUM_ACTUATORS];
uint16_t dim_duration_seconds[NUM_ACTUATORS];
uint8_t dim_default[NUM_ACTUATORS];
uint8_t dim_duration_task_counter = 0;
bool channel_enabled[NUM_ACTUATORS];
bool channel_enable_request[NUM_ACTUATORS];

void control_update_actuator_LEDs(void){
    if(dim_values[0]>0) DRV1_LED_Set();
    else DRV1_LED_Clear();
    if(dim_values[1]>0) DRV2_LED_Set();
    else DRV2_LED_Clear();
}

// output enable task runs every 100ms
void control_output_enable_task(void){
    for(int i=0;i<NUM_ACTUATORS;i++){
        if(channel_enable_request[i]){
            control_output_enable(i+1, true);
            channel_enable_request[i] = false;
        }
    }
}

// dim duration task runs every 100ms
void control_dim_duration_task(void){
    dim_duration_task_counter++;
    if(dim_duration_task_counter<10) return;
    dim_duration_task_counter = 0;
    for(int i=0;i<NUM_ACTUATORS;i++){
        if(dim_duration[i]>=0 && dim_default[i]!=DIM_NO_CHANGE){
            dim_duration_seconds[i]++;
            if(dim_duration_seconds[i]==60){
                dim_duration_seconds[i] = 0;
                if(dim_duration[i]>0) dim_duration[i]--;                
                if(dim_duration[i]>=0){
                    SYS_CONSOLE_PRINT("control: actuator%u dim duration %d, dim default %u\r\n",i+1,dim_duration[i],dim_default[i]);
                }
                if((dim_duration[i]==0)&&(dim_default[i]<DIM_NO_CHANGE)){
                    control_set_dim_value(i+1, dim_default[i]);
                    dim_default[i] = DIM_NO_CHANGE;
                }
            }
        }
    }
}

void control_init(void){
    SYS_CONSOLE_PRINT("control: init\r\n");
    io_expander_init();
    dim_duration_task_counter = 0;
    for(int i=0;i<NUM_ACTUATORS;i++){
        channel_enabled[i] = false;
        channel_enable_request[i] = false;
        dim_values[i] = 0;
        dim_duration[i] = DIM_DURATION_DISABLED;
        dim_default[i] = DIM_NO_CHANGE;
        dim_duration_seconds[i] = 0;
        //control_output_enable(i+1, false);
    }    
    dac_init();
    pwm_init();
    EN_48V_Set();
    for(int i=0;i<NUM_ACTUATORS;i++){
        channel_enable_request[i] = true;
    }
}

void control_update_pwm_mode(uint8_t channel){
    if(channel_enabled[channel-1]){
        channel_enable_request[channel-1] = true;
    }
}

uint8_t control_get_dim_value(uint8_t channel){
    return dim_values[channel-1];
}

void control_set_dim_value(uint8_t channel, uint8_t new_value){
    if(actuators_actuator_get_is_at(channel)){
        bool changed = dim_values[0] != new_value;
        changed |= dim_values[0] != new_value;
        dim_values[0] = new_value;
        dim_values[1] = new_value;
        if(changed){
            SYS_CONSOLE_PRINT("control: set dim value: %u, AT\r\n", new_value);
        }
    }
    else{
        bool changed = dim_values[channel-1] != new_value;
        dim_values[channel-1] = new_value;
        if(changed){
            SYS_CONSOLE_PRINT("control: set dim value: %u, actuator%u\r\n", new_value, channel);
        }
    }
    pwm_set_dim(channel, new_value);
    dac_set_channel_level(channel, 100); // update channel level
    dac_set_010V_level(new_value);
    control_update_actuator_LEDs();
}

uint16_t control_get_at_value(void){
    return pwm_get_at();
}

void control_set_at_value(uint16_t new_value){
    SYS_CONSOLE_PRINT("control: set AT value: %u\r\n", new_value);
    pwm_set_at(new_value);
}

void control_set_dim_duration(uint8_t channel, int32_t duration, uint8_t default_value){
    dim_duration[channel-1] = duration;
    dim_default[channel-1] = default_value;
}

#define CUV_VALUES_LENGTH 17
const uint16_t cuv_values[CUV_VALUES_LENGTH] = {
    0,
    12000,14000,17000,19000,
    22000,24000,27000,29000,
    31000,33000,36000,38000,
    41000,43000,45000,48000
};

void control_set_voltage(uint8_t channel, uint16_t voltage){    
    if(actuators_actuator_get_cuv_enable(channel)){
        uint8_t i;
        for(i=1; i<CUV_VALUES_LENGTH; i++){
            if(cuv_values[i]==voltage) break;
        }
        if(i>=CUV_VALUES_LENGTH) return;
        i -= 1; // index starts at 0
        if(i%4==0){
            io_expander_set(VCV1_24V,false);
            io_expander_set(VCV1_36V,false);
        }
        if(i%4==1){
            io_expander_set(VCV1_24V,true);
            io_expander_set(VCV1_36V,false);
        }
        if(i%4==2){
            io_expander_set(VCV1_24V,false);
            io_expander_set(VCV1_36V,true);
        }
        if(i%4==3){
            io_expander_set(VCV1_24V,true);
            io_expander_set(VCV1_36V,true);
        }
        if(i/4==0){
            io_expander_set(VCV2_24V,false);
            io_expander_set(VCV2_36V,false);
        }
        if(i/4==1){
            io_expander_set(VCV2_24V,true);
            io_expander_set(VCV2_36V,false);
        }
        if(i/4==2){
            io_expander_set(VCV2_24V,false);
            io_expander_set(VCV2_36V,true);
        }
        if(i/4==3){
            io_expander_set(VCV2_24V,true);
            io_expander_set(VCV2_36V,true);
        }
    }
    else{   
        uint8_t VCV_36V = VCV1_36V;
        uint8_t VCV_24V = VCV1_24V;
        if(channel == 2){
            VCV_36V = VCV2_36V;
            VCV_24V = VCV2_24V;
        }     
        if(voltage<=12000){
            io_expander_set(VCV_24V, false);
            io_expander_set(VCV_36V, false);
        }
        else if(voltage<=24000){
            io_expander_set(VCV_24V, true);
            io_expander_set(VCV_36V, false);
        }
        else if(voltage<=36000){
            io_expander_set(VCV_24V, false);
            io_expander_set(VCV_36V, true);
        }
        else{
            io_expander_set(VCV_24V, true);
            io_expander_set(VCV_36V, true);
        }
    }
}

void control_output_protection_delay(int32_t delay){
    while(delay>0){
        SYS_CONSOLE_PRINT("control: output protection delay: %d\r\n", delay/1000);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        delay -= 1000;
    }
    SYS_CONSOLE_PRINT("control: output protection delay: done\r\n");
}

void control_output_enable(uint8_t channel, bool enable){
    if(!enable){
        SYS_CONSOLE_PRINT("control: actuator%u enable: %s\r\n", channel, enable ? "true" : "false");
    }
    bool is_cc = actuators_actuator_get_is_cc(channel);
    if(is_cc){
        if(channel == 1){
            io_expander_set(VCV1_EN, 0);
            io_expander_set(VCC1_EN, 0);
            if(!enable) return;
            if(channel_enabled[0]){
                control_output_protection_delay(6000);
                pwm_set_present_duty_cycle(1, 0);
            }
            io_expander_set(VCC1_EN, 1);
            channel_enabled[0] = true;
        }
        if(channel == 2){
            io_expander_set(VCV2_EN, 0);
            io_expander_set(VCC2_EN, 0);
            if(!enable) return;
            if(channel_enabled[1]){
                control_output_protection_delay(6000);
                pwm_set_present_duty_cycle(2, 0);
            }
            io_expander_set(VCC2_EN, 1);
            channel_enabled[1] = true;
        }
    }
    else{
        if(channel == 1){
            io_expander_set(VCC1_EN, 0);
            io_expander_set(VCV1_EN, 0);
            if(!enable) return;
            if(channel_enabled[0]){
                control_output_protection_delay(6000);
                pwm_set_present_duty_cycle(1, 0);
            }
            io_expander_set(VCV1_EN, 1);
            channel_enabled[0] = true;            
        }
        if(channel == 2){
            io_expander_set(VCC2_EN, 0);
            io_expander_set(VCV2_EN, 0);
            if(!enable) return;
            if(channel_enabled[1]){
                control_output_protection_delay(6000);
                pwm_set_present_duty_cycle(2, 0);
            }
            io_expander_set(VCV2_EN, 1);
            channel_enabled[1] = true;
        }
    }
    SYS_CONSOLE_PRINT("control: actuator%u enable: %s, %s\r\n", channel, enable ? "true" : "false", is_cc ? "CC" : "CV");
}