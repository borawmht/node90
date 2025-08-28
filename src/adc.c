/*
* ADC Module
* adc.c
* created by: Brad Oraw
* created on: 2025-08-28
*/

#include "adc.h"
#include "definitions.h"

bool adc_initialized = false;

void adc_init(void){
    if(adc_initialized){
        return;
    }
    adc_initialized = true;
    SYS_CONSOLE_PRINT("adc: init\r\n");
    AD1CON3bits.ADRC = 0; // Clock derived from Peripheral Bus Clock (PBCLK)
    AD1CON3bits.SAMC = 12; // 12 TAD * 100ns = 1.2us
    AD1CON3bits.ADCS = 8; // 8 * TPB = TAD = 100ns
    AD1CON2bits.VCFG = 0b000; // VR+ = AVDD, VR- = AVSS
    AD1CON1bits.FORM = 0b000; // Integer 16-bit (DOUT = 0000 0000 0000 0000 0000 00dd dddd dddd)
    AD1CON1bits.SSRC = 0b111; // Internal counter ends sampling and starts conversion (auto convert)
    AD1CON1bits.ASAM = 0; // Sampling begins when SAMP bit is set
    // AD1CON1bits.ASAM = 1; // Auto sample mode
    AD1CON1bits.ON = 1;    
}

uint16_t adc_read(uint8_t channel){
    if(adc_initialized == false){
        return 0;
    }    
    
    // Set the channel to convert
    AD1CHSbits.CH0SA = channel;
    
    // Start sampling (since ASAM = 0 in init)
    AD1CON1bits.SAMP = 1;  
    
    // Wait for sampling to complete (SAMP bit will clear automatically)
    // The sampling time is controlled by SAMC bits
    while(AD1CON1bits.SAMP);

    // Wait for conversion to complete
    while(!AD1CON1bits.DONE);    
    
    return ADC1BUF0;
}
