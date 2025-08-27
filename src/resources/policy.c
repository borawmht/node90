/*
* policy.c
* created by: Brad Oraw
* created on: 2025-08-27
*/

#include "policy.h"
#include "resources.h"
#include "ethernet.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"

policy_t policies[NUM_POLICIES];

void policy_init(void){
    SYS_CONSOLE_PRINT("policy: init\r\n");
    for(int i = 0; i < NUM_POLICIES; i++){
        policies[i].channel = i+1;
        sprintf(policies[i].ns, "policy%d", i+1);
        storage_loadStrIndex(policies[i].ns, "on", policies[i].on, "F0,0,100;", i+1, &policy_set_on);
        storage_loadStrIndex(policies[i].ns, "off", policies[i].off, "F0,0,0;", i+1, &policy_set_off);
        storage_loadStrIndex(policies[i].ns, "up", policies[i].up, "F2,0,25;", i+1, &policy_set_up);
        storage_loadStrIndex(policies[i].ns, "down", policies[i].down, "F3,0,25;", i+1, &policy_set_down);
        storage_loadStrIndex(policies[i].ns, "mot", policies[i].mot, "F0,0,100;", i+1, &policy_set_mot);
        storage_loadStrIndex(policies[i].ns, "vac", policies[i].vac, "F0,0,0;", i+1, &policy_set_vac);
        storage_loadStrIndex(policies[i].ns, "s1", policies[i].s1, "none", i+1, &policy_set_s1);
        storage_loadStrIndex(policies[i].ns, "s2", policies[i].s2, "none", i+1, &policy_set_s2);
        storage_loadStrIndex(policies[i].ns, "s3", policies[i].s3, "none", i+1, &policy_set_s3);
    }
}

char * policy_get_json_str(uint8_t channel) {
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy_get_json_str: channel out of range: %d\r\n", channel);
        return NULL;
    }
    cJSON * root = cJSON_CreateObject();
    uint8_t i = channel - 1;
    cJSON_AddNumberToObject(root,"channel",policies[i].channel);
    cJSON_AddStringToObject(root,"on",policies[i].on);
    cJSON_AddStringToObject(root,"off",policies[i].off);
    cJSON_AddStringToObject(root,"up",policies[i].up);
    cJSON_AddStringToObject(root,"down",policies[i].down);
    cJSON_AddStringToObject(root,"mot",policies[i].mot);
    cJSON_AddStringToObject(root,"vac",policies[i].vac);
    cJSON_AddStringToObject(root,"s1",policies[i].s1);
    cJSON_AddStringToObject(root,"s2",policies[i].s2);
    cJSON_AddStringToObject(root,"s3",policies[i].s3);    
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool policy_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("policy: policy%u coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",policy_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("policy: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("policy: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("policy: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool policy_set_on(uint8_t channel, char *on){
    // TODO: validate on
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_on: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].on,on,POLICY_SIZE) != 0;
    strncpy(policies[i].on,on,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u on: %s\r\n", channel, policies[i].on);
    if(changed){
        return storage_setStr(policies[i].ns, "on", policies[i].on);
    }
    return true;
}

bool policy_set_off(uint8_t channel, char *off){
    // TODO: validate off
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_off: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].off,off,POLICY_SIZE) != 0;
    strncpy(policies[i].off,off,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u off: %s\r\n", channel, policies[i].off);
    if(changed){
        return storage_setStr(policies[i].ns, "off", policies[i].off);
    }
    return true;
}

bool policy_set_up(uint8_t channel, char *up){  
    // TODO: validate up
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_up: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].up,up,POLICY_SIZE) != 0;
    strncpy(policies[i].up,up,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u up: %s\r\n", channel, policies[i].up);
    if(changed){
        return storage_setStr(policies[i].ns, "up", policies[i].up);
    }
    return true;
}

bool policy_set_down(uint8_t channel, char *down){
    // TODO: validate down
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_down: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].down,down,POLICY_SIZE) != 0;
    strncpy(policies[i].down,down,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u down: %s\r\n", channel, policies[i].down);
    if(changed){
        return storage_setStr(policies[i].ns, "down", policies[i].down);
    }
    return true;
}

bool policy_set_mot(uint8_t channel, char *mot){
    // TODO: validate mot
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_mot: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].mot,mot,POLICY_SIZE) != 0;
    strncpy(policies[i].mot,mot,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u mot: %s\r\n", channel, policies[i].mot);
    if(changed){
        return storage_setStr(policies[i].ns, "mot", policies[i].mot);
    }
    return true;
}

bool policy_set_vac(uint8_t channel, char *vac){
    // TODO: validate vac
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_vac: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].vac,vac,POLICY_SIZE) != 0;
    strncpy(policies[i].vac,vac,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u vac: %s\r\n", channel, policies[i].vac);
    if(changed){
        return storage_setStr(policies[i].ns, "vac", policies[i].vac);
    }
    return true;
}

bool policy_set_s1(uint8_t channel, char *s1){
    // TODO: validate s1
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_s1: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].s1,s1,POLICY_SIZE) != 0;
    strncpy(policies[i].s1,s1,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u s1: %s\r\n", channel, policies[i].s1);
    if(changed){
        return storage_setStr(policies[i].ns, "s1", policies[i].s1);
    }
    return true;
}

bool policy_set_s2(uint8_t channel, char *s2){
    // TODO: validate s2
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_s2: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].s2,s2,POLICY_SIZE) != 0;
    strncpy(policies[i].s2,s2,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u s2: %s\r\n", channel, policies[i].s2);
    if(changed){
        return storage_setStr(policies[i].ns, "s2", policies[i].s2);
    }
    return true;
}

bool policy_set_s3(uint8_t channel, char *s3){
    // TODO: validate s3
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u set_s3: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(policies[i].s3,s3,POLICY_SIZE) != 0;
    strncpy(policies[i].s3,s3,POLICY_SIZE);
    SYS_CONSOLE_PRINT("policy: policy%u s3: %s\r\n", channel, policies[i].s3);
    if(changed){
        return storage_setStr(policies[i].ns, "s3", policies[i].s3);
    }
    return true;
}

bool policy_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("policy: policy%u put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * on = cJSON_GetObjectItem(map,"on");    
    if(on){
        ret &= policy_set_on(channel,on->valuestring);
        found = true;
    }
    cJSON * off = cJSON_GetObjectItem(map,"off");    
    if(off){
        ret &= policy_set_off(channel,off->valuestring);
        found = true;
    }
    cJSON * up = cJSON_GetObjectItem(map,"up");    
    if(up){
        ret &= policy_set_up(channel,up->valuestring);
        found =true;
    }
    cJSON * down = cJSON_GetObjectItem(map,"down");    
    if(down){
        ret &= policy_set_down(channel,down->valuestring);
        found = true;
    }
    cJSON * mot = cJSON_GetObjectItem(map,"mot");    
    if(mot){
        ret &= policy_set_mot(channel,mot->valuestring);
        found = true;
    }
    cJSON * vac = cJSON_GetObjectItem(map,"vac");    
    if(vac){
        ret &= policy_set_vac(channel,vac->valuestring);
        found = true;
    }
    cJSON * s1 = cJSON_GetObjectItem(map,"s1");    
    if(s1){
        ret &= policy_set_s1(channel,s1->valuestring);
        found = true;
    }
    cJSON * s2 = cJSON_GetObjectItem(map,"s2");    
    if(s2){
        ret &= policy_set_s2(channel,s2->valuestring);
        found = true;
    }
    cJSON * s3 = cJSON_GetObjectItem(map,"s3");    
    if(s3){
        ret &= policy_set_s3(channel,s3->valuestring);
        found = true;
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool policy_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("policy: policy%u coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return policy_put_json_str(channel, resource_json_str);
}

bool policy_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("policy: policy coap handler: %s\r\n", uri);
        // sscanf(uri, "/inx/policy/policy%hhu", &channel);
        channel = 1;
        SYS_CONSOLE_PRINT("policy: policy%u coap handler\r\n", channel);
    }
    else{
        SYS_CONSOLE_PRINT("policy: policy coap handler: failed to parse uri\r\n");
        return false;
    }
    // SYS_CONSOLE_PRINT("policy: policy%u coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return policy_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return policy_coap_put_handler(channel, request, response);
    }
    return false;
}

char * policy_get_on(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_on: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].on;
}

char * policy_get_off(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_off: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].off;
}

char * policy_get_up(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_up: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].up;
}

char * policy_get_down(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_down: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].down;
}

char * policy_get_mot(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_mot: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].mot;
}

char * policy_get_vac(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_vac: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].vac;
}

char * policy_get_s1(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_s1: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].s1;
}

char * policy_get_s2(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_s2: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].s2;
}

char * policy_get_s3(uint8_t channel){
    if(channel<1 || channel>NUM_POLICIES){
        SYS_CONSOLE_PRINT("policy: policy%u get_s3: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return policies[channel-1].s3;
}