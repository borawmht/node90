/*
* I2C Module
* i2c.c
* created by: Brad Oraw
* created on: 2025-08-28
*/

#include "i2c.h"
#include "definitions.h"

bool i2c_is_initialized = false;

bool i2c_detect(uint8_t address){
    i2c_start();
    I2C1TRN=(address<<1)|0; // Send slave address with Read/Write bit cleared
    while(I2C1STATbits.TBF==1); // Wait until transmit buffer is empty
    i2c_wait_for_idle(); // Wait until I2C bus is idle
    uint16_t wait_counter=0;
    while((I2C1STATbits.ACKSTAT==1)&&(wait_counter++<10000));
    return (wait_counter<10000)?(true):(false);
}

// value is the value of the data we want to send, set ack_nack to 0 to send an ACK or anything else to send a NACK
void i2c_read(uint8_t *value, bool ack_nack){
    I2C1CONbits.RCEN=1; // Receive enable
    while(I2C1CONbits.RCEN); // Wait until RCEN is cleared (automatic)
    while(!I2C1STATbits.RBF); // Wait until Receive Buffer is Full (RBF flag)
    *value=I2C1RCV; // Retrieve value from I2C1RCV
    if(!ack_nack) // Do we need to send an ACK or a NACK?
        i2c_ack(); // Send ACK
    else
        i2c_nack(); // Send NACK
}

// address is I2C slave address, set wait_ack to 1 to wait for ACK bit or anything else to skip ACK checking
void i2c_write(uint8_t address, bool wait_ack){
    I2C1TRN=address|0; // Send slave address with Read/Write bit cleared
    while(I2C1STATbits.TBF==1); // Wait until transmit buffer is empty
    i2c_wait_for_idle(); // Wait until I2C bus is idle
    if(wait_ack) while(I2C1STATbits.ACKSTAT==1); // Wait until ACK is received
}

// i2c_init() initialises I2C1 at at frequency of [frequency]Hz
void i2c_init(void){
    if(i2c_is_initialized){
        return;
    }
    i2c_is_initialized = true;
    double frequency = I2C_FREQUENCY;
    double BRG;
    // I2C pins are open drain, I2C module should control data direction
    ODCDbits.ODCD9=1; // SDA
    ODCDbits.ODCD10=1; // SCL
    //TRISDbits.TRISD9 = 1;       // SDA is an input
    //TRISDbits.TRISD10 = 0;      // SCL is an output
    I2C1CON=0; // Turn off I2C1 module
    I2C1CONbits.DISSLW=1; // Disable slew rate for 100kHz
    // BRG = FPBCLK/(2*FSCK) - 1 - (FPBCLK*130ns)/2)
    BRG=(1/frequency)-0.000000104;
    BRG=(BRG*(CPU_CLOCK_FREQUENCY/2))-1;
    I2C1BRG=(uint16_t)BRG; // Set baud rate
    I2C1CONbits.ON=1; // Turn on I2C1 module
}

// i2c_wait_for_idle() waits until the I2C peripheral is no longer doing anything
void i2c_wait_for_idle(void){
    while(I2C1CON&0x1F); // Acknowledge sequence not in progress
    // Receive sequence not in progress
    // Stop condition not in progress
    // Repeated Start condition not in progress
    // Start condition not in progress
    while(I2C1STATbits.TRSTAT); // Bit = 0 ? Master transmit is not in progress
}

// i2c_start() sends a start condition
void i2c_start(){
    i2c_wait_for_idle();
    I2C1CONbits.SEN=1;
    while(I2C1CONbits.SEN==1);
}

// i2c_stop() sends a stop condition
void i2c_stop(){
    i2c_wait_for_idle();
    I2C1CONbits.PEN=1;
}

// i2c_restart() sends a repeated start/restart condition
void i2c_restart(){
    i2c_wait_for_idle();
    I2C1CONbits.RSEN=1;
    while(I2C1CONbits.RSEN==1);
}

// i2c_ack() sends an ACK condition
void i2c_ack(void){
    i2c_wait_for_idle();
    I2C1CONbits.ACKDT=0; // Set hardware to send ACK bit
    I2C1CONbits.ACKEN=1; // Send ACK bit, will be automatically cleared by hardware when sent
    while(I2C1CONbits.ACKEN); // Wait until ACKEN bit is cleared, meaning ACK bit has been sent
}

// i2c_nack() sends a NACK condition
void i2c_nack(void){
    i2c_wait_for_idle();
    I2C1CONbits.ACKDT=1; // Set hardware to send NACK bit
    I2C1CONbits.ACKEN=1; // Send NACK bit, will be automatically cleared by hardware when sent
    while(I2C1CONbits.ACKEN); // Wait until ACKEN bit is cleared, meaning NACK bit has been sent
}