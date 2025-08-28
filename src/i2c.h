/*
* I2C Module
* i2c.h
* created by: Brad Oraw
* created on: 2025-08-28
*/

#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

#define I2C_FREQUENCY 100000

void i2c_init(void);
bool i2c_detect(uint8_t address);
void i2c_wait_for_idle(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_restart(void);
void i2c_ack(void);
void i2c_nack(void);
void i2c_write(uint8_t data, bool wait_ack);
void i2c_read(uint8_t *data, bool ack_nack);

#endif