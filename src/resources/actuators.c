/*
* Actuators Resource
* actuators.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "actuators.h"
#include "control.h"
#include "resources.h"
#include "sense.h"
#include "ethernet.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"

actuator_t actuators[NUM_ACTUATORS];

const char * actuator_modes[] = {
    "PWM",
    "ELS",
    "BATTERY_BACKUP_CC",
    "BATTERY_BACKUP_CV",
    "DESK_CC",
    "DESK_CV",
    "MECHO",
    "SOMFY",
    NULL
};

void actuators_init(void){
    SYS_CONSOLE_PRINT("actuators: init\r\n");
    for(int i = 0; i < NUM_ACTUATORS; i++){
        actuators[i].channel = i+1;
        sprintf(actuators[i].ns, "actuator%d", i+1);
        storage_loadStrIndex(actuators[i].ns, "cluster", actuators[i].cluster, "group1", i+1, &actuators_actuator_set_cluster); 
        storage_loadStrIndex(actuators[i].ns, "prphtag", actuators[i].prphtag, "0", i+1, &actuators_actuator_set_prphtag); 
        storage_loadStrIndex(actuators[i].ns, "mode", actuators[i].mode, "PWM", i+1, &actuators_actuator_set_mode); 
        storage_loadU16Index(actuators[i].ns, "fadetime", &actuators[i].fadetime, 2000, i+1, &actuators_actuator_set_fadetime); 
        storage_loadStrIndex(actuators[i].ns, "pwm_mode", actuators[i].pwm_mode, "DIM_CC", i+1, &actuators_actuator_set_pwm_mode); 
        storage_loadBoolIndex(actuators[i].ns, "motion_enable", &actuators[i].motion_enable, true, i+1, &actuators_actuator_set_motion_enable); 
        storage_loadU8Index(actuators[i].ns, "dim_els", &actuators[i].dim_els, 25, i+1, &actuators_actuator_set_dim_els); 
        storage_loadBoolIndex(actuators[i].ns, "cuv_enable", &actuators[i].cuv_enable, false, i+1, &actuators_actuator_set_cuv_enable); 
        storage_loadU16Index(actuators[i].ns, "cc", &actuators[i].cc, 2500, i+1, &actuators_actuator_set_cc); 
        storage_loadU16Index(actuators[i].ns, "cv", &actuators[i].cv, 12000, i+1, &actuators_actuator_set_cv); 
        storage_loadU16Index(actuators[i].ns, "cp", &actuators[i].cp, 100, i+1, &actuators_actuator_set_cp); 
        control_set_dim_value(i+1,0);
        control_set_at_value(4000);
    }
}

char * actuators_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON * actuators_array = cJSON_CreateArray();    
    for(int i = 0; i < NUM_ACTUATORS; i++){
        cJSON * actuator = cJSON_CreateObject();
        cJSON_AddNumberToObject(actuator,"channel",actuators[i].channel);
        char uri[64];
        sprintf(uri,"inx/actuators/actuator%u",i+1);
        cJSON_AddStringToObject(actuator,"uri",uri);
        cJSON_AddItemToArray(actuators_array, actuator);
    }    
    cJSON_AddItemToObject(root, "actuators", actuators_array);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool actuators_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: coap get handler\r\n");
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",actuators_get_json_str());
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("actuators: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("actuators: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("actuators: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool actuators_put_json_str(char * json_str){
    SYS_CONSOLE_PRINT("actuators: put json str: %s\r\n", json_str);
    return false;
}

bool actuators_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: coap put handler\r\n");
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return actuators_put_json_str(resource_json_str);
}

bool actuators_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("actuators: coap handler\r\n");
    if(request->code == COAP_CODE_GET){
        return actuators_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return actuators_coap_put_handler(request, response);
    }
    return false;
}

char * actuators_actuator_get_json_str(uint8_t channel) {
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator_get_json_str: channel out of range: %d\r\n", channel);
        return NULL;
    }
    cJSON * root = cJSON_CreateObject();
    uint8_t i = channel - 1;
    cJSON_AddNumberToObject(root,"channel",actuators[i].channel);
    cJSON_AddStringToObject(root,"cluster",actuators[i].cluster);
    cJSON_AddStringToObject(root,"prphtag",actuators[i].prphtag);
    cJSON_AddStringToObject(root,"mode",actuators[i].mode);
    cJSON_AddNumberToObject(root,"fadetime",actuators[i].fadetime);
    cJSON_AddStringToObject(root,"pwm_mode",actuators[i].pwm_mode);
    cJSON_AddStringToObject(root,"motion_enable",actuators[i].motion_enable ? "true" : "false");
    cJSON_AddStringToObject(root,"motdsbl",actuators[i].motion_enable ? "33" : "3");
    cJSON_AddNumberToObject(root,"dim",actuators_actuator_get_dim(channel));
    cJSON_AddNumberToObject(root,"pp",actuators_actuator_get_dim(channel));
    cJSON_AddNumberToObject(root,"dim_els",actuators[i].dim_els);
    cJSON_AddStringToObject(root,"cuv_enable",actuators[i].cuv_enable ? "true" : "false");
    cJSON_AddNumberToObject(root,"cc",actuators[i].cc);
    cJSON_AddNumberToObject(root,"cv",actuators[i].cv);
    cJSON_AddNumberToObject(root,"cp",actuators[i].cp);
    cJSON_AddNumberToObject(root,"at",actuators_actuator_get_at(channel));
    cJSON_AddNumberToObject(root,"current",sense_get_actuator_current(channel));
    cJSON_AddNumberToObject(root,"voltage",sense_get_actuator_voltage(channel));
    cJSON_AddNumberToObject(root,"power",sense_get_actuator_power(channel));
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool actuators_actuator_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: actuator%u coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",actuators_actuator_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("actuators: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("actuators: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("actuators: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

char * actuators_actuator_context_get_json_str(uint8_t channel) {
    cJSON * root = cJSON_CreateObject();
    cJSON * keyw_array = cJSON_CreateArray();    
    
    // add cluster to keyw_array
    cJSON_AddItemToArray(keyw_array, cJSON_CreateString(actuators[channel-1].cluster));
    
    cJSON_AddItemToObject(root, "keyw", keyw_array);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool actuators_actuator_context_coap_get_handler(uint8_t channel, coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: actuator%u context coap get handler\r\n", channel);
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",actuators_actuator_context_get_json_str(channel));
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("actuators: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("actuators: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("actuators: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool actuators_actuator_set_cluster(uint8_t channel, char *cluster){
    // TODO: validate cluster
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_cluster: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(actuators[i].cluster,cluster,16) != 0;
    strncpy(actuators[i].cluster,cluster,16);
    SYS_CONSOLE_PRINT("actuators: actuator%u cluster: %s\r\n", channel, actuators[i].cluster);
    if(changed){
        return storage_setStr(actuators[i].ns, "cluster", actuators[i].cluster);
    }
    return true;
}

bool actuators_actuator_set_prphtag(uint8_t channel, char *prphtag){
    // TODO: validate prphtag
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_prphtag: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(actuators[i].prphtag,prphtag,16) != 0;
    strncpy(actuators[i].prphtag,prphtag,16);
    SYS_CONSOLE_PRINT("actuators: actuator%u prphtag: %s\r\n", channel, actuators[i].prphtag);
    if(changed){
        return storage_setStr(actuators[i].ns, "prphtag", actuators[i].prphtag);
    }
    return true;
}

bool actuators_actuator_set_mode(uint8_t channel, char *mode){
    // TODO: validate mode
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_mode: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(actuators[i].mode,mode,ACTUATOR_MODE_SIZE) != 0;
    strncpy(actuators[i].mode,mode,ACTUATOR_MODE_SIZE);
    SYS_CONSOLE_PRINT("actuators: actuator%u mode: %s\r\n", channel, actuators[i].mode);
    if(changed){
        return storage_setStr(actuators[i].ns, "mode", actuators[i].mode);
    }
    return true;
}

bool actuators_actuator_set_fadetime(uint8_t channel, uint16_t fadetime){
    // TODO: validate fadetime
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_fadetime: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].fadetime != fadetime;
    actuators[i].fadetime = fadetime;
    SYS_CONSOLE_PRINT("actuators: actuator%u fadetime: %u\r\n", channel, actuators[i].fadetime);
    if(changed){
        return storage_setU16(actuators[i].ns, "fadetime", actuators[i].fadetime);
    }
    return true;
}

bool actuators_actuator_set_pwm_mode(uint8_t channel, char *pwm_mode){
    // TODO: validate pwm_mode
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_pwm_mode: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = strncmp(actuators[i].pwm_mode,pwm_mode,16) != 0;
    strncpy(actuators[i].pwm_mode,pwm_mode,16);
    SYS_CONSOLE_PRINT("actuators: actuator%u pwm_mode: %s\r\n", channel, actuators[i].pwm_mode);
    if(changed){
        return storage_setStr(actuators[i].ns, "pwm_mode", actuators[i].pwm_mode);
    }
    control_update_pwm_mode(channel);
    return true;
}

bool actuators_actuator_set_motion_enable(uint8_t channel, bool motion_enable){
    // TODO: validate motion_enable
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_motion_enable: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].motion_enable != motion_enable;
    actuators[i].motion_enable = motion_enable;
    SYS_CONSOLE_PRINT("actuators: actuator%u motion_enable: %s\r\n", channel, actuators[i].motion_enable ? "true" : "false");
    if(changed){
        return storage_setBool(actuators[i].ns, "motion_enable", actuators[i].motion_enable);
    }
    return true;
}

bool actuators_actuator_set_dim(uint8_t channel, uint8_t dim){
    // TODO: validate dim
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_dim: channel out of range: %d\r\n", channel);
        return false;
    }
    control_set_dim_value(channel,dim);
    return true;
}

bool actuators_actuator_set_dim_els(uint8_t channel, uint8_t dim_els){
    // TODO: validate dim_els
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_dim_els: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].dim_els != dim_els;
    actuators[i].dim_els = dim_els;
    SYS_CONSOLE_PRINT("actuators: actuator%u dim_els: %u\r\n", channel, actuators[i].dim_els);
    if(changed){
        return storage_setU8(actuators[i].ns, "dim_els", actuators[i].dim_els);
    }
    return true;
}

bool actuators_actuator_set_cuv_enable(uint8_t channel, bool cuv_enable){
    // TODO: validate cuv_enable
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_cuv_enable: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[0].cuv_enable != cuv_enable;
    changed |= actuators[1].cuv_enable != cuv_enable;
    actuators[0].cuv_enable = cuv_enable;
    actuators[1].cuv_enable = cuv_enable;
    SYS_CONSOLE_PRINT("actuators: cuv_enable: %s\r\n", actuators[i].cuv_enable ? "true" : "false");
    if(changed){        
        return storage_setBool(actuators[0].ns, "cuv_enable", actuators[0].cuv_enable) &&
               storage_setBool(actuators[1].ns, "cuv_enable", actuators[1].cuv_enable);
    }    
    return true;
}

bool actuators_actuator_set_cc(uint8_t channel, uint16_t cc){
    // TODO: validate cc
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_cc: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].cc != cc;
    actuators[i].cc = cc;
    SYS_CONSOLE_PRINT("actuators: actuator%u cc: %u\r\n", channel, actuators[i].cc);
    if(changed){
        return storage_setU16(actuators[i].ns, "cc", actuators[i].cc);
    }
    control_set_dim_value(channel,control_get_dim_value(channel)); // update the pwm duty cycle and dac level
    return true;
}

bool actuators_actuator_set_cv(uint8_t channel, uint16_t cv){
    // TODO: validate cv
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_cv: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].cv != cv;
    actuators[i].cv = cv;
    SYS_CONSOLE_PRINT("actuators: actuator%u cv: %u\r\n", channel, actuators[i].cv);
    if(changed){
        return storage_setU16(actuators[i].ns, "cv", actuators[i].cv);
    }
    control_set_voltage(channel,cv);
    return true;
}

bool actuators_actuator_set_cp(uint8_t channel, uint16_t cp){
    // TODO: validate cp
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_cp: channel out of range: %d\r\n", channel);
        return false;
    }
    uint8_t i = channel - 1;
    bool changed = actuators[i].cp != cp;
    actuators[i].cp = cp;
    SYS_CONSOLE_PRINT("actuators: actuator%u cp: %u\r\n", channel, actuators[i].cp);
    if(changed){
        return storage_setU16(actuators[i].ns, "cp", actuators[i].cp);
    }
    control_set_dim_value(channel,control_get_dim_value(channel)); // update the pwm duty cycle
    return true;
}

bool actuators_actuator_set_at(uint8_t channel, uint16_t at){
    // TODO: validate at
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u set_at: channel out of range: %d\r\n", channel);
        return false;
    }
    control_set_at_value(at);
    return true;
}

bool actuators_actuator_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("actuators: actuator%u put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * cluster = cJSON_GetObjectItem(map,"cluster");    
    if(cluster){
        ret &= actuators_actuator_set_cluster(channel,cluster->valuestring);
        found = true;
    }
    cJSON * prphtag = cJSON_GetObjectItem(map,"prphtag");    
    if(prphtag){
        ret &= actuators_actuator_set_prphtag(channel,prphtag->valuestring);
        found = true;
    }
    cJSON * mode = cJSON_GetObjectItem(map,"mode");    
    if(mode){
        ret &= actuators_actuator_set_mode(channel,mode->valuestring);
        found = true;
    }
    cJSON * fadetime = cJSON_GetObjectItem(map,"fadetime");    
    if(fadetime){
        ret &= actuators_actuator_set_fadetime(channel,fadetime->valueint);
        found =true;
    }
    cJSON * pwm_mode = cJSON_GetObjectItem(map,"pwm_mode");    
    if(pwm_mode){
        ret &= actuators_actuator_set_pwm_mode(channel,pwm_mode->valuestring);
        found = true;
    }
    cJSON * motion_enable = cJSON_GetObjectItem(map,"motion_enable");    
    if(motion_enable){
        ret &= actuators_actuator_set_motion_enable(channel,strcmp(motion_enable->valuestring,"true") == 0);
        found = true;
    }
    cJSON * motdsbl = cJSON_GetObjectItem(map,"motdsbl");    
    if(motdsbl){
        ret &= actuators_actuator_set_motion_enable(channel,strcmp(motdsbl->valuestring,"33") == 0);
        found = true;
    }
    cJSON * dim = cJSON_GetObjectItem(map,"dim");    
    if(dim){
        ret &= actuators_actuator_set_dim(channel,dim->valueint);
        found = true;
    }
    cJSON * pp = cJSON_GetObjectItem(map,"pp");    
    if(pp){
        ret &= actuators_actuator_set_dim(channel,pp->valueint);
        found = true;
    }
    cJSON * dim_els = cJSON_GetObjectItem(map,"dim_els");    
    if(dim_els){
        ret &= actuators_actuator_set_dim_els(channel,dim_els->valueint);
        found = true;
    }
    cJSON * cuv_enable = cJSON_GetObjectItem(map,"cuv_enable");    
    if(cuv_enable){
        ret &= actuators_actuator_set_cuv_enable(channel,strcmp(cuv_enable->valuestring,"true") == 0);
        found = true;
    }
    cJSON * cc = cJSON_GetObjectItem(map,"cc");    
    if(cc){
        ret &= actuators_actuator_set_cc(channel,cc->valueint);
        found = true;
    }
    cJSON * cv = cJSON_GetObjectItem(map,"cv");    
    if(cv){
        ret &= actuators_actuator_set_cv(channel,cv->valueint);
        found = true;
    }
    cJSON * cp = cJSON_GetObjectItem(map,"cp");    
    if(cp){
        ret &= actuators_actuator_set_cp(channel,cp->valueint);
        found = true;
    }
    cJSON * at = cJSON_GetObjectItem(map,"at");    
    if(at){
        ret &= actuators_actuator_set_at(channel,at->valueint);
        found = true;
    }   
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool actuators_actuator_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: actuator%u coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return actuators_actuator_put_json_str(channel, resource_json_str);
}

bool actuators_actuator_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("actuators: actuator coap handler: %s\r\n", uri);
        sscanf(uri, "/inx/actuators/actuator%hhu", &channel);
        SYS_CONSOLE_PRINT("actuators: actuator%u coap handler\r\n", channel);
    }
    else{
        SYS_CONSOLE_PRINT("actuators: actuator coap handler: failed to parse uri\r\n");
        return false;
    }
    // SYS_CONSOLE_PRINT("actuators: actuator%u coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return actuators_actuator_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return actuators_actuator_coap_put_handler(channel, request, response);
    }
    return false;
}

bool actuators_actuator_context_put_json_str(uint8_t channel, char * json_str){
    SYS_CONSOLE_PRINT("actuators: actuator%u context put json str: %s\r\n", channel, json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * keyw = cJSON_GetObjectItem(map,"keyw");    
    if(keyw && cJSON_IsArray(keyw) && cJSON_GetArraySize(keyw) > 0){
        cJSON * first_element = cJSON_GetArrayItem(keyw, 0);
        if(first_element && cJSON_IsString(first_element)){
            ret &= actuators_actuator_set_cluster(channel, first_element->valuestring);
            found = true;
        }
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool actuators_actuator_context_coap_put_handler(uint8_t channel, const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("actuators: actuator%u context coap put handler\r\n", channel);
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return actuators_actuator_context_put_json_str(channel, resource_json_str);
}

bool actuators_actuator_context_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // parse the uri
    char uri[64];
    uint8_t channel = 0;
    if(coap_parse_uri(request, uri, sizeof(uri))){
        SYS_CONSOLE_PRINT("actuators: actuator context coap handler: %s\r\n", uri);
        sscanf(uri, "/inx/actuators/actuator%hhu/context", &channel);
    }
    else{
        SYS_CONSOLE_PRINT("actuators: actuator context coap handler: failed to parse uri\r\n");
        return false;
    }
    SYS_CONSOLE_PRINT("actuators: actuator%u context coap handler\r\n", channel);
    if(request->code == COAP_CODE_GET){
        return actuators_actuator_context_coap_get_handler(channel, response);
    }
    else if(request->code == COAP_CODE_PUT){
        return actuators_actuator_context_coap_put_handler(channel, request, response);
    }
    return false;
}

char * actuators_actuator_get_cluster(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_cluster: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return actuators[channel-1].cluster;
}

char * actuators_actuator_get_prphtag(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_prphtag: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return actuators[channel-1].prphtag;
}

char * actuators_actuator_get_mode(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_mode: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return actuators[channel-1].mode;
}

uint16_t actuators_actuator_get_fadetime(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_fadetime: channel out of range: %d\r\n", channel);
        return 0;
    }
    return actuators[channel-1].fadetime;
}

char * actuators_actuator_get_pwm_mode(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_pwm_mode: channel out of range: %d\r\n", channel);
        return NULL;
    }
    return actuators[channel-1].pwm_mode;
}

bool actuators_actuator_get_motion_enable(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_motion_enable: channel out of range: %d\r\n", channel);
        return false;
    }
    return actuators[channel-1].motion_enable;
}

uint8_t actuators_actuator_get_dim(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_dim: channel out of range: %d\r\n", channel);
        return 0;
    }
    return control_get_dim_value(channel);
}

uint8_t actuators_actuator_get_dim_els(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_dim_els: channel out of range: %d\r\n", channel);
        return 0;
    }
    return actuators[channel-1].dim_els;
}

bool actuators_actuator_get_cuv_enable(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_cuv_enable: channel out of range: %d\r\n", channel);
        return false;
    }
    return actuators[channel-1].cuv_enable;
}

uint16_t actuators_actuator_get_cc(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_cc: channel out of range: %d\r\n", channel);
        return 0;
    }
    return actuators[channel-1].cc;
}

uint16_t actuators_actuator_get_cv(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_cv: channel out of range: %d\r\n", channel);
        return 0;
    }
    return actuators[channel-1].cv;
}

uint16_t actuators_actuator_get_cp(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_cp: channel out of range: %d\r\n", channel);
        return 0;
    }
    return actuators[channel-1].cp;
}

uint16_t actuators_actuator_get_at(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){ 
        SYS_CONSOLE_PRINT("actuators: actuator%u get_at: channel out of range: %d\r\n", channel);
        return 0;
    }
    return control_get_at_value();
}

uint16_t actuators_actuator_get_current(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_current: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sense_get_actuator_current(channel);
}

uint16_t actuators_actuator_get_voltage(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_voltage: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sense_get_actuator_voltage(channel);
}

uint16_t actuators_actuator_get_power(uint8_t channel){
    if(channel<1 || channel>NUM_ACTUATORS){
        SYS_CONSOLE_PRINT("actuators: actuator%u get_power: channel out of range: %d\r\n", channel);
        return 0;
    }
    return sense_get_actuator_power(channel);
}

bool actuators_actuator_get_is_cc(uint8_t channel){
    return !actuators_actuator_get_is_cv(channel);
}

bool actuators_actuator_get_is_cv(uint8_t channel){
    if(strcmp(actuators_actuator_get_pwm_mode(channel),"DIM_CV")==0){
        return true;
    }
    else if(strcmp(actuators_actuator_get_pwm_mode(channel),"AT_CV")==0){
        return true;
    }
    return false;
}

bool actuators_actuator_get_is_at(uint8_t channel){
    if(strcmp(actuators_actuator_get_pwm_mode(channel),"AT_CC")==0){
        return true;
    }
    else if(strcmp(actuators_actuator_get_pwm_mode(channel),"AT_CV")==0){
        return true;
    }
    return false;
}