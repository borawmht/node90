/*
* storage.h
*/
#ifndef _STORAGE_H
#define _STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
// #include <string.h>

#define STORAGE_STR_LENGTH_MAX 512

bool storage_getBool(const char * namespace, const char * key, bool * value);
bool storage_setBool(const char * namespace, const char * key, bool value);
bool storage_loadBool(const char * namespace, const char * key, bool * b, bool default_value, bool (*func)(bool));

bool storage_getU8(const char * namespace, const char * key, uint8_t * value);
bool storage_setU8(const char * namespace, const char * key, uint8_t value);
bool storage_loadU8(const char * namespace, const char * key, uint8_t * u8, uint8_t default_value, bool (*func)(uint8_t));

bool storage_getI8(const char * namespace, const char * key, int8_t * value);
bool storage_setI8(const char * namespace, const char * key, int8_t value);
bool storage_loadI8(const char * namespace, const char * key, int8_t * i8, int8_t default_value, bool (*func)(int8_t));

bool storage_getU16(const char * namespace, const char * key, uint16_t * value);
bool storage_setU16(const char * namespace, const char * key, uint16_t value);
bool storage_loadU16(const char * namespace, const char * key, uint16_t * u16, uint16_t default_value, bool (*func)(uint16_t));

bool storage_getI16(const char * namespace, const char * key, int16_t * value);
bool storage_setI16(const char * namespace, const char * key, int16_t value);
bool storage_loadI16(const char * namespace, const char * key, int16_t * i16, int16_t default_value, bool (*func)(int16_t));

bool storage_getU32(const char * namespace, const char * key, uint32_t * value);
bool storage_setU32(const char * namespace, const char * key, uint32_t value);
bool storage_loadU32(const char * namespace, const char * key, uint32_t * u32, uint32_t default_value, bool (*func)(uint32_t));

bool storage_getI32(const char * namespace, const char * key, int32_t * value);
bool storage_setI32(const char * namespace, const char * key, int32_t value);
bool storage_loadI32(const char * namespace, const char * key, int32_t * i32, int32_t default_value, bool (*func)(int32_t));

bool storage_getStr(const char * namespace, const char * key, char * value);
bool storage_setStr(const char * namespace, const char * key, char * value);
bool storage_loadStr(const char * namespace, const char * key, char * str, const char * default_value, bool (*func)(char *));
bool storage_loadStrIndex(const char * namespace, const char * key, char * value, const char * default_value, uint16_t index, bool (*func)(uint16_t,char *));

// Add blob support
bool storage_getBlob(const char * namespace, const char * key, void * value, size_t *length);
bool storage_setBlob(const char * namespace, const char * key, const void * value, size_t length);
bool storage_loadBlob(const char * namespace, const char * key, void * value, size_t max_length, const void * default_value, size_t default_length, bool (*func)(void *));

#endif