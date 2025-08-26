/*
* OTA Resource
* ota.c
* created by: Brad Oraw
* created on: 2025-08-26
*/

#include "ota.h"
#include "resources.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"
#include "firmware_update.h"
#include "app.h"

ota_t ota;

const char * ota_ns = "ota";

void ota_init(void) {
    SYS_CONSOLE_PRINT("ota: init\r\n");
    // Load configuration using storage API with namespaces
    storage_loadStr(ota_ns, "bin_url", ota.bin_url, "https://192.168.1.68/release/node90_latest.bin", &ota_set_bin_url);    
}

char * ota_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"bin_url",ota.bin_url);
    cJSON_AddStringToObject(root,"start","false");
    cJSON_AddStringToObject(root,"version",firmware_update_get_external_version());
    cJSON_AddStringToObject(root,"valid",firmware_update_get_external_valid() ? "true" : "false");
    cJSON_AddStringToObject(root,"downloading",app_firmware_downloading() ? "true" : "false");
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(resource_json_str,print_str,RESOURCE_JSON_STR_SIZE);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return resource_json_str;
}

bool ota_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("ota: coap get handler\r\n");
    snprintf(resource_e_json_str,RESOURCE_E_JSON_STR_SIZE,"{\"e\":%s}",ota_get_json_str());
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
    SYS_CONSOLE_PRINT("ota: response: ");
    int len = strlen(resource_e_json_str);
    for(int i = 0; i < len; i++){
        SYS_CONSOLE_PRINT("%c", resource_e_json_str[i]);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool ota_set_bin_url(char * bin_url){
    // TODO: validate url
    bool changed = strncmp(ota.bin_url,bin_url,OTA_BIN_URL_SIZE) != 0;
    strncpy(ota.bin_url,bin_url,OTA_BIN_URL_SIZE);
    SYS_CONSOLE_PRINT("ota: bin_url: %s\r\n", ota.bin_url);
    if(changed){
        return storage_setStr(ota_ns, "bin_url", ota.bin_url);
    }
    return true;
}

bool ota_set_start(char * start){
    bool ret = false;
    if(strcmp(start,"true") == 0){
        ret = app_start_firmware_download(ota.bin_url,true);
    }
    else if(strcmp(start,"download") == 0){
        ret = app_start_firmware_download(ota.bin_url,false);
    }
    else if(strcmp(start,"update") == 0){
        trigger_pattern = TRIGGER_UPDATE;
        SYS_RESET_SoftwareReset();
    }
    return ret;
}

bool ota_put_json_str(char * json_str){
    SYS_CONSOLE_PRINT("ota: put json str: %s\r\n", json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * bin_url = cJSON_GetObjectItem(map,"bin_url");    
    if(bin_url){
        ret &= ota_set_bin_url(bin_url->valuestring);
        found = true;
    }
    cJSON * start = cJSON_GetObjectItem(map,"start");    
    if(start){
        ret &= ota_set_start(start->valuestring);
        found = true;
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool ota_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("ota: coap put handler\r\n");
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, resource_json_str, RESOURCE_JSON_STR_SIZE, &encoded_size, 0);        
    return ota_put_json_str(resource_json_str);
}

bool ota_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("ota: coap handler\r\n");
    if(request->code == COAP_CODE_GET){
        return ota_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return ota_coap_put_handler(request, response);
    }
    return false;
}

char * ota_get_bin_url(void){
    return ota.bin_url;
}