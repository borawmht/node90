/*
* main.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdint.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "project_version.h"


// Application signature for bootloader validation
__attribute__((section(".app_signature")))
__attribute__((aligned(4)))
const uint32_t app_signature = 0x12345678;

__attribute__((section(".app_name")))
const char app_name[] = PROJECT_NAME;

__attribute__((section(".app_version")))
const char app_version[] = PROJECT_VERSION;

__attribute__((section(".trigger_pattern")))
uint32_t trigger_pattern = 0x12345678;

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

