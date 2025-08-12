/*
* CoAP (Constrained Application Protocol) implementation
* coap.c
* created by: Brad Oraw
* created on: 2025-08-11
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
    
    // Create packet using structs for clarity
    coap_packet_t packet = {0};
    
    // Fill Ethernet header (will be filled by ethernet_send_to, but we can set ethertype)
    packet.eth.ethertype = TCPIP_Helper_htons(0x0800);  // IPv4
    
    // Fill IP header
    packet.ip.version_ihl = 0x45;  // Version 4, header length 5 words
    packet.ip.tos = 0x00;          // Type of Service
    packet.ip.total_length = TCPIP_Helper_htons(sizeof(ip_header_t) + sizeof(udp_header_t) + payload_length);
    packet.ip.identification = 0;   // Identification
    packet.ip.flags_offset = 0x4000;  // Flags, Fragment offset
    packet.ip.ttl = 64;              // Time to Live
    packet.ip.protocol = 17;         // Protocol (UDP)
    packet.ip.checksum = 0;          // Checksum (optional for IPv4)
    memcpy(packet.ip.src_ip, src_ip, 4);
    memcpy(packet.ip.dst_ip, dst_ip, 4);
    
    // Fill UDP header
    packet.udp.src_port = TCPIP_Helper_htons(src_port);
    packet.udp.dst_port = TCPIP_Helper_htons(dst_port);
    packet.udp.length = TCPIP_Helper_htons(sizeof(udp_header_t) + payload_length);
    packet.udp.checksum = 0;  // Checksum (optional for IPv4)
    
    // Copy payload
    memcpy(packet.payload, payload, payload_length);
    
    // Copy the packet to the buffer (skip Ethernet header as it's filled by ethernet_send_to)
    memcpy(packet_buffer, (uint8_t*)&packet.eth, sizeof(ethernet_header_t));
    memcpy(packet_buffer + sizeof(ethernet_header_t), (uint8_t*)&packet.ip, sizeof(ip_header_t));
    memcpy(packet_buffer + sizeof(ethernet_header_t) + sizeof(ip_header_t), (uint8_t*)&packet.udp, sizeof(udp_header_t));
    memcpy(packet_buffer + sizeof(ethernet_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t), payload, payload_length);
    
    *packet_length = sizeof(ethernet_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + payload_length;
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
    if (coap_ctx.is_server && request.type == COAP_TYPE_CON) {
        // SYS_CONSOLE_PRINT("coap: creating response\r\n");
        
        coap_message_t response = {0};
        response.version = 1;
        response.type = (request.type == COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON;
        response.message_id = request.message_id;
        response.token_length = request.token_length;
        memcpy(response.token, request.token, request.token_length);
        response.content_format = COAP_CONTENT_FORMAT_TEXT_PLAIN;
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
        bool resource_result = false;
        for (int i = 0; i < coap_resource_count; i++) {
            // SYS_CONSOLE_PRINT("coap: checking resource '%s' against '%s'\r\n", coap_resources[i].path, request_uri);
            if (strcmp(coap_resources[i].path, request_uri) == 0) {
                // SYS_CONSOLE_PRINT("coap: calling resource handler\r\n");
                SYS_CONSOLE_PRINT("coap: resource handler: %s\r\n", coap_resources[i].path);
                resource_found = true;
                resource_result = coap_resources[i].handler(&request, &response);
                // SYS_CONSOLE_PRINT("coap: resource handler returned: %d\r\n", resource_found);
                break;
            }
        }
        
        if (!resource_found) {
            SYS_CONSOLE_PRINT("coap: no resource found for '%s', setting NOT_FOUND\r\n", request_uri);
            response.code = COAP_CODE_NOT_FOUND;
        }
        
        // Add URI_PATH option to response first (lower option number)
        // if (resource_found && response.options_count < COAP_MAX_OPTIONS) {
        //     // Remove leading slash for the option value
        //     const char *path_segment = request_uri;
        //     if (path_segment[0] == '/') {
        //         path_segment++;  // Skip leading slash
        //     }
            
        //     if (strlen(path_segment) > 0) {
        //         response.options[response.options_count].number = COAP_OPTION_URI_PATH;
        //         response.options[response.options_count].length = strlen(path_segment);
        //         response.options[response.options_count].value = (uint8_t*)path_segment;
        //         response.options_count++;
        //         // SYS_CONSOLE_PRINT("coap: added URI_PATH option: '%s'\r\n", path_segment);
        //     }
        // }
        
        // Add Content-Format option for text responses (higher option number)
        if(resource_found && request.code == COAP_CODE_GET){
            if (resource_result && response.options_count < COAP_MAX_OPTIONS) {
                response.options[response.options_count].number = COAP_OPTION_CONTENT_FORMAT;
                response.options[response.options_count].length = 1;
                response.options[response.options_count].value = (uint8_t*)&(uint8_t){response.content_format};
                response.options_count++;
                // SYS_CONSOLE_PRINT("coap: added Content-Format option\r\n");
            }
            else if(!resource_result){
                response.code = COAP_CODE_NOT_FOUND;
            }
        }
        else if(request.code == COAP_CODE_PUT){
            if(resource_found){
                response.code = COAP_CODE_CHANGED;
            }
            else{
                response.code = COAP_CODE_BAD_REQUEST;
            }
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

// Simple checksum calculation function
static uint16_t calculate_ip_checksum(const uint8_t *data, uint16_t length) {
    uint32_t sum = 0;
    uint16_t i;
    
    // Sum 16-bit words
    for (i = 0; i < length - 1; i += 2) {
        sum += (data[i] << 8) | data[i + 1];
    }
    
    // Handle odd byte if present
    if (i < length) {
        sum += data[i] << 8;
    }
    
    // Add carry and take one's complement
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

uint32_t net_checksum_add(int len, uint8_t *buf)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (i & 1)
            sum += (uint32_t)buf[i];
        else
            sum += (uint32_t)buf[i] << 8;
    }
    return sum;
}

uint16_t net_checksum_finish(uint32_t sum)
{
    while (sum>>16)
	    sum = (sum & 0xFFFF)+(sum >> 16);
    return ~sum;
}

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf)
{
    uint32_t sum = 0;

    sum += net_checksum_add(length, buf);         // payload
    sum += net_checksum_add(8, addrs);            // src + dst address
    sum += proto + length;                        // protocol & length
    return net_checksum_finish(sum);
}

void net_checksum_calculate(uint8_t *data, int length)
{
    int hlen, plen, proto, csum_offset;
    uint16_t csum;

    if ((data[14] & 0xf0) != 0x40)
	    return; /* not IPv4 */
    hlen  = (data[14] & 0x0f) * 4;
    plen  = (data[16] << 8 | data[17]) - hlen;
    proto = data[23];

    switch (proto) {
        case PROTO_TCP:
            csum_offset = 16;
            break;
        case PROTO_UDP:
            csum_offset = 6;
            break;
        default:
            return;
    }

    if (plen < csum_offset+2)
	    return;

    data[14+hlen+csum_offset]   = 0;
    data[14+hlen+csum_offset+1] = 0;
    csum = net_checksum_tcpudp(plen, proto, data+14+12, data+14+hlen);
    data[14+hlen+csum_offset]   = csum >> 8;
    data[14+hlen+csum_offset+1] = csum & 0xff;
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
    
    // Create packet using structs for clarity
    coap_packet_t packet = {0};
    
    // Fill Ethernet header (will be filled by ethernet_send_to, but we can set ethertype)
    memcpy(packet.eth.dst_mac,src_mac,6);
    memcpy(packet.eth.src_mac,ethernet_getMACAddress(),6);
    packet.eth.ethertype = TCPIP_Helper_htons(0x0800);  // IPv4
    
    // Fill IP header
    packet.ip.version_ihl = 0x45;  // Version 4, header length 5 words
    packet.ip.tos = 0x00;          // Type of Service
    packet.ip.total_length = TCPIP_Helper_htons(sizeof(ip_header_t) + sizeof(udp_header_t) + coap_length);
    //packet.ip.identification = TCPIP_Helper_htons(0x1234);  // Identification
    packet.ip.identification = TCPIP_Helper_htons(coap_ctx.message_id_counter++);  // Identification
    packet.ip.flags_offset = 0x0000;  // Don't Fragment, Fragment Offset: 0
    packet.ip.ttl = 100;              // Time to Live
    packet.ip.protocol = 17;         // Protocol (UDP)
    packet.ip.checksum = 0;          // Will be calculated
    memcpy(packet.ip.src_ip, packet_info->src_ip, 4);
    memcpy(packet.ip.dst_ip, packet_info->dst_ip, 4);
    
    // Fill UDP header
    packet.udp.src_port = TCPIP_Helper_htons(packet_info->src_port);
    packet.udp.dst_port = TCPIP_Helper_htons(packet_info->dst_port);
    packet.udp.length = TCPIP_Helper_htons(sizeof(udp_header_t) + coap_length);
    packet.udp.checksum = 0;  // Will be calculated
    
    // Copy CoAP payload
    memcpy(packet.payload, coap_buffer, coap_length);
    
    // Calculate IP checksum
    packet.ip.checksum = TCPIP_Helper_htons(calculate_ip_checksum((uint8_t*)&packet.ip, sizeof(ip_header_t)));

    net_checksum_calculate((uint8_t*)&packet, sizeof(ethernet_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + coap_length);
    // SYS_CONSOLE_PRINT("coap: ip.checksum: %04x\r\n", packet.ip.checksum);
    // SYS_CONSOLE_PRINT("coap: udp.checksum: %04x\r\n", packet.udp.checksum);
    
    // Debug: Print packet structure
    // coap_debug_packet(&packet, coap_length);
    
    // SYS_CONSOLE_PRINT("coap: packet created, total length: %d\r\n", sizeof(ethernet_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + coap_length);
    
    // Send the packet (skip Ethernet header as it's filled by ethernet_send_to)
    size_t packet_length = sizeof(ethernet_header_t) + sizeof(ip_header_t) + sizeof(udp_header_t) + coap_length;
    if (!ethernet_send((uint8_t*)&packet, packet_length)) {
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
        SYS_CONSOLE_PRINT("coap: packet too long\r\n");
        return false;
    }

    if (coap_packet_ready) {
        SYS_CONSOLE_PRINT("coap: packet already queued\r\n");
        return false;
    }

    // SYS_CONSOLE_PRINT("coap: queueing packet\r\n");

    // SYS_CONSOLE_PRINT("coap: length %d\r\n",packet_length);

    // SYS_CONSOLE_PRINT("coap: src_ip %d.%d.%d.%d\r\n",src_ip[0],src_ip[1],src_ip[2],src_ip[3]);
    // SYS_CONSOLE_PRINT("coap: dst_ip %d.%d.%d.%d\r\n",dst_ip[0],dst_ip[1],dst_ip[2],dst_ip[3]);
    // SYS_CONSOLE_PRINT("coap: src_port %d\r\n",src_port);
    // SYS_CONSOLE_PRINT("coap: dst_port %d\r\n",dst_port);
    // SYS_CONSOLE_PRINT("coap: src_mac %02X:%02X:%02X:%02X:%02X:%02X\r\n",src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5]);

    // SYS_CONSOLE_PRINT("coap: packet data: ");
    // for (int i = 0; i < packet_length; i++) {
    //     SYS_CONSOLE_PRINT("%02X ", packet_data[i]);
    // }
    // SYS_CONSOLE_PRINT("\r\n");

    memcpy(coap_current_packet.packet_data, packet_data, packet_length);
    coap_current_packet.packet_length = packet_length;
    memcpy(coap_current_packet.src_ip, src_ip, 4);
    memcpy(coap_current_packet.dst_ip, dst_ip, 4);
    coap_current_packet.src_port = src_port;
    coap_current_packet.dst_port = dst_port;
    memcpy(coap_current_packet.src_mac, src_mac, 6);

    coap_packet_ready = true;
    // SYS_CONSOLE_PRINT("coap: packet queued\r\n");
    return true;
}

// CoAP task function
void coap_task(void *pvParameters) {
    SYS_CONSOLE_PRINT("coap: task start\r\n");
    
    while (1) {
        if (coap_packet_ready) {
            // SYS_CONSOLE_PRINT("coap: processing packet\r\n");                        
            
            // Process the packet
            coap_handle_packet(coap_current_packet.packet_data, coap_current_packet.packet_length,
                             coap_current_packet.src_ip, coap_current_packet.dst_ip, 
                             coap_current_packet.src_port, coap_current_packet.dst_port,
                             coap_current_packet.src_mac);

            coap_packet_ready = false;
            
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
            if(uri_length>1){
                uri_buffer[uri_length] = '/';
                uri_length++;
                uri_buffer[uri_length] = '\0';
            }
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

// Add these functions to your coap.c implementation

bool coap_set_content_format_option(coap_message_t *message, coap_content_format_t format) {
    if (!message || message->options_count >= COAP_MAX_OPTIONS) {
        return false;
    }
    
    // Find if content format option already exists
    for (int i = 0; i < message->options_count; i++) {
        if (message->options[i].number == COAP_OPTION_CONTENT_FORMAT) {
            // Update existing option
            uint16_t format_value = format;
            message->options[i].length = 2;
            message->options[i].value = (uint8_t*)&format_value;
            return true;
        }
    }
    
    // Add new content format option
    uint16_t format_value = format;
    message->options[message->options_count].number = COAP_OPTION_CONTENT_FORMAT;
    message->options[message->options_count].length = 2;
    message->options[message->options_count].value = (uint8_t*)&format_value;
    message->options_count++;
    
    return true;
}

coap_content_format_t coap_get_content_format_option(const coap_message_t *message) {
    if (!message) {
        return COAP_CONTENT_FORMAT_TEXT_PLAIN; // Default
    }
    
    for (int i = 0; i < message->options_count; i++) {
        if (message->options[i].number == COAP_OPTION_CONTENT_FORMAT) {
            if (message->options[i].length == 2) {
                return (coap_content_format_t)*(uint16_t*)message->options[i].value;
            }
        }
    }
    
    return COAP_CONTENT_FORMAT_TEXT_PLAIN; // Default if not found
}

// Debug function to print packet structure
void coap_debug_packet(const coap_packet_t *packet, uint16_t payload_length) {
    SYS_CONSOLE_PRINT("=== CoAP Packet Debug ===\r\n");
    
    // Print Ethernet header
    SYS_CONSOLE_PRINT("Ethernet: dst_mac=%02x:%02x:%02x:%02x:%02x:%02x, src_mac=%02x:%02x:%02x:%02x:%02x:%02x, ethertype=0x%04x\r\n",
                      packet->eth.dst_mac[0], packet->eth.dst_mac[1], packet->eth.dst_mac[2],
                      packet->eth.dst_mac[3], packet->eth.dst_mac[4], packet->eth.dst_mac[5],
                      packet->eth.src_mac[0], packet->eth.src_mac[1], packet->eth.src_mac[2],
                      packet->eth.src_mac[3], packet->eth.src_mac[4], packet->eth.src_mac[5],
                      TCPIP_Helper_ntohs(packet->eth.ethertype));
    
    // Print IP header
    SYS_CONSOLE_PRINT("IP: version_ihl=0x%02x, tos=0x%02x, total_length=%d, id=0x%04x\r\n",
                      packet->ip.version_ihl, packet->ip.tos, 
                      TCPIP_Helper_ntohs(packet->ip.total_length),
                      TCPIP_Helper_ntohs(packet->ip.identification));
    SYS_CONSOLE_PRINT("IP: flags_offset=0x%04x, ttl=%d, protocol=%d, checksum=0x%04x\r\n",
                      packet->ip.flags_offset, packet->ip.ttl, packet->ip.protocol,
                      packet->ip.checksum);
    SYS_CONSOLE_PRINT("IP: src_ip=%d.%d.%d.%d, dst_ip=%d.%d.%d.%d\r\n",
                      packet->ip.src_ip[0], packet->ip.src_ip[1], packet->ip.src_ip[2], packet->ip.src_ip[3],
                      packet->ip.dst_ip[0], packet->ip.dst_ip[1], packet->ip.dst_ip[2], packet->ip.dst_ip[3]);
    
    // Print UDP header
    SYS_CONSOLE_PRINT("UDP: src_port=%d, dst_port=%d, length=%d, checksum=0x%04x\r\n",
                      TCPIP_Helper_ntohs(packet->udp.src_port),
                      TCPIP_Helper_ntohs(packet->udp.dst_port),
                      TCPIP_Helper_ntohs(packet->udp.length),
                      packet->udp.checksum);
    
    // Print payload length
    SYS_CONSOLE_PRINT("Payload: length=%d\r\n", payload_length);
    
    // Print first few bytes of payload
    SYS_CONSOLE_PRINT("Payload data: ");
    for (int i = 0; i < (payload_length > 16 ? 16 : payload_length); i++) {
        SYS_CONSOLE_PRINT("%02x ", packet->payload[i]);
    }
    if (payload_length > 16) {
        SYS_CONSOLE_PRINT("...");
    }
    SYS_CONSOLE_PRINT("\r\n");
    
    SYS_CONSOLE_PRINT("=== End Debug ===\r\n");
}

// Debug function to test UDP checksum calculation
void debug_udp_checksum(void) {
    SYS_CONSOLE_PRINT("=== UDP Checksum Debug ===\r\n");
    
    // Test packet data from your example (this includes Ethernet + IP + UDP)
    uint8_t test_packet[] = {
        0xfc, 0x0f, 0xe7, 0xc1, 0xda, 0xf0, 0x00, 0xe0, 0x4c, 0x68, 0x00, 0x5a, 0x08, 0x00, 0x45, 0x00,
        0x00, 0x2c, 0x2d, 0x4e, 0x40, 0x00, 0x40, 0x11, 0x89, 0xc1, 0xc0, 0xa8, 0x01, 0x44, 0xc0, 0xa8,
        0x01, 0x1d, 0xde, 0x80, 0x16, 0x33, 0x00, 0x18, 0x83, 0xdb, 0x40, 0x01, 0x32, 0xe5, 0xb3, 0x69,
        0x6e, 0x78, 0x07, 0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b
    };

    coap_packet_t *packet = (coap_packet_t *)test_packet;

    SYS_CONSOLE_PRINT("coap: udp.checksum: %04x\r\n", packet->udp.checksum);

    packet->udp.checksum = 0;

    net_checksum_calculate(test_packet, sizeof(test_packet));

    SYS_CONSOLE_PRINT("coap: udp.checksum: %04x\r\n", packet->udp.checksum);
    
    SYS_CONSOLE_PRINT("=== End UDP Checksum Debug ===\r\n");
}