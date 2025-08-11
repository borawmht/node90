/*
* CoAP (Constrained Application Protocol) implementation
* coap.c
* created by: Brad Oraw
* created on: 2025-01-XX
*/

#include "coap.h"
#include "ethernet.h"
#include "definitions.h"
#include <string.h>
#include "config/default/library/tcpip/tcpip_helpers.h"

// Add queue structures for packet processing
typedef struct {
    uint8_t packet_data[COAP_MAX_PAYLOAD_SIZE];
    uint16_t packet_length;
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t src_mac[6];
} coap_packet_queue_item_t;

// Global flag for packet processing
static volatile bool coap_packet_ready = false;
static coap_packet_queue_item_t coap_current_packet;

// Global CoAP context
static coap_context_t coap_ctx = {0};
static coap_resource_t coap_resources[10] = {0};
static uint8_t coap_resource_count = 0;

// CoAP message parsing
bool coap_parse_message(const uint8_t *data, uint16_t length, coap_message_t *message) {
    if (length < 4 || data == NULL || message == NULL) {
        return false;
    }
    
    // Parse header
    message->version = (data[0] >> 6) & 0x03;
    message->type = (data[0] >> 4) & 0x03;
    message->token_length = data[0] & 0x0F;
    message->code = data[1];
    message->message_id = TCPIP_Helper_ntohs(*(uint16_t*)(&data[2]));
    
    uint16_t offset = 4;
    
    // Parse token
    if (message->token_length > 0) {
        if (offset + message->token_length > length) {
            return false;
        }
        memcpy(message->token, &data[offset], message->token_length);
        offset += message->token_length;
    }
    
    // Parse options
    message->options_count = 0;
    uint16_t option_number = 0;
    
    while (offset < length && message->options_count < COAP_MAX_OPTIONS) {
        if (data[offset] == 0xFF) {
            // Payload marker
            offset++;
            break;
        }
        
        uint8_t option_delta = (data[offset] >> 4) & 0x0F;
        uint8_t option_length = data[offset] & 0x0F;
        offset++;
        
        // Handle option delta
        if (option_delta == 13) {
            if (offset >= length) return false;
            option_number += data[offset] + 13;
            offset++;
        } else if (option_delta == 14) {
            if (offset + 1 >= length) return false;
            option_number += TCPIP_Helper_ntohs(*(uint16_t*)(&data[offset])) + 269;
            offset += 2;
        } else {
            option_number += option_delta;
        }
        
        // Handle option length
        if (option_length == 13) {
            if (offset >= length) return false;
            option_length = data[offset] + 13;
            offset++;
        } else if (option_length == 14) {
            if (offset + 1 >= length) return false;
            option_length = TCPIP_Helper_ntohs(*(uint16_t*)(&data[offset])) + 269;
            offset += 2;
        }
        
        // Store option
        if (offset + option_length > length) return false;
        
        message->options[message->options_count].number = option_number;
        message->options[message->options_count].length = option_length;
        message->options[message->options_count].value = (uint8_t*)&data[offset];
        message->options_count++;
        
        offset += option_length;
    }
    
    // Parse payload
    if (offset < length) {
        message->payload_length = length - offset;
        memcpy(message->payload, &data[offset], message->payload_length);
    } else {
        message->payload_length = 0;
    }
    
    return true;
}

// CoAP message building (simplified and fixed)
bool coap_build_message(const coap_message_t *message, uint8_t *buffer, uint16_t *length) {
    if (message == NULL || buffer == NULL || length == NULL) {
        return false;
    }
    
    // SYS_CONSOLE_PRINT("coap: building message, type: %d, code: %d, options_count: %d\r\n", 
    //                   message->type, message->code, message->options_count);
    
    uint16_t offset = 0;
    
    // Build header
    buffer[offset++] = (message->version << 6) | (message->type << 4) | message->token_length;
    buffer[offset++] = message->code;
    *(uint16_t*)(&buffer[offset]) = TCPIP_Helper_htons(message->message_id);
    offset += 2;
    
    // SYS_CONSOLE_PRINT("coap: header built, offset: %d\r\n", offset);
    
    // Build token
    if (message->token_length > 0) {
        memcpy(&buffer[offset], message->token, message->token_length);
        offset += message->token_length;
        // SYS_CONSOLE_PRINT("coap: token built, offset: %d\r\n", offset);
    }
    
    // Build options (simplified)
    uint16_t option_number = 0;
    for (int i = 0; i < message->options_count; i++) {
        uint16_t current_option_number = message->options[i].number;
        uint16_t option_length = message->options[i].length;
        
        // SYS_CONSOLE_PRINT("coap: building option %d, number: %d, length: %d\r\n", 
        //                   i, current_option_number, option_length);
        
        // Calculate delta (difference from previous option number)
        uint16_t option_delta = current_option_number - option_number;
        
        // SYS_CONSOLE_PRINT("coap: option_delta: %d, option_length: %d\r\n", option_delta, option_length);
        
        // Build option header with proper extended encoding support
        if (option_delta < 13 && option_length < 13) {
            // Standard encoding
            buffer[offset++] = (option_delta << 4) | option_length;
        } else if (option_delta < 269 && option_length < 13) {
            // Extended delta encoding
            buffer[offset++] = (13 << 4) | option_length;
            buffer[offset++] = option_delta - 13;
        } else if (option_delta < 13 && option_length < 269) {
            // Extended length encoding
            buffer[offset++] = (option_delta << 4) | 13;
            buffer[offset++] = option_length - 13;
        } else if (option_delta < 269 && option_length < 269) {
            // Both extended
            buffer[offset++] = (13 << 4) | 13;
            buffer[offset++] = option_delta - 13;
            buffer[offset++] = option_length - 13;
        } else {
            // SYS_CONSOLE_PRINT("coap: option too large, skipping\r\n");
            continue;  // Skip options that are too large for now
        }
        
        // Add option value
        if (option_length > 0 && message->options[i].value != NULL) {
            memcpy(&buffer[offset], message->options[i].value, option_length);
            offset += option_length;
        }
        
        option_number = current_option_number;
        // SYS_CONSOLE_PRINT("coap: option built, offset: %d\r\n", offset);
    }
    
    // Add payload marker if there's payload
    if (message->payload_length > 0) {
        buffer[offset++] = 0xFF;
        memcpy(&buffer[offset], message->payload, message->payload_length);
        offset += message->payload_length;
        // SYS_CONSOLE_PRINT("coap: payload added, offset: %d\r\n", offset);
    }
    
    *length = offset;
    // SYS_CONSOLE_PRINT("coap: message built successfully, total length: %d\r\n", *length);
    return true;
}

// Create UDP packet with IP and UDP headers
bool coap_create_udp_packet(const uint8_t *src_ip, const uint8_t *dst_ip,
                           uint16_t src_port, uint16_t dst_port,
                           const uint8_t *payload, uint16_t payload_length,
                           uint8_t *packet_buffer, uint16_t *packet_length) {
    if (packet_buffer == NULL || payload == NULL) {
        return false;
    }
    
    uint16_t offset = 0;
    
    // Ethernet header (14 bytes) - will be filled by ethernet_send_to
    offset += 14;
    
    // IP header (20 bytes)
    packet_buffer[offset] = 0x45;  // Version 4, header length 5 words
    packet_buffer[offset + 1] = 0x00;  // Type of Service
    *(uint16_t*)(&packet_buffer[offset + 2]) = TCPIP_Helper_htons(20 + 8 + payload_length);  // Total length
    *(uint16_t*)(&packet_buffer[offset + 4]) = 0;  // Identification
    *(uint16_t*)(&packet_buffer[offset + 6]) = 0x4000;  // Flags, Fragment offset
    packet_buffer[offset + 8] = 64;  // TTL
    packet_buffer[offset + 9] = 17;  // Protocol (UDP)
    *(uint16_t*)(&packet_buffer[offset + 10]) = 0;  // Checksum (will be calculated)
    
    // Source IP
    memcpy(&packet_buffer[offset + 12], src_ip, 4);
    // Destination IP
    memcpy(&packet_buffer[offset + 16], dst_ip, 4);
    
    offset += 20;
    
    // UDP header (8 bytes)
    *(uint16_t*)(&packet_buffer[offset]) = TCPIP_Helper_htons(src_port);
    *(uint16_t*)(&packet_buffer[offset + 2]) = TCPIP_Helper_htons(dst_port);
    *(uint16_t*)(&packet_buffer[offset + 4]) = TCPIP_Helper_htons(8 + payload_length);
    *(uint16_t*)(&packet_buffer[offset + 6]) = 0;  // Checksum (optional for IPv4)
    
    offset += 8;
    
    // Payload
    memcpy(&packet_buffer[offset], payload, payload_length);
    offset += payload_length;
    
    *packet_length = offset;
    return true;
}

// Handle incoming CoAP packet
bool coap_handle_packet(const uint8_t *packet_data, uint16_t packet_length, 
                       const uint8_t *src_ip, const uint8_t *dst_ip,
                       uint16_t src_port, uint16_t dst_port, uint8_t *src_mac) {
    
    // Check if this is a CoAP packet (UDP port 5683)
    if (dst_port != COAP_DEFAULT_PORT && src_port != COAP_DEFAULT_PORT) {
        return false;  // Not a CoAP packet
    }
    
    // SYS_CONSOLE_PRINT("coap: packet from %d.%d.%d.%d:%d\r\n", 
    //                   src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port);
    
    // Parse CoAP message
    coap_message_t request;
    if (!coap_parse_message(packet_data, packet_length, &request)) {
        SYS_CONSOLE_PRINT("coap: parse failed\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("coap: %s request, code: %d, options_count: %d\r\n", 
    //                   request.type == COAP_TYPE_CON ? "CON" : "NON", request.code, request.options_count);
    
    // Debug: Check server status
    // SYS_CONSOLE_PRINT("coap: is_server: %d, resource_count: %d\r\n", coap_ctx.is_server, coap_resource_count);
    
    // Handle the request
    if (coap_ctx.is_server) {
        // SYS_CONSOLE_PRINT("coap: creating response\r\n");
        
        coap_message_t response = {0};
        response.version = 1;
        response.type = (request.type == COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON;
        response.message_id = request.message_id;
        response.token_length = request.token_length;
        memcpy(response.token, request.token, request.token_length);
        response.options_count = 0;
        
        // Parse the URI from the request
        char request_uri[64];
        if (coap_parse_uri(&request, request_uri, sizeof(request_uri))) {
            // SYS_CONSOLE_PRINT("coap: parsed URI: '%s'\r\n", request_uri);
        } else {
            // SYS_CONSOLE_PRINT("coap: failed to parse URI, using default\r\n");
            strcpy(request_uri, "/");
        }
        
        // Find matching resource
        bool resource_found = false;
        for (int i = 0; i < coap_resource_count; i++) {
            // SYS_CONSOLE_PRINT("coap: checking resource '%s' against '%s'\r\n", coap_resources[i].path, request_uri);
            if (strcmp(coap_resources[i].path, request_uri) == 0) {
                // SYS_CONSOLE_PRINT("coap: calling resource handler\r\n");
                SYS_CONSOLE_PRINT("coap: resource handler: %s\r\n", coap_resources[i].path);
                resource_found = coap_resources[i].handler(&request, &response);
                // SYS_CONSOLE_PRINT("coap: resource handler returned: %d\r\n", resource_found);
                break;
            }
        }
        
        if (!resource_found) {
            SYS_CONSOLE_PRINT("coap: no resource found for '%s', setting NOT_FOUND\r\n", request_uri);
            response.code = COAP_CODE_NOT_FOUND;
        }
        
        // Add URI_PATH option to response first (lower option number)
        if (resource_found && response.options_count < COAP_MAX_OPTIONS) {
            // Remove leading slash for the option value
            const char *path_segment = request_uri;
            if (path_segment[0] == '/') {
                path_segment++;  // Skip leading slash
            }
            
            if (strlen(path_segment) > 0) {
                response.options[response.options_count].number = COAP_OPTION_URI_PATH;
                response.options[response.options_count].length = strlen(path_segment);
                response.options[response.options_count].value = (uint8_t*)path_segment;
                response.options_count++;
                // SYS_CONSOLE_PRINT("coap: added URI_PATH option: '%s'\r\n", path_segment);
            }
        }
        
        // Add Content-Format option for text responses (higher option number)
        if (resource_found && response.options_count < COAP_MAX_OPTIONS) {
            response.options[response.options_count].number = COAP_OPTION_CONTENT_FORMAT;
            response.options[response.options_count].length = 1;
            response.options[response.options_count].value = (uint8_t*)&(uint8_t){COAP_CONTENT_FORMAT_TEXT_PLAIN};
            response.options_count++;
            // SYS_CONSOLE_PRINT("coap: added Content-Format option\r\n");
        }
        
        // SYS_CONSOLE_PRINT("coap: calling send_packet_response\r\n");
        
        // Send response
        coap_packet_info_t packet_info = {
            .src_ip = {0}, .dst_ip = {0}, .src_port = dst_port, .dst_port = src_port,
            .packet_data = NULL, .packet_length = 0
        };
        memcpy(packet_info.src_ip, dst_ip, 4);
        memcpy(packet_info.dst_ip, src_ip, 4);
        
        bool send_result = coap_send_packet_response(src_mac, &packet_info, &response);
        // SYS_CONSOLE_PRINT("coap: send_packet_response returned: %d\r\n", send_result);
    } else {
        SYS_CONSOLE_PRINT("coap: not in server mode, ignoring request\r\n");
    }
    
    return true;
}

// Send CoAP response as raw packet
bool coap_send_packet_response(uint8_t *src_mac, const coap_packet_info_t *packet_info, 
                              const coap_message_t *response) {
    if (packet_info == NULL || response == NULL) {
        return false;
    }
    
    // Build CoAP message
    uint8_t coap_buffer[COAP_MAX_PAYLOAD_SIZE];
    uint16_t coap_length;
    if (!coap_build_message(response, coap_buffer, &coap_length)) {
        SYS_CONSOLE_PRINT("coap: build failed\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("coap: response built, len: %d\r\n", coap_length);
    
    // Create UDP packet (IP + UDP headers + CoAP payload)
    uint8_t udp_packet[1518];
    uint16_t udp_packet_length = 0;
    
    // IP header (20 bytes)
    udp_packet[udp_packet_length++] = 0x45;  // Version 4, header length 5 words
    udp_packet[udp_packet_length++] = 0x00;  // Type of Service
    *(uint16_t*)(&udp_packet[udp_packet_length]) = TCPIP_Helper_htons(20 + 8 + coap_length);  // Total length
    udp_packet_length += 2;
    *(uint16_t*)(&udp_packet[udp_packet_length]) = TCPIP_Helper_htons(0x1234);  // Identification (non-zero)
    udp_packet_length += 2;
    *(uint16_t*)(&udp_packet[udp_packet_length]) = 0x0000;  // Flags: Don't Fragment, Fragment Offset: 0
    udp_packet_length += 2;
    udp_packet[udp_packet_length++] = 64;  // TTL
    udp_packet[udp_packet_length++] = 17;  // Protocol (UDP)
    *(uint16_t*)(&udp_packet[udp_packet_length]) = 0;  // Checksum (will be calculated)
    udp_packet_length += 2;
    
    // Source IP (our IP)
    memcpy(&udp_packet[udp_packet_length], packet_info->src_ip, 4);
    udp_packet_length += 4;
    // Destination IP
    memcpy(&udp_packet[udp_packet_length], packet_info->dst_ip, 4);
    udp_packet_length += 4;
    
    // UDP header (8 bytes)
    *(uint16_t*)(&udp_packet[udp_packet_length]) = TCPIP_Helper_htons(packet_info->src_port);
    udp_packet_length += 2;
    *(uint16_t*)(&udp_packet[udp_packet_length]) = TCPIP_Helper_htons(packet_info->dst_port);
    udp_packet_length += 2;
    *(uint16_t*)(&udp_packet[udp_packet_length]) = TCPIP_Helper_htons(8 + coap_length);
    udp_packet_length += 2;
    *(uint16_t*)(&udp_packet[udp_packet_length]) = 0;  // Checksum (optional for IPv4)
    udp_packet_length += 2;
    
    // CoAP payload
    memcpy(&udp_packet[udp_packet_length], coap_buffer, coap_length);
    udp_packet_length += coap_length;
    
    // SYS_CONSOLE_PRINT("coap: UDP packet created, length: %d\r\n", udp_packet_length);
    
    // Send the packet
    if (!ethernet_send_to(src_mac, udp_packet, udp_packet_length, 0x0800)) {
        SYS_CONSOLE_PRINT("coap: failed to send packet\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("coap: response sent successfully\r\n");
    return true;
}

// CoAP server functions
bool coap_server_start(uint16_t port) {
    if (!ethernet_hasIP()) {
        SYS_CONSOLE_PRINT("coap: cannot start server - no IP address\r\n");
        return false;
    }
    
    coap_ctx.is_server = true;
    coap_ctx.port = port;
    coap_ctx.message_id_counter = 1;
    
    SYS_CONSOLE_PRINT("coap: server started on port %d\r\n", port);
    return true;
}

// CoAP client functions
bool coap_client_init(void) {
    if (!ethernet_hasIP()) {
        SYS_CONSOLE_PRINT("coap: cannot init client - no IP address\r\n");
        return false;
    }
    
    coap_ctx.is_server = false;
    coap_ctx.message_id_counter = 1;
    
    SYS_CONSOLE_PRINT("coap: client initialized\r\n");
    return true;
}

// Resource registration
bool coap_register_resource(const char *path, coap_resource_handler_t handler) {
    if (coap_resource_count >= 10) {
        SYS_CONSOLE_PRINT("coap: too many resources registered\r\n");
        return false;
    }
    
    coap_resources[coap_resource_count].path = path;
    coap_resources[coap_resource_count].handler = handler;
    coap_resource_count++;
    
    SYS_CONSOLE_PRINT("coap: registered resource %s\r\n", path);
    return true;
}

// Queue a packet for processing (called from packet handler)
bool coap_queue_packet(const uint8_t *packet_data, uint16_t packet_length, 
    const uint8_t *src_ip, const uint8_t *dst_ip,
    uint16_t src_port, uint16_t dst_port, uint8_t *src_mac) {
    if (packet_length > COAP_MAX_PAYLOAD_SIZE) {
        return false;
    }

    memcpy(coap_current_packet.packet_data, packet_data, packet_length);
    coap_current_packet.packet_length = packet_length;
    memcpy(coap_current_packet.src_ip, src_ip, 4);
    memcpy(coap_current_packet.dst_ip, dst_ip, 4);
    coap_current_packet.src_port = src_port;
    coap_current_packet.dst_port = dst_port;
    memcpy(coap_current_packet.src_mac, src_mac, 6);

    coap_packet_ready = true;
    return true;
}

// CoAP task function
void coap_task(void *pvParameters) {
    SYS_CONSOLE_PRINT("coap: task start\r\n");
    
    while (1) {
        if (coap_packet_ready) {
            // SYS_CONSOLE_PRINT("coap: processing packet\r\n");
            
            coap_packet_ready = false;
            
            // Process the packet
            coap_handle_packet(coap_current_packet.packet_data, coap_current_packet.packet_length,
                             coap_current_packet.src_ip, coap_current_packet.dst_ip, 
                             coap_current_packet.src_port, coap_current_packet.dst_port,
                             coap_current_packet.src_mac);
            
            // SYS_CONSOLE_PRINT("coap: packet processed\r\n");
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay
    }
}

// Initialize CoAP
void coap_init(void) {
    SYS_CONSOLE_PRINT("coap: init\r\n");

    coap_packet_ready = false;
    
    // Create CoAP task
    (void) xTaskCreate(
        (TaskFunction_t) coap_task,
        "coap_task",
        2 * 1024,  // Larger stack for CoAP processing
        NULL,
        1U,
        (TaskHandle_t*) NULL);
}

// Parse URI from CoAP options
bool coap_parse_uri(const coap_message_t *message, char *uri_buffer, uint16_t buffer_size) {
    if (message == NULL || uri_buffer == NULL || buffer_size == 0) {
        return false;
    }
    
    uri_buffer[0] = '\0';  // Start with empty string
    uint16_t uri_length = 0;
    
    // Always start with a slash
    if (buffer_size > 1) {
        uri_buffer[0] = '/';
        uri_length = 1;
    }
    
    // Look for URI_PATH options
    for (int i = 0; i < message->options_count; i++) {
        if (message->options[i].number == COAP_OPTION_URI_PATH) {
            // Add path segment
            uint16_t segment_len = message->options[i].length;
            if (uri_length + segment_len < buffer_size) {
                memcpy(&uri_buffer[uri_length], message->options[i].value, segment_len);
                uri_length += segment_len;
                uri_buffer[uri_length] = '\0';
            }
        }
    }
    
    // If no URI_PATH found, we already have "/" from above
    if (uri_length == 1) {
        return true;  // Just the root "/"
    }
    
    return true;
}