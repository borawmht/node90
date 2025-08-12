/*
* app.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "app.h"
#include "eeprom.h"
#include "resources.h"
#include "ethernet.h"
#include "coap.h"

APP_DATA appData;

#define SLOW_LED_PERIOD 10
#define FAST_LED_PERIOD 2

uint8_t led_stat_counter = 0; // led status counter
uint8_t led_stat_period = FAST_LED_PERIOD; // led status period

// this runs before task scheduler starts
// create all tasks before starting task scheduler
void APP_Initialize ( void ){    
    SYS_CONSOLE_PRINT("app: init tasks\r\n");    
    ethernet_init(); // start ethernet task 
    coap_init(); // start coap task
    appData.state = APP_STATE_INIT; // start app task in init state
}

// this runs after task scheduler starts
// called by task scheduler
// this is the main application loop
// the task period is 100ms
void APP_Tasks ( void ){
    // toggle status LED
    if(led_stat_counter<led_stat_period/2){
        led_stat_counter++;
    }
    else{
        led_stat_counter = 0;
        LED_STAT_Toggle();
    }  
    
    switch ( appData.state )
    {        
        case APP_STATE_INIT:
        {            
            SYS_CONSOLE_PRINT("app: init\r\n");    
            eeprom_init();
            resources_init();
            led_stat_period = SLOW_LED_PERIOD;
            // SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
            appData.state = APP_STATE_RUN;
            break;
        }

        case APP_STATE_RUN:
        {            
            // TODO: add main application logic here            
            break;
        }

        default:
        {            
            break;
        }
    }
}
