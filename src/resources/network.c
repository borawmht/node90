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
#include "definitions.h"

void network_init(void) {
    // TODO: Initialize network resources
}

char network_json_str[1024];
char * network_get_json_str(void) {    
    cJSON * root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"tag","0");
    cJSON_AddStringToObject(root,"serial_number","12345");
    cJSON_AddStringToObject(root,"serialnum","12345");
    cJSON_AddStringToObject(root,"mac",ethernet_getMACAddressString());
    cJSON_AddStringToObject(root,"ip",ethernet_getIPAddressString());
    cJSON_AddStringToObject(root,"mask",ethernet_getSubnetMaskString());
    cJSON_AddStringToObject(root,"gateway",ethernet_getGatewayString());
    cJSON_AddStringToObject(root,"broadcast_ip",ethernet_getBroadcastAddressString());
    cJSON_AddStringToObject(root,"inx_ip","192.168.1.68");
    cJSON_AddStringToObject(root,"inxip","192.168.1.68"); 
    char * print_str = cJSON_PrintUnformatted(root);
    strncpy(network_json_str,print_str,1024);
    cJSON_free(print_str);
    cJSON_Delete(root);
    return network_json_str;
}

bool network_coap_handler(const coap_message_t *request, coap_message_t *response) {
    SYS_CONSOLE_PRINT("network: coap handler\r\n");
    char * json_str = network_get_json_str();
    char e_json_str[1030];    
    snprintf(e_json_str,1024,"{\"e\":%s}",json_str);
    uint8_t cbor_buffer[1024];  // Fixed size buffer
    size_t encoded_size = 0;
    CborError error = json_to_cbor(e_json_str, cbor_buffer, sizeof(cbor_buffer), &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("network: json_to_cbor error: %d\r\n", error);
        // Fallback to json response
        response->code = COAP_CODE_CONTENT;
        response->content_format = COAP_CONTENT_FORMAT_APPLICATION_JSON;
        response->payload_length = strlen(e_json_str);
        memcpy(response->payload, e_json_str, response->payload_length);
        SYS_CONSOLE_PRINT("network: fallback to json response\r\n");        
    }
    else{
        response->code = COAP_CODE_CONTENT;
        response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
        response->payload_length = encoded_size;
        memcpy(response->payload, cbor_buffer, response->payload_length);
    }

    // SYS_CONSOLE_PRINT("network: response: %s\r\n", e_json_str);
    // break long print into parts, split on 100 characters
    SYS_CONSOLE_PRINT("network: response: ");
    int len = strlen(e_json_str);
    for(int i = 0; i < len; i += 100){
        SYS_CONSOLE_PRINT("%s", e_json_str + i);
    }    
    return true;
}