/*
* http.h
* created by: Brad Oraw
* created on: 2025-08-15
*
* HTTP/HTTPS Client Library
* 
* Usage Examples:
* 
* // Simple GET request
* uint8_t response[1024];
* size_t response_size = sizeof(response);
* if (http_client_get("http://example.com/api", response, &response_size)) {
*     // response contains the response body, response_size contains actual bytes read
*     printf("Received %d bytes: %.*s\n", response_size, response_size, response);
* }
* 
* // Simple POST request
* const char *post_data = "name=value&other=data";
* if (http_client_post("https://example.com/api", post_data, strlen(post_data), 
*                     response, &response_size)) {
*     // response contains the response body
* }
* 
 * // Advanced usage with full control
 * if (http_client_fetch("https://api.example.com/data", NULL, 0, response, &response_size)) {
 *     // Custom handling
 * }
 * 
 * // Streaming download for large files (like firmware updates)
 * http_stream_t stream;
 * if (http_stream_open("http://example.com/firmware.bin", &stream)) {
 *     uint8_t buffer[1024];
 *     while (1) {
 *         int bytes_read = http_stream_read(&stream, buffer, sizeof(buffer));
 *         if (bytes_read < 0) {
 *             // Error occurred
 *             break;
 *         }
 *         if (bytes_read == 0) {
 *             // Download complete
 *             break;
 *         }
 *         // Process the chunk (e.g., write to flash)
 *         process_chunk(buffer, bytes_read);
 *     }
 *     http_stream_close(&stream);
 * }
 */

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "definitions.h"
#include "config/default/library/tcpip/tcp.h"
#include "bearssl/inc/bearssl.h"

#define REQUEST_SIZE 512
extern char request[REQUEST_SIZE];

#define RESPONSE_SIZE 2048
extern uint8_t response_buffer[RESPONSE_SIZE];
extern size_t response_buffer_size;

void http_init(void);
void http_server_init(void);
// Unified HTTP client that handles both HTTP and HTTPS URLs automatically
// data_write: data to send (for POST requests), NULL for GET requests
// data_write_size: size of data to send
// data_read: buffer to receive response data
// data_read_size: size of data_read buffer, returns actual bytes read
bool http_client_fetch(const char *url, const char *data_write, size_t data_write_size, 
                        uint8_t *data_read, size_t *data_read_size);

// Convenience function for simple GET requests
// Returns true on success, false on failure
bool http_client_get(const char *url, uint8_t *data_read, size_t *data_read_size);

// Convenience function for simple POST requests
// Returns true on success, false on failure
bool http_client_post(const char *url, const char *data_write, size_t data_write_size,
                     uint8_t *data_read, size_t *data_read_size);

// HTTP client connection structure (forward declaration)
typedef struct http_client_connection {
    char hostname[64];
    uint16_t port;
    IPV4_ADDR server_ip;
    bool is_https;
    TCP_SOCKET tcp_socket;
    bool ssl_initialized;
    br_ssl_client_context *ssl_ctx;
    br_sslio_context *ssl_io;
} http_client_connection_t;

// Streaming HTTP client for large downloads
typedef struct {
    http_client_connection_t conn;
    bool headers_received;
    bool download_complete;
    uint32_t total_received;
    uint32_t content_length;
    int status_code;
    char *body_start;
    size_t body_data_in_buffer;
} http_stream_t;

// Initialize streaming download
bool http_stream_open(const char *url, http_stream_t *stream);

// Read next chunk of data (returns bytes read, 0 if complete, -1 on error)
int http_stream_read(http_stream_t *stream, uint8_t *buffer, size_t buffer_size);

// Close streaming connection
void http_stream_close(http_stream_t *stream);

// Helper functions for URL parsing and network operations
bool parse_url(const char *url, char *hostname, uint16_t *port, char *path, bool *is_https);
bool resolve_hostname(const char *hostname, IPV4_ADDR *ip_addr);
bool parse_http_response_status(const char *response, int *status_code);

#endif // HTTP_H