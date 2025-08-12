/*
* Network Resource
* network.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "network.h"
#include "ethernet.h"
#include "cJSON.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include "storage.h"

network_t network;

const char * network_ns = "network";

void network_init(void) {
    SYS_CONSOLE_PRINT("network: init\r\n");
    // Load configuration using storage API with namespaces
    storage_loadStr(network_ns, "serial_number", network.serial_number, "12345", &network_set_serial_number);    
    storage_loadStr(network_ns, "tag", network.tag, "0", &network_set_tag);    
    storage_loadStr(network_ns, "inx_ip", network.inx_ip, "192.168.1.68", &network_set_inx_ip);
}

char network_json_str[1024];
char * network_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"tag",network.tag);
    cJSON_AddStringToObject(root,"serial_number",network.serial_number);
    cJSON_AddStringToObject(root,"serialnum",network.serial_number);
    cJSON_AddStringToObject(root,"mac",ethernet_getMACAddressString());
    cJSON_AddStringToObject(root,"ip",ethernet_getIPAddressString());
    cJSON_AddStringToObject(root,"mask",ethernet_getSubnetMaskString());
    cJSON_AddStringToObject(root,"gateway",ethernet_getGatewayString());
    cJSON_AddStringToObject(root,"broadcast_ip",ethernet_getBroadcastAddressString());
    cJSON_AddStringToObject(root,"inx_ip",network.inx_ip);
    cJSON_AddStringToObject(root,"inxip",network.inx_ip); 
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(network_json_str,print_str,1024);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return network_json_str;
}

bool network_coap_get_handler(coap_message_t *response){
    SYS_CONSOLE_PRINT("network: coap get handler\r\n");
    char * json_str = network_get_json_str();
    char e_json_str[1030];    
    snprintf(e_json_str,1024,"{\"e\":%s}",json_str);
    uint8_t cbor_buffer[1024];  // Fixed size buffer
    size_t encoded_size = 0;
    CborError error = json_to_cbor(e_json_str, cbor_buffer, sizeof(cbor_buffer), &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("network: json_to_cbor error: %d\r\n", error);
        return false;      
    }
    
    response->code = COAP_CODE_CONTENT;
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    response->payload_length = encoded_size;
    memcpy(response->payload, cbor_buffer, response->payload_length);

    // SYS_CONSOLE_PRINT("network: response: %s\r\n", e_json_str);
    // break long print into parts, split on 100 characters
    SYS_CONSOLE_PRINT("network: response: ");
    int len = strlen(e_json_str);
    for(int i = 0; i < len; i += 100){
        SYS_CONSOLE_PRINT("%s", e_json_str + i);
    }    
    SYS_CONSOLE_PRINT("\r\n");
    return true;
}

bool network_set_tag(char *tag){
    // TODO: validate tag and save to eeprom
    strncpy(network.tag,tag,16);
    SYS_CONSOLE_PRINT("network: tag: %s\r\n", network.tag);
    return storage_setStr(network_ns, "tag", network.tag);
}

bool network_set_inx_ip(char *inx_ip){
    // TODO: validate inx_ip and save to eeprom
    strncpy(network.inx_ip,inx_ip,20);
    SYS_CONSOLE_PRINT("network: inx_ip: %s\r\n", network.inx_ip);
    return storage_setStr(network_ns, "inx_ip", network.inx_ip);
}

bool network_set_serial_number(char *serial_number){
    // TODO: validate serial_number and save to eeprom
    strncpy(network.serial_number,serial_number,16);
    SYS_CONSOLE_PRINT("network: serial_number: %s\r\n", network.serial_number);
    return storage_setStr(network_ns, "serial_number", network.serial_number);
}

bool network_put_json_str(char * json_str){
    SYS_CONSOLE_PRINT("network: put json str: %s\r\n", json_str);
    // parse json
    bool ret = true;
    bool found = false;
    cJSON * root = cJSON_Parse(json_str);
    cJSON * map = root;
    cJSON * e = cJSON_GetObjectItem(root,"e");    
    if(e){
        map = e;
    }
    cJSON * tag = cJSON_GetObjectItem(map,"tag");    
    if(tag){
        ret &= network_set_tag(tag->valuestring);
        found = true;
    }
    cJSON * inx_ip = cJSON_GetObjectItem(map,"inx_ip");
    if(inx_ip){
        ret &= network_set_inx_ip(inx_ip->valuestring);
        found = true;
    }
    cJSON * serial_number = cJSON_GetObjectItem(map,"serial_number");
    if(serial_number){
        ret &= network_set_serial_number(serial_number->valuestring);
        found =true;
    }
    ret &= found;
    cJSON_Delete(root);
    return ret;
}

bool network_coap_put_handler(const coap_message_t *request, coap_message_t *response){
    SYS_CONSOLE_PRINT("network: coap put handler\r\n");
    // cbor to json
    size_t encoded_size = request->payload_length;
    cbor_to_json_string(request->payload, encoded_size, network_json_str, sizeof(network_json_str), &encoded_size, 0);        
    return network_put_json_str(network_json_str);
}

bool network_coap_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("network: coap handler\r\n");
    if(request->code == COAP_CODE_GET){
        return network_coap_get_handler(response);
    }
    else if(request->code == COAP_CODE_PUT){
        return network_coap_put_handler(request, response);
    }
    return false;
}

char * network_get_tag(void){
    return network.tag;
}

char * network_get_inx_ip(void){
    return network.inx_ip;
}

char * network_get_serial_number(void){
    return network.serial_number;
}