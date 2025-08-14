/*
* commands.c
* created by: Brad Oraw
* created on: 2025-08-14
*/

#include "commands.h"
#include "storage.h"
#include "definitions.h"
#include <string.h>

static const char *TAG = "commands";

char command_response[256];
static void command_trigger(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    if(argc > 1) {
        trigger_pattern = strtoul(argv[1], NULL, 16);
        sprintf(command_response,"trigger pattern: 0x%08lX\r\nrestarting...\r\n", trigger_pattern);        
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, command_response );
        vTaskDelay(1000);
        SYS_RESET_SoftwareReset();
    } else {
        sprintf(command_response,"trigger: missing argument\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, command_response );
    }
}

const SYS_CMD_DESCRIPTOR commands[] =
{   
    {"trigger",     command_trigger,   ": set trigger pattern"},
    {NULL, NULL, NULL}
};

void commands_init(void) {
    SYS_CONSOLE_PRINT("commands: init\r\n");
    uint8_t num_commands = sizeof(commands) / sizeof(commands[0]);
    SYS_CMD_ADDGRP(commands, num_commands, "commands", "commands");
}