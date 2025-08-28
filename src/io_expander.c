/*
* IO Expander Module
* io_expander.c
* created by: Brad Oraw
* created on: 2025-08-28
*/

#include "io_expander.h"
#include "i2c.h"
#include "definitions.h"

uint8_t io_expander_address;

void io_expander_write(uint8_t reg, uint8_t val){
    if(io_expander_address==IO_EXPANDER_NOT_FOUND_ADDRESS){
        return;
    }
    i2c_start(); /* Send start condition */
    i2c_write(io_expander_address<<1, 1); /* Send address, read/write bit not set (AD + R) */
    i2c_write(reg, 1); /* Send the register address */
    i2c_write(val, 1); /* Send the value */
    i2c_stop(); /* Send stop condition */
}

uint8_t io_expander_read(uint8_t reg){
    if(io_expander_address==IO_EXPANDER_NOT_FOUND_ADDRESS){
        return 0;
    }
    uint8_t value;
    i2c_start(); /* Send start condition */
    i2c_write((io_expander_address<<1), 1); /* Send address, read/write bit not set (AD + R) */
    i2c_write(reg, 1); /* Send the register address (RA) */
    i2c_restart(); /* Send repeated start condition */
    i2c_write((io_expander_address<<1)|1, 1); /* Send address, read/write bit set (AD + W) */
    i2c_read(&value, 1); /* Read value from the I2C bus */
    i2c_stop(); /* Send stop condition */
    return value;
}

void io_expander_init(void){
    SYS_CONSOLE_PRINT("io_expander: init\r\n");
    i2c_init();
    io_expander_address = TCA9534_ADDRESS; // set the address
    if(!i2c_detect(io_expander_address)){
        io_expander_address = IO_EXPANDER_ALTERNATE_ADDRESS;
        if(!i2c_detect(io_expander_address)){
            io_expander_address = IO_EXPANDER_NOT_FOUND_ADDRESS;
            SYS_CONSOLE_PRINT("io_expander: not found\r\n");
        }        
    }
    io_expander_write(0x03,0x00); // configure all outputs
    io_expander_write(0x01,0x00); // clear all outputs
}

void io_expander_set(uint8_t pin, bool value){
    if(value){
        io_expander_write(0x01, io_expander_read(0x01)|(0x01<<pin));
    }
    else{
        io_expander_write(0x01, io_expander_read(0x01)&(~(0x01<<pin)));
    }
}