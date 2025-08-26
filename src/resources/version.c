/*
* Version Resource
* version.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "version.h"
#include "resources.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"
#include "project_version.h"

const char * version_ns = "version";

void version_init(void) {
    SYS_CONSOLE_PRINT("version: init\r\n");
}

char * version_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"name",PROJECT_NAME);
    cJSON_AddStringToObject(root,"version",PROJECT_VERSION);
    cJSON_AddStringToObject(root,"date",PROJECT_BUILD_DATE);
    cJSON_AddStringToObject(root,"time",PROJECT_BUILD_TIME);
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool version_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("version: coap get handler\r\n");
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",version_get_json_str());
    size_t encoded_size = 0;
    CborError error = json_to_cbor(resource_e_json_str, resource_cbor_buffer, RESOURCE_CBOR_BUFFER_SIZE, &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("network: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, resource_cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("network: response: %s\r\n", resource_e_json_str);
    // break long print into parts, split on characters
    SYS_CONSOLE_PRINT("version: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool version_put_json_str(char * json_str){
    SYS_CONSOLE_PRINT("version: put json str: %s\r\n", json_str);
    return true;
}

bool version_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("version: coap put handler\r\n");
    return true;
}

bool version_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("version: coap handler\r\n");
    if(request->code == COAP_CODE_GET){
        return version_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return version_coap_put_handler(request, response);
    }
    return false;
}

char * version_get_name(void){
    return PROJECT_NAME;
}

char * version_get_version(void){
    return PROJECT_VERSION;
}

char * version_get_date(void){
    return PROJECT_BUILD_DATE;
}

char * version_get_time(void){
    return PROJECT_BUILD_TIME;
}