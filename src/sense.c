/*
* Sense Module
* sense.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "sense.h"
#include "adc.h"
#include "resources/actuators.h"
#include "resources/sensors.h"
#include "resources/event.h"
#include "definitions.h"

uint8_t adc_scan_index = 0;

typedef struct{
    uint8_t channel;
    uint16_t value;
    uint16_t scale;
    uint16_t divisor;
}adc_channel_t;

#define NUM_ADC_CHANNELS 10
adc_channel_t adc_channels[NUM_ADC_CHANNELS] = {
    {ADC_V48V_EXT, 0, 212, 12},
    {ADC_VLED1, 0, 210, 10},
    {ADC_VLED2, 0, 210, 10},
    {ADC_VPOE, 0, 212, 12},
    {ADC_AUX_AN1, 0, 122, 22},
    {ADC_ILED2, 0, 30, 11},
    {ADC_ILED1, 0, 30, 11},
    {ADC_V48V, 0, 212, 12},
    {ADC_V010V, 0, 542, 67},
    {ADC_VNTC, 0, 1, 1},
};

uint16_t i_cv_scale = 5;
uint16_t i_cv_divisor = 2;

typedef struct{
    uint8_t voltage_channel;
    uint8_t current_channel;
    uint16_t voltage;
    uint16_t current;
    uint16_t power;
    uint8_t dim_save;
}sense_actuator_t;

sense_actuator_t sense_actuators[] = {
    {ADC_VLED1,ADC_ILED1, 0, 0, 0, 0},
    {ADC_VLED2,ADC_ILED2, 0, 0, 0, 0},
};

uint16_t sensor1_voltage = 0;
bool sensor1_input_state = false;
bool sensor1_logical_state = false;

uint16_t poe_voltage = 0;
uint16_t v48v_voltage = 0;
uint16_t v48v_ext_voltage = 0;
bool external_power_running = false;

int16_t temperature_x10_C = 0;

void sense_adc_task(void){
    adc_channels[adc_scan_index].value = adc_read(adc_channels[adc_scan_index].channel);
    adc_scan_index++;
    if(adc_scan_index >= NUM_ADC_CHANNELS){
        adc_scan_index = 0;
    }
}

void sense_temperature_task(void){
    uint16_t val = adc_channels[9].value;
    if(val<600){
        temperature_x10_C=4676-(int16_t)val*7;
    }
    else{
        temperature_x10_C=1649-(int16_t)val*2;
    }
}

uint16_t sense_get_adc_voltage(uint8_t channel){    
    uint8_t i;
    for(i = 0; i < NUM_ADC_CHANNELS; i++){
        if(adc_channels[i].channel == channel){
            break;
        }
    }
    if(i >= sizeof(adc_channels)/sizeof(adc_channels[0])){
        return 0;
    }
    uint32_t voltage = (uint32_t)adc_channels[i].value * adc_channels[i].scale / adc_channels[i].divisor;
    voltage = voltage * 3300 / 1024;
    // SYS_CONSOLE_PRINT("sense: adc_voltage: %u\r\n", voltage);
    return (uint16_t)voltage;
}

void sense_poe_task(void){
    v48v_voltage = sense_get_adc_voltage(ADC_V48V);
    v48v_ext_voltage = sense_get_adc_voltage(ADC_V48V_EXT);
    poe_voltage = sense_get_adc_voltage(ADC_VPOE);
    if(poe_voltage < 12000 && !external_power_running){
        SYS_CONSOLE_PRINT("sense: lost PoE\r\n");
        EN_48V_EXT_Set();
        external_power_running = true;
        if(actuators_actuator_get_is_els(1)){            
            SYS_CONSOLE_PRINT("sense: actuator1: els running\r\n");
            sense_actuators[0].dim_save = actuators_actuator_get_dim(1);
            actuators_actuator_set_dim(1, actuators_actuator_get_dim_els(1));            
        }
        if(actuators_actuator_get_is_els(2)){
            SYS_CONSOLE_PRINT("sense: actuator2: els running\r\n");
            sense_actuators[1].dim_save = actuators_actuator_get_dim(2);
            actuators_actuator_set_dim(2, actuators_actuator_get_dim_els(2));
        }
    }
    else if(poe_voltage > 16000 && external_power_running){
        SYS_CONSOLE_PRINT("sense: resume PoE\r\n");
        EN_48V_EXT_Clear();
        external_power_running = false;
        if(actuators_actuator_get_is_els(1)){
            SYS_CONSOLE_PRINT("sense: actuator1: els stopped\r\n");
            actuators_actuator_set_dim(1, sense_actuators[0].dim_save);
        }
        if(actuators_actuator_get_is_els(2)){
            SYS_CONSOLE_PRINT("sense: actuator2: els stopped\r\n");
            actuators_actuator_set_dim(2, sense_actuators[1].dim_save);
        }
    }
}

void sense_actuator_task(void){
    for(uint8_t i = 0; i < NUM_ACTUATORS; i++){
        sense_actuators[i].voltage = sense_get_adc_voltage(sense_actuators[i].voltage_channel);
        sense_actuators[i].current = sense_get_adc_voltage(sense_actuators[i].current_channel);
        if(actuators_actuator_get_is_cv(i+1)){
            sense_actuators[i].current = (uint16_t)((uint32_t)sense_actuators[i].current * i_cv_scale / i_cv_divisor);
        }
        sense_actuators[i].power = sense_actuators[i].voltage / 100 * (sense_actuators[i].current/100);
        // SYS_CONSOLE_PRINT("sense: actuator %d: voltage: %u, current: %u, power: %u\r\n", i, sense_actuators[i].voltage, sense_actuators[i].current, sense_actuators[i].power);
    }
}

uint8_t sense_sensor1_counter = 0;
void sense_sensor1_task(void){
    sensor1_voltage = sense_get_adc_voltage(ADC_AUX_AN1);
    if(sensor1_voltage > sensors_sensor_get_high_threshold(1) && !sensor1_input_state){
        sensor1_input_state = true;
        SYS_CONSOLE_PRINT("sense: sensor1: input state: true\r\n");
        if(sensors_sensor_get_is_input_type(1)){
            event_send_key_value(
                ethernet_getBroadcastAddressString(), 
                sensors_sensor_get_eventlh(1), 
                sensors_sensor_get_cluster(1), 
                true
            );
            event_execute_key_value(
                sensors_sensor_get_eventlh(1), 
                sensors_sensor_get_cluster(1)
            );
        }
    }
    else if(sensor1_voltage < sensors_sensor_get_low_threshold(1) && sensor1_input_state){
        sensor1_input_state = false;
        SYS_CONSOLE_PRINT("sense: sensor1: input state: false\r\n");
        if(sensors_sensor_get_is_input_type(1)){
            event_send_key_value(
                ethernet_getBroadcastAddressString(), 
                sensors_sensor_get_eventhl(1), 
                sensors_sensor_get_cluster(1), 
                true
            );
            event_execute_key_value(
                sensors_sensor_get_eventhl(1), 
                sensors_sensor_get_cluster(1)
            );
        }
    }
    sense_sensor1_counter++; // 100ms
    if(sense_sensor1_counter>=30){
        //SYS_CONSOLE_PRINT("sense: sensor1: %u mV\r\n", sensor1_voltage);
        sense_sensor1_counter = 0;
    }
    
}

void sense_app_task(void){
    sense_sensor1_task();
    sense_actuator_task();
    sense_poe_task();
    sense_temperature_task();
}

void sense_init(void){
    SYS_CONSOLE_PRINT("sense: init\r\n");
    adc_init();
}

uint16_t sense_get_actuator_current(uint8_t channel){
    if(channel == 1){
        return sense_actuators[0].current;
    }
    else if(channel == 2){
        return sense_actuators[1].current;
    }
    return 0;
}

uint16_t sense_get_actuator_voltage(uint8_t channel){
    if(channel == 1){
        return sense_actuators[0].voltage;
    }
    else if(channel == 2){
        return sense_actuators[1].voltage;
    }
    return 0;
}

uint16_t sense_get_actuator_power(uint8_t channel){
    if(channel == 1){
        return sense_actuators[0].power;
    }
    else if(channel == 2){
        return sense_actuators[1].power;
    }
    return 0;
}

uint16_t sense_get_sensor_voltage(uint8_t channel){
    return sensor1_voltage;
}


