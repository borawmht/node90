/*
* DAC Module
* dac.c
* created by: Brad Oraw
* created on: 2025-08-28
*/

#include "dac.h"
#include "i2c.h"
#include "resources/actuators.h"
#include "definitions.h"

uint8_t dac_address;
bool dac_initialized = false;

void dac_set_channel_level(uint8_t channel, uint8_t level){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("dac: set_level: channel out of range: %d\r\n", channel);
        return;
    }
    if(channel==1){
        dac_write(WRITE_CHANNEL_D, level);
    }
    else if(channel==2){
        dac_write(WRITE_CHANNEL_C, level);
    }
}

void dac_set_010V_level(uint8_t level){
    dac_write(WRITE_CHANNEL_A, level);
}

void dac_write(uint8_t reg_address, uint8_t dim_level){
    if(dac_address==DAC_NOT_FOUND_ADDRESS){
        return;
    }
    uint16_t value;
    uint32_t value32;
    uint8_t reg;
    if(dim_level>100){
        dim_level=100;
    } // maximum allowable value

    bool linear_dimming_1 = false;
    bool linear_dimming_2 = false;
    
    // MCP4728
    // 12bit (4096 full scale), 3.3V reference, 8.14V/V opamp gain
    // scale = 10V * 4096 / 8.14V/V / 3.3V = 1525
    // rev3 scale = 10V * 4096 / 3.13V/V / 3.3V = 3966 * 10V/9.8V = 4046 calibration 
    // scale = 0.25V * 4096 / 1V/V / 3.3V = 310
    //
    // DS3911
    // 10bit (1024 full scale), 2.5V reference, 8.14V/V opamp gain
    // scale = 10V * 1024 / 8.14V/V / 2.5V = 503
    // rev3 scale = 10V * 1024 / 3.13V/V / 2.5V = 1309 * 10V/9.8V = 1335 calibration
    // scale = 0.25V * 1024 / 1V/V / 2.5V = 102
    if(dac_address==MCP4728_ADDRESS){
        if((linear_dimming_1&&reg_address==WRITE_CHANNEL_D)||
                (linear_dimming_2&&reg_address==WRITE_CHANNEL_C)){
            value=((uint16_t)dim_level*310)/100;
        }
        else if(!linear_dimming_1&&reg_address==WRITE_CHANNEL_D){
            value32 = ((uint32_t)dim_level*310)/100;
            if(actuators_actuator_get_cc(1)<MAX_MAXA){
                value32 *= (uint32_t)actuators_actuator_get_cc(1);
                value32 /= (uint32_t)MAX_MAXA;                
            }
            value = (uint16_t)value32;
            //SYS_CONSOLE_PRINT("LD1 %u\r\n",value);
        }
        else if(!linear_dimming_2&&reg_address==WRITE_CHANNEL_C){
            value32 = ((uint32_t)dim_level*310)/100;
            if(actuators_actuator_get_cc(2)<MAX_MAXA){
                value32 *= (uint32_t)actuators_actuator_get_cc(2);
                value32 /= (uint32_t)MAX_MAXA;                
            }
            value = (uint16_t)value32;
            //SYS_CONSOLE_PRINT("LD2 %u\r\n",value);
        }
        else value=((uint16_t)dim_level*1525)/100;
        reg=reg_address;
    }
    else{
        if((linear_dimming_1&&reg_address==WRITE_CHANNEL_D)||
                (linear_dimming_2&&reg_address==WRITE_CHANNEL_C)){
            value=((uint16_t)dim_level*102)/100;
        }
        else if(!linear_dimming_1&&reg_address==WRITE_CHANNEL_D){
            value32 = ((uint32_t)dim_level*102)/100;
            if(actuators_actuator_get_cc(1)<MAX_MAXA){
                value32 *= (uint32_t)actuators_actuator_get_cc(1);
                value32 /= (uint32_t)MAX_MAXA;                
            }
            value = (uint16_t)value32;
            //SYS_CONSOLE_PRINT("LD1 %u\r\n",value);
        }
        else if(!linear_dimming_2&&reg_address==WRITE_CHANNEL_C){
            value32 = ((uint32_t)dim_level*102)/100;
            if(actuators_actuator_get_cc(2)<MAX_MAXA){
                value32 *= (uint32_t)actuators_actuator_get_cc(2);
                value32 /= (uint32_t)MAX_MAXA;                
            }
            value = (uint16_t)value32;
            //SYS_CONSOLE_PRINT("LD2 %u\r\n",value);
        }
        else value=((uint16_t)dim_level*503)/100;
        if(value>0x3FF){
            value=0x3FF;
        }
        value<<=6; // left justified
        reg=0x16-(reg_address-0x40);
    }
    //SYS_CONSOLE_PRINT("DAC_write reg = 0x%02X, value = %u\r\n",reg,value);
    i2c_start(); // start
    i2c_write(dac_address<<1, 1); // 7bit address plus write bit
    i2c_write(reg, 1); // register address
    i2c_write((value>>8) & 0xFF, 1); // upper byte
    i2c_write(value&0xFF, 1); // lower byte
    i2c_stop(); // stop
}

// Read a byte from register at reg_address and return in *value
void dac_read(uint8_t reg_address, uint8_t *value){
    if(dac_address!=MCP4728_ADDRESS){
        return;
    }
    i2c_start(); /* Send start condition */
    i2c_write(MCP4728_ADDRESS, 1); /* Send address, read/write bit not set (AD + R) */
    i2c_write(reg_address, 1); /* Send the register address (RA) */
    i2c_restart(); /* Send repeated start condition */
    i2c_write(MCP4728_ADDRESS|1, 1); /* Send address, read/write bit set (AD + W) */
    i2c_read(value, 1); /* Read value from the I2C bus */
    i2c_stop(); /* Send stop condition */
}

void dac_init(void){
    if(dac_initialized) return;
    dac_initialized = true;
    SYS_CONSOLE_PRINT("dac: init\r\n");
    i2c_init();
    dac_address = MCP4728_ADDRESS;
    if(!i2c_detect(dac_address)){
        dac_address = DS3911_ADDRESS;
        if(!i2c_detect(dac_address)){
            dac_address=DAC_NOT_FOUND_ADDRESS;
            SYS_CONSOLE_PRINT("dac: device not found\r\n");
        }
        else{
            dac_write(0x78, 0); // POR zero and disable LUT, enable manual value
            dac_write(0x7A, 0);
            dac_write(0x7C, 0);
            dac_write(0x7E, 0);
        }
    }
    dac_set_channel_level(1, 100);
    dac_set_channel_level(2, 100);
    dac_set_010V_level(0);
}