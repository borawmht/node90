/*
* firmware_update.h
* created by: Brad Oraw
* created on: 2025-08-15
*/

#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <stdbool.h>

#define APPLICATION_START_ADDRESS 0x9D004000
#define APPLICATION_END_ADDRESS   0x9D07FFFF
#define APPLICATION_NAME_ADDRESS  0x9D005020
#define APPLICATION_NAME_SIZE     0x20
#define APPLICATION_VERSION_ADDRESS 0x9D005040
#define APPLICATION_VERSION_SIZE    0x20

void firmware_update_init(void);
bool firmware_update_copy_internal_flash_to_external_flash(void);
bool firmware_update_copy_external_flash_to_internal_flash(void);
bool firmware_update_compare_internal_flash_to_external_flash(void);
bool firmware_update_download_binary_to_external_flash(const char * url);
char * firmware_update_get_internal_name(void);
char * firmware_update_get_internal_version(void);
bool firmware_update_get_internal_latest(void);
char * firmware_update_get_external_name(void);
char * firmware_update_get_external_version(void);
bool firmware_update_get_external_valid(void);

#endif // FIRMWARE_UPDATE_H