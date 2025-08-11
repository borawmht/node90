/*
* coap_server.c
* created by: Brad Oraw
* created on: 2025-08-11
*/

#include "coap_server.h"
#include "ethernet.h"
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
             "{\"status\":\"ok\",\"ip\":\"%d.%d.%d.%d\",\"link\":%s}",
             ethernet_getIPAddress()[0], ethernet_getIPAddress()[1],
             ethernet_getIPAddress()[2], ethernet_getIPAddress()[3],
             ethernet_linkUp() ? "true" : "false");
    
    response->code = COAP_CODE_CONTENT;
    response->payload_length = strlen(status_msg);
    memcpy(response->payload, status_msg, response->payload_length);
    response->payload[response->payload_length] = '\0';
    
    SYS_CONSOLE_PRINT("coap_server: response: %s\r\n", response->payload);
    return true;
} 

void coap_server_init(void) {
    // Start CoAP server
    if (coap_server_start(COAP_DEFAULT_PORT)) {
        // Register some example resources
        coap_register_resource("/", coap_hello_handler);
        coap_register_resource("/status", coap_status_handler);
    }
}