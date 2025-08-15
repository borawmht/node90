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
#include "http.h"
#include "commands.h"
#include "flash.h"
#include "firmware_update.h"

// Application signature for bootloader validation
__attribute__((section(".app_signature")))
__attribute__((aligned(4)))
const uint32_t app_signature = 0x12345678;

__attribute__((section(".app_name")))
const char app_name[] = PROJECT_NAME;

__attribute__((section(".app_version")))
const char app_version[] = PROJECT_VERSION;

__attribute__((section(".trigger_pattern")))
uint32_t trigger_pattern;
#define TRIGGER_PATTERN_ADDRESS 0xA0000000

APP_DATA appData;

#define SLOW_LED_PERIOD 10
#define FAST_LED_PERIOD 2

uint8_t led_stat_counter = 0; // led status counter
uint8_t led_stat_period = FAST_LED_PERIOD; // led status period

uint16_t app_test_counter = 0;

// this runs before task scheduler starts
// create all tasks before starting task scheduler
void APP_Initialize ( void ){
    // trigger pattern is always 0 when the app is started
    // trigger_pattern = *(uint32_t*)TRIGGER_PATTERN_ADDRESS;
    // SYS_CONSOLE_PRINT("Trigger pattern: 0x%08X\r\n", trigger_pattern);

    // SYS_CONSOLE_PRINT("app: init tasks\r\n");    
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
            // SYS_CONSOLE_PRINT("app: init\r\n");    
            eeprom_init();
            resources_init();
            commands_init();
            flash_init();
            firmware_update_init();
            led_stat_period = SLOW_LED_PERIOD;
            // SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
            appData.state = APP_STATE_RUN;
            break;
        }

        case APP_STATE_RUN:
        {            
            // TODO: add main application logic here    
            if(app_test_counter<60){
                app_test_counter++;
            }
            else if(ethernet_linkUp() && ethernet_hasIP() && app_test_counter==60){
                app_test_counter++;                
                http_client_get_url("http://httpbin.org/get", NULL, 0);
                http_client_get_url("http://192.168.1.1", NULL, 0);
            }        
            break;
        }

        default:
        {            
            break;
        }
    }
}
