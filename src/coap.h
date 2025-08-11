/*
* CoAP (Constrained Application Protocol) implementation
* coap.h
* created by: Brad Oraw
* created on: 2025-08-11
*/

#ifndef COAP_H
#define COAP_H

#include "stdbool.h"
#include "stdint.h"
#include "ethernet.h"

// CoAP constants
#define COAP_DEFAULT_PORT         5683
#define COAP_MAX_PAYLOAD_SIZE     1024
#define COAP_MAX_OPTIONS          16
#define COAP_MAX_TOKEN_LENGTH     8

// CoAP message types
typedef enum {
    COAP_TYPE_CON = 0,    // Confirmable
    COAP_TYPE_NON = 1,    // Non-confirmable
    COAP_TYPE_ACK = 2,    // Acknowledgement
    COAP_TYPE_RST = 3     // Reset
} coap_type_t;

// CoAP response codes
typedef enum {
    COAP_CODE_EMPTY = 0x00,
    COAP_CODE_GET = 0x01,
    COAP_CODE_POST = 0x02,
    COAP_CODE_PUT = 0x03,
    COAP_CODE_DELETE = 0x04,
    
    // Response codes
    COAP_CODE_CREATED = 0x41,
    COAP_CODE_DELETED = 0x42,
    COAP_CODE_VALID = 0x43,
    COAP_CODE_CHANGED = 0x44,
    COAP_CODE_CONTENT = 0x45,
    COAP_CODE_BAD_REQUEST = 0x80,
    COAP_CODE_UNAUTHORIZED = 0x81,
    COAP_CODE_BAD_OPTION = 0x82,
    COAP_CODE_FORBIDDEN = 0x83,
    COAP_CODE_NOT_FOUND = 0x84,
    COAP_CODE_METHOD_NOT_ALLOWED = 0x85,
    COAP_CODE_NOT_ACCEPTABLE = 0x86,
    COAP_CODE_REQUEST_ENTITY_INCOMPLETE = 0x88,
    COAP_CODE_PRECONDITION_FAILED = 0x8C,
    COAP_CODE_REQUEST_ENTITY_TOO_LARGE = 0x8D,
    COAP_CODE_UNSUPPORTED_CONTENT_FORMAT = 0x8F,
    COAP_CODE_INTERNAL_SERVER_ERROR = 0xA0,
    COAP_CODE_NOT_IMPLEMENTED = 0xA1,
    COAP_CODE_BAD_GATEWAY = 0xA2,
    COAP_CODE_SERVICE_UNAVAILABLE = 0xA3,
    COAP_CODE_GATEWAY_TIMEOUT = 0xA4,
    COAP_CODE_PROXYING_NOT_SUPPORTED = 0xA5
} coap_code_t;

// CoAP option numbers
typedef enum {
    COAP_OPTION_IF_MATCH = 1,
    COAP_OPTION_URI_HOST = 3,
    COAP_OPTION_ETAG = 4,
    COAP_OPTION_IF_NONE_MATCH = 5,
    COAP_OPTION_URI_PORT = 7,
    COAP_OPTION_LOCATION_PATH = 8,
    COAP_OPTION_URI_PATH = 11,
    COAP_OPTION_CONTENT_FORMAT = 12,
    COAP_OPTION_MAX_AGE = 14,
    COAP_OPTION_URI_QUERY = 15,
    COAP_OPTION_ACCEPT = 17,
    COAP_OPTION_LOCATION_QUERY = 20,
    COAP_OPTION_BLOCK2 = 23,
    COAP_OPTION_BLOCK1 = 27,
    COAP_OPTION_SIZE2 = 28,
    COAP_OPTION_PROXY_URI = 35,
    COAP_OPTION_PROXY_SCHEME = 39,
    COAP_OPTION_SIZE1 = 60
} coap_option_t;

// CoAP content formats
typedef enum {
    COAP_CONTENT_FORMAT_TEXT_PLAIN = 0,
    COAP_CONTENT_FORMAT_APPLICATION_LINK_FORMAT = 40,
    COAP_CONTENT_FORMAT_APPLICATION_XML = 41,
    COAP_CONTENT_FORMAT_APPLICATION_OCTET_STREAM = 42,
    COAP_CONTENT_FORMAT_APPLICATION_JSON = 50,
    COAP_CONTENT_FORMAT_APPLICATION_CBOR = 60
} coap_content_format_t;

// CoAP message structure
typedef struct {
    uint8_t version;
    coap_type_t type;
    uint8_t token_length;
    coap_code_t code;
    uint16_t message_id;
    uint8_t token[COAP_MAX_TOKEN_LENGTH];
    uint8_t payload[COAP_MAX_PAYLOAD_SIZE];
    uint16_t payload_length;
    uint8_t options_count;
    struct {
        uint16_t number;
        uint16_t length;
        uint8_t *value;
    } options[COAP_MAX_OPTIONS];
} coap_message_t;

// CoAP packet context (for packet-based approach)
typedef struct {
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t *packet_data;
    uint16_t packet_length;
} coap_packet_info_t;

// CoAP server context (simplified for packet-based approach)
typedef struct {
    bool is_server;
    uint16_t port;
    uint16_t message_id_counter;
    coap_message_t last_message;
} coap_context_t;

// Add queue structures for packet processing
// typedef struct {
//     uint8_t packet_data[COAP_MAX_PAYLOAD_SIZE];
//     uint16_t packet_length;
//     uint8_t src_ip[4];
//     uint8_t dst_ip[4];
//     uint16_t src_port;
//     uint16_t dst_port;
// } coap_packet_queue_item_t;

// Function prototypes
void coap_init(void);
bool coap_server_start(uint16_t port);
bool coap_client_init(void);

// Task-based processing
void coap_task(void *pvParameters);
bool coap_queue_packet(const uint8_t *packet_data, uint16_t packet_length, 
                      const uint8_t *src_ip, const uint8_t *dst_ip,
                      uint16_t src_port, uint16_t dst_port, uint8_t *src_mac);

// Packet-based handling functions
bool coap_handle_packet(const uint8_t *packet_data, uint16_t packet_length, 
                       const uint8_t *src_ip, const uint8_t *dst_ip,
                       uint16_t src_port, uint16_t dst_port, uint8_t *src_mac);
bool coap_send_packet_response(uint8_t *src_mac, const coap_packet_info_t *packet_info, 
                              const coap_message_t *response);

// Message handling functions
bool coap_parse_message(const uint8_t *data, uint16_t length, coap_message_t *message);
bool coap_build_message(const coap_message_t *message, uint8_t *buffer, uint16_t *length);

// Resource handling (for server)
typedef bool (*coap_resource_handler_t)(const coap_message_t *request, coap_message_t *response);
typedef struct {
    const char *path;
    coap_resource_handler_t handler;
} coap_resource_t;

bool coap_register_resource(const char *path, coap_resource_handler_t handler);

// Raw packet creation for responses
bool coap_create_udp_packet(const uint8_t *src_ip, const uint8_t *dst_ip,
                           uint16_t src_port, uint16_t dst_port,
                           const uint8_t *payload, uint16_t payload_length,
                           uint8_t *packet_buffer, uint16_t *packet_length);

// URI parsing function
bool coap_parse_uri(const coap_message_t *message, char *uri_buffer, uint16_t buffer_size);

#endif /* COAP_H */ 