/*
* coap_server.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "coap_server.h"
#include "ethernet.h"
#include "resources.h"
#include "jsoncbor.h"
#include "cborjson.h"
#include "definitions.h"
#include <string.h>

// Example resource handlers
bool coap_hello_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("coap_server: hello_handler\r\n");
    
    const char *hello_msg = "Hello from CoAP server!";
    response->code = COAP_CODE_CONTENT;
    response->payload_length = strlen(hello_msg);
    memcpy(response->payload, hello_msg, response->payload_length);
    response->payload[response->payload_length] = '\0';
    
    // SYS_CONSOLE_PRINT("coap: hello_handler completed, payload_length: %d\r\n", response->payload_length);
    SYS_CONSOLE_PRINT("coap_server: response: %s\r\n", response->payload);
    return true;
}

bool coap_status_handler(const coap_message_t *request, coap_message_t *response) {
    // SYS_CONSOLE_PRINT("coap_server: status_handler\r\n");
    
    char status_msg[128];
    snprintf(status_msg, sizeof(status_msg), 
             "{\"e\":{\"status\":\"ok\",\"ip\":\"%d.%d.%d.%d\",\"link\":%s}}",
             ethernet_getIPAddress()[0], ethernet_getIPAddress()[1],
             ethernet_getIPAddress()[2], ethernet_getIPAddress()[3],
             ethernet_linkUp() ? "true" : "false");
    
    // Use a fixed buffer instead of dynamic allocation
    uint8_t cbor_buffer[256];  // Fixed size buffer
    size_t encoded_size = 0;
    
    // Use the fixed buffer version
    CborError error = json_to_cbor(status_msg, cbor_buffer, sizeof(cbor_buffer), &encoded_size);
    if (error != CborNoError) {
        SYS_CONSOLE_PRINT("coap_server: json_to_cbor error: %d\r\n", error);
        // Fallback to plain text response
        response->code = COAP_CODE_CONTENT;
        response->content_format = COAP_CONTENT_FORMAT_APPLICATION_JSON;
        response->payload_length = strlen(status_msg);
        memcpy(response->payload, status_msg, response->payload_length);
        SYS_CONSOLE_PRINT("coap_server: fallback to text response\r\n");
        return true;
    }
    
    response->code = COAP_CODE_CONTENT;
    
    // Set content format as an option (not direct field)
    // coap_set_content_format_option(response, COAP_CONTENT_FORMAT_APPLICATION_CBOR);
    response->content_format = COAP_CONTENT_FORMAT_APPLICATION_CBOR;
    
    response->payload_length = encoded_size;
    memcpy(response->payload, cbor_buffer, response->payload_length);
    
    char json_str[256];
    cbor_to_json_string(cbor_buffer, encoded_size, json_str, sizeof(json_str), &encoded_size, 0);
    SYS_CONSOLE_PRINT("coap_server: response: %s\r\n", json_str);

    // SYS_CONSOLE_PRINT("coap_server: CBOR response created, size: %d\r\n", response->payload_length);
    return true;
} 

void coap_server_init(void) {
    // Start CoAP server
    if (coap_server_start(COAP_DEFAULT_PORT)) {
        // Register some example resources
        coap_register_resource("/", coap_hello_handler);
        coap_register_resource("/status", coap_status_handler);
        for (int i = 0; resources[i].path != NULL; i++) {
            if (resources[i].coap_server_handler) {
                coap_register_resource(resources[i].path, resources[i].coap_server_handler);
            }
        }
    }
}