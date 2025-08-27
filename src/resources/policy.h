/*
* policy.h
* created by: Brad Oraw
* created on: 2025-08-27
*/

#ifndef POLICY_H
#define POLICY_H

#include <stdint.h>
#include <stdbool.h>
#include "resources.h"

#define NUM_POLICIES 1
#define POLICY_SIZE 128

typedef struct {
    char ns[16];
    uint8_t channel;
    char on[POLICY_SIZE];
    char off[POLICY_SIZE];
    char up[POLICY_SIZE];
    char down[POLICY_SIZE];
    char mot[POLICY_SIZE];
    char vac[POLICY_SIZE];
    char s1[POLICY_SIZE];
    char s2[POLICY_SIZE];
    char s3[POLICY_SIZE];
} policy_t;

void policy_init(void);

bool policy_put_json_str(uint8_t channel, char * json_str);
char * policy_get_json_str(uint8_t channel);
bool policy_coap_handler(const coap_message_t *request, coap_message_t *response);

bool policy_set_on(uint8_t channel, char * on);
bool policy_set_off(uint8_t channel, char * off);
bool policy_set_up(uint8_t channel, char * up);
bool policy_set_down(uint8_t channel, char * down);
bool policy_set_mot(uint8_t channel, char * mot);
bool policy_set_vac(uint8_t channel, char * vac);
bool policy_set_s1(uint8_t channel, char * s1);
bool policy_set_s2(uint8_t channel, char * s2);
bool policy_set_s3(uint8_t channel, char * s3);
char * policy_get_on(uint8_t channel);
char * policy_get_off(uint8_t channel);
char * policy_get_up(uint8_t channel);
char * policy_get_down(uint8_t channel);
char * policy_get_mot(uint8_t channel);
char * policy_get_vac(uint8_t channel);
char * policy_get_s1(uint8_t channel);
char * policy_get_s2(uint8_t channel);
char * policy_get_s3(uint8_t channel);

#endif