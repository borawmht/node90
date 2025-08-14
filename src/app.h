/*
* app.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef _APP_H
#define _APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "configuration.h"
#include "definitions.h"
#include "project_version.h"

extern uint32_t trigger_pattern;
extern const uint32_t app_signature;
extern const char app_name[];
extern const char app_version[];

typedef enum{
    APP_STATE_INIT=0,
    APP_STATE_RUN,    
} APP_STATES;

typedef struct{
    APP_STATES state;
} APP_DATA;

void APP_Initialize(void); // initialize before task scheduler starts
void APP_Tasks(void); // called by task scheduler

#endif
