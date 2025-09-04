/*
* pdline.h
*/
#ifndef _PDLINE_H
#define _PDLINE_H

#define PDLINE_ENABLE_DEFAULT true

#include <stdbool.h>

void pdline_init(void);
bool pdline_get_enable(void);
void pdline_set_enable(bool enable);
void pdline_data_ready_task(void);
void pdline_task(void);

#endif