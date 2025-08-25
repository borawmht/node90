/*
* bootloader
* main.c  
*/

#include <stddef.h>                     // Defines NULL
#include <stdint.h>
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <xc.h>
#include "definitions.h"                // SYS function prototypes
#include "project_version.h"
#include "firmware_update.h"
#include "flash.h"

__attribute__((section(".bootloader_name")))
const char bootloader_name[] = PROJECT_NAME;

__attribute__((section(".bootloader_version")))
const char bootloader_version[] = PROJECT_VERSION;

// __attribute__((section(".trigger_pattern")))
uint32_t trigger_pattern;
#define TRIGGER_PATTERN_ADDRESS 0xA0000000

// Application entry point
#define APP_START_ADDRESS    0x9D008000
#define APP_VALID_SIGNATURE  0x12345678

typedef void (*app_function_t)(void);

void check_reset_source(void) {
    printf("bootloader: reset: ");

    // read the trigger pattern from the trigger pattern address
    trigger_pattern = *(uint32_t*)TRIGGER_PATTERN_ADDRESS;
    
    if (RCONbits.POR) {
        printf("Power-on Reset\r\n");
        // RCONbits.POR = 0;  // Clear the bit
        trigger_pattern = 0; // clear the trigger pattern
    }
    
    else if (RCONbits.BOR) {
        printf("Brown-out Reset\r\n");
        // RCONbits.BOR = 0;  // Clear the bit
    }
    
    else if (RCONbits.SWR) {
        printf("Software Reset\r\n");
        // RCONbits.SWR = 0;  // Clear the bit        
    }
    
    // else if (RCONbits.WDT) {
    //     printf("Watchdog Timer Reset\r\n");
    //     RCONbits.WDT = 0;  // Clear the bit
    // }
    
    else if (RCONbits.EXTR) {
        printf("External Reset\r\n");
        // RCONbits.EXTR = 0;  // Clear the bit
        trigger_pattern = 0; // clear the trigger pattern
    }
    
    // If none of the above are set, it might be a different type
    else {
        printf("  Unknown reset source\r\n");
        trigger_pattern = 0; // clear the trigger pattern
    }

    RCONbits.BOR = 0;  // Clear the bit
    RCONbits.SWR = 0;  // Clear the bit
    RCONbits.EXTR = 0;  // Clear the bit
    RCONbits.POR = 0;  // Clear the bit
}

bool check_application_valid(void) {
    // Check if application has valid signature
    uint32_t *app_signature = (uint32_t*)(APP_START_ADDRESS + 0x1000);
    printf("bootloader: application signature: 0x%08X\r\n", *app_signature);
    return (*app_signature == APP_VALID_SIGNATURE);
}

void jump_to_application(void) {
    // trigger_pattern = 0; // clear the trigger pattern
    // *(uint32_t*)TRIGGER_PATTERN_ADDRESS = trigger_pattern;
    printf("bootloader: load application\r\n");
    app_function_t app_main = (app_function_t)APP_START_ADDRESS;
    
    // Disable interrupts
    __builtin_disable_interrupts();
    
    // Jump to application
    app_main();
}

void bootloader_main(void) {
    printf("bootloader: main\r\n");    
    flash_init();
    if(trigger_pattern == 2) {
        firmware_update_copy_external_flash_to_internal_flash();
        jump_to_application();
    }
    else if(trigger_pattern == 3) {
        firmware_update_copy_internal_flash_to_external_flash();
        jump_to_application();
    }
    else if(trigger_pattern == 4) {
        firmware_update_compare_internal_flash_to_external_flash();
        jump_to_application();
    }
    else {
        jump_to_application();
    }
    while(1){
        SYS_Tasks();
    }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    LED_STAT_Set();
    
    printf("bootloader: starting\r\n");
    printf("bootloader: %s %s\r\n", bootloader_name, bootloader_version);
    
    check_reset_source();
    
    if(trigger_pattern > 1) {
        printf("bootloader: trigger pattern: 0x%08X\r\n", trigger_pattern);
        bootloader_main();
    }
    else {
        // check_application_valid();
        jump_to_application();
    }
    
    return EXIT_FAILURE;
}


/*******************************************************************************
 End of File
*/

