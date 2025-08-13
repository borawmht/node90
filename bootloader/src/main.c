/*
* bootloader
* main.c  
*/

#include <stddef.h>                     // Defines NULL
#include <stdint.h>
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "project_version.h"

__attribute__((section(".bootloader_name")))
const char bootloader_name[] = PROJECT_NAME;

__attribute__((section(".bootloader_version")))
const char bootloader_version[] = PROJECT_VERSION;

// Application entry point
#define APP_START_ADDRESS    0x9D010000
#define APP_VALID_SIGNATURE  0x12345678

typedef void (*app_function_t)(void);

bool check_application_valid(void) {
    // Check if application has valid signature
    uint32_t *app_signature = (uint32_t*)(APP_START_ADDRESS + 0x1000);
    printf("Application signature: %08X\r\n", *app_signature);
    return (*app_signature == APP_VALID_SIGNATURE);
}

void jump_to_application(void) {
    printf("Load application\r\n");
    app_function_t app_main = (app_function_t)APP_START_ADDRESS;
    
    // Disable interrupts
    __builtin_disable_interrupts();
    
    // Jump to application
    app_main();
}

void bootloader_main(void) {
    while(1){
        SYS_Tasks();
    }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    printf("Starting bootloader\r\n");

    // Check for bootloader trigger (e.g., button press)
//    if (GPIO_PinRead(GPIO_PIN_RB0) == 0) {
//        // Enter bootloader mode
//        bootloader_main();
//    } else if (check_application_valid()) {
//        // Jump to application
//        jump_to_application();
//    } else {
//        // Invalid application, stay in bootloader
//        bootloader_main();
//    }
    
    check_application_valid();
    jump_to_application();
    
    return EXIT_FAILURE;
}


/*******************************************************************************
 End of File
*/

