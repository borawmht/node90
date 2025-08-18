/*
* commands.c
* created by: Brad Oraw
* created on: 2025-08-14
*/

#include "commands.h"
#include "storage.h"
#include "definitions.h"
#include "firmware_update.h"
#include <string.h>

static const char *TAG = "commands";

char command_response[512];
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

static void command_download_firmware(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    if(argc > 1) {        
        sprintf(command_response,"download_firmware: downloaded firmware\r\n");
        (*pCmdIO->pCmdApi->msg)(cmdIoParam, command_response );
        app_start_firmware_download(argv[1]);
    } else {
        // char url[] = "http://192.168.1.68:8080/release/node90_1.0.1.bin";
        // sprintf(command_response,"download_firmware: no argument, assume %s\r\n", url);
        // (*pCmdIO->pCmdApi->msg)(cmdIoParam, command_response );
        app_start_firmware_download(NULL);
    }
}

static void command_version(SYS_CMD_DEVICE_NODE* pCmdIO, int argc, char** argv)
{
    const void* cmdIoParam = pCmdIO->cmdIoParam;
    sprintf(command_response,"version: internal: %s %s %s\r\nversion: external: %s %s %s\r\n", 
        firmware_update_get_internal_name(), 
        firmware_update_get_internal_version(),
        firmware_update_get_internal_latest() ? "latest ✓" : "not latest ✗",
        firmware_update_get_external_name(), 
        firmware_update_get_external_version(), 
        firmware_update_get_external_valid() ? "valid ✓" : "invalid ✗");
    (*pCmdIO->pCmdApi->msg)(cmdIoParam, command_response );
}

const SYS_CMD_DESCRIPTOR commands[] =
{   
    {"trigger",                 command_trigger,                ": set trigger pattern"},
    {"download_firmware",       command_download_firmware,      ": download firmware"},
    {"version",                 command_version,                ": show version"},
    // {NULL, NULL, NULL}
};

void commands_init(void) {
    SYS_CONSOLE_PRINT("commands: init\r\n");
    uint8_t num_commands = sizeof(commands) / sizeof(commands[0]);
    SYS_CMD_ADDGRP(commands, num_commands, "commands", "commands");
}