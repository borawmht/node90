/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "eeprom.h"
#include "resources.h"
#include "ethernet.h"
#include "coap.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
*/

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
*/


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    SYS_CONSOLE_PRINT("Initialize Application\r\n");    
    ethernet_init();    
    coap_init();   
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

// bool send_test_packet = true;
uint8_t send_test_packet = 0;
uint8_t app_counter = 255;
void APP_Tasks ( void )
{
    LED_STAT_Toggle(); 
    if(app_counter<255){
        app_counter++;
        if(app_counter%10 == 0){
            SYS_CONSOLE_PRINT("app: tick %u\r\n",app_counter/10);
            // SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
        }
    }    
    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {            
            eeprom_init();
            resources_init();
            // SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize());
            appData.state = APP_STATE_SERVICE_TASKS;
            break;
        }

        case APP_STATE_SERVICE_TASKS:
        {            
            if(send_test_packet && ethernet_linkUp() && ethernet_hasIP()){
                //vTaskDelay(3000/portTICK_PERIOD_MS);     
                SYS_CONSOLE_MESSAGE("app: sending test packet\r\n");
                // send test packet to specific MAC and IP
                uint8_t dstMac[6] = {0x88, 0xAE, 0xDD, 0x0E, 0x75, 0x61}; // 88-AE-DD-0E-75-61
                uint8_t test_payload[20] = "Hello from PIC32!";
                
                // Send as raw ethernet frame (you can change the ethernet type as needed)
                if(ethernet_send_to(dstMac, test_payload, strlen((char*)test_payload), 0x1234)){ // Custom ethernet type                    
                    SYS_CONSOLE_MESSAGE("app: test packet sent\r\n");
                    SYS_CONSOLE_PRINT("Free heap: %d bytes\r\n", xPortGetFreeHeapSize()); // check for memory leak
                } else {
                    SYS_CONSOLE_MESSAGE("app: test packet send failed\r\n");
                }
                send_test_packet--;
            }            
            break;
        }

        /* TODO: implement your application state machine.*/


        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
