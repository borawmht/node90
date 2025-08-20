/*
* http.c
* created by: Brad Oraw
* created on: 2025-08-15
*/

#include "http.h"
#include "definitions.h"
#include "config/default/library/tcpip/tcp.h"
#include "config/default/library/tcpip/dns.h"
#include "config/default/library/tcpip/tcpip.h"
// #include "third_party/wolfssl/wolfssl/ssl.h"  // Removed - using BearSSL instead
#include "bearssl/inc/bearssl.h"
#include "bearssl/bearssl_custom.h"
#include "ethernet.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declarations for static functions
static int bearssl_recv(void *ctx, unsigned char *buf, size_t len);
static int bearssl_send(void *ctx, const unsigned char *buf, size_t len);
static bool http_client_init_ssl(http_client_connection_t *conn);
static bool http_client_create_socket(const char *url, http_client_connection_t *conn);
static bool http_client_send_request(http_client_connection_t *conn, const char *data, size_t data_size);
static bool http_client_read_data(http_client_connection_t *conn, uint8_t *response_buffer, size_t buffer_size, size_t *received);
static void http_client_close_connection(http_client_connection_t *conn);

// Simple time function for WolfSSL (replaces missing SNTP function)
uint32_t TCPIP_SNTP_UTCSecondsGet(void) {
   // Return a simple timestamp - this is just for WolfSSL initialization
   // In a real application, you might want to implement a proper time source
   static uint32_t time_counter = 0;
   time_counter += 1000; // Increment by 1 second (assuming 1ms tick)
   return time_counter;
}

bool http_server_initialized = false;

// HTTP client state
typedef enum {
    HTTP_CLIENT_STATE_IDLE,
    HTTP_CLIENT_STATE_DNS_RESOLVING,
    HTTP_CLIENT_STATE_CONNECTING,
    HTTP_CLIENT_STATE_SENDING_REQUEST,
    HTTP_CLIENT_STATE_RECEIVING_RESPONSE,
    HTTP_CLIENT_STATE_COMPLETE,
    HTTP_CLIENT_STATE_ERROR
} http_client_state_t;

typedef struct {
    http_client_state_t state;    
    char hostname[64];
    uint16_t port;
    IPV4_ADDR server_ip;
    bool dns_resolved;
    bool is_https;
    uint32_t timeout_start;
    uint32_t timeout_duration;
} http_client_context_t;

// HTTP client connection structure is now defined in http.h

static http_client_context_t http_client = {0};

char request[REQUEST_SIZE];
uint8_t response_buffer[RESPONSE_SIZE];
size_t response_buffer_size = RESPONSE_SIZE;

#define IOBUF_SIZE 4096  // Increased to 4KB for better BearSSL compatibility
unsigned char iobuf[IOBUF_SIZE];

br_ssl_client_context cc;
br_sslio_context sslio;
br_x509_custom_context custom_x509_ctx;

// Helper function to parse HTTP response status
bool parse_http_response_status(const char *response, int *status_code) {
    if (!response || !status_code) return false;
    
    // Look for "HTTP/1.x XXX" pattern
    if (strncmp(response, "HTTP/1.", 7) == 0) {
        // Find the space after the version number
        const char *version_end = response + 7;
        while (*version_end && *version_end != ' ') version_end++;
        
        // Skip spaces
        while (*version_end == ' ') version_end++;
        
        // Parse the status code
        *status_code = atoi(version_end);
        return true;
    }
    
    return false;
}

// Helper function to extract response body
static const char* get_response_body(const char *response) {
    if (!response) return NULL;
    
    // Look for double CRLF that separates headers from body
    const char *body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        return body_start + 4;
    }
    
    // Fallback: look for double newline
    body_start = strstr(response, "\n\n");
    if (body_start) {
        return body_start + 2;
    }
    
    return NULL;
}

// Helper function to check if HTTP response is complete
static bool is_http_response_complete(const char *response, size_t received) {
    if (!response || received == 0) return false;
    
    // Look for double CRLF that marks end of headers
    const char *body_start = strstr(response, "\r\n\r\n");
    if (!body_start) {
        // Try single newlines as fallback
        body_start = strstr(response, "\n\n");
        if (!body_start) return false;
    }
    
    // Check for Content-Length header - be more careful about parsing
    const char *headers_end = body_start;
    const char *content_length = NULL;
    
    // Search for Content-Length in the headers section only
    const char *header_start = response;
    while (header_start < headers_end) {
        content_length = strstr(header_start, "Content-Length:");
        if (content_length && content_length < headers_end) {
            // Found Content-Length header in headers section
            break;
        } else if (content_length) {
            // Found Content-Length but it's in the body, not headers
            content_length = NULL;
            break;
        } else {
            // No Content-Length found
            break;
        }
    }
    
    if (content_length && content_length < headers_end) {
        // Parse the Content-Length value
        const char *value_start = content_length + 15; // Skip "Content-Length: "
        while (*value_start == ' ') value_start++; // Skip leading spaces
        
        int expected_length = atoi(value_start);
        const char *body = body_start + 4;
        size_t body_length = received - (body - response);
        
        SYS_CONSOLE_PRINT("https_client: Content-Length: %d, body_length: %d, received: %d\r\n", 
                         expected_length, body_length, received);
        
        return body_length >= expected_length;
    }
    
    // Check for chunked transfer encoding
    const char *transfer_encoding = strstr(response, "Transfer-Encoding: chunked");
    if (transfer_encoding && transfer_encoding < body_start) {
        // Look for final chunk (0\r\n\r\n)
        const char *final_chunk = strstr(response, "0\r\n\r\n");
        return final_chunk != NULL;
    }
    
    // For HTTPS, be more conservative - don't assume completion unless we have a lot of data
    // or the connection is clearly closed
    if (received > 1000) {
        // If we have more than 1KB and no Content-Length, assume it's complete
        return true;
    }
    
    // Don't assume completion for small responses without Content-Length
    return false;
}

static bool http_test_client_fetch(void) {
    // Test with both HTTP and HTTPS
    bool success = true;
    
    // Buffer for receiving response data
    uint8_t * response_data = response_buffer;
    size_t response_size = RESPONSE_SIZE;
    
    // Test HTTP
    SYS_CONSOLE_PRINT("http_client: testing HTTP to httpbin.org\r\n");
    success &= http_client_fetch("http://httpbin.org/get", NULL, 0, response_data, &response_size);
    if (success && response_size > 0) {
        SYS_CONSOLE_PRINT("http_client: HTTP response received: %d bytes\r\n", response_size);
        // Optionally display first part of response
        if (response_size > 0) {
            response_data[response_size < 100 ? response_size : 100] = '\0';
            SYS_CONSOLE_PRINT("http_client: HTTP response preview: %s\r\n", response_data);
        }
    }
    
    // Add delay between tests to allow socket cleanup
    SYS_CONSOLE_PRINT("http_client: waiting between tests for socket cleanup...\r\n");
    for (volatile int i = 0; i < 100000; i++); // Longer delay for cleanup
    
    // Test local connection to gateway
    SYS_CONSOLE_PRINT("http_client: testing local connection to gateway\r\n");
    response_size = sizeof(response_data);
    success &= http_client_fetch("http://192.168.1.1:80", NULL, 0, response_data, &response_size);
    
    // Add delay between tests to allow socket cleanup
    SYS_CONSOLE_PRINT("http_client: waiting between tests for socket cleanup...\r\n");
    for (volatile int i = 0; i < 100000; i++); // Longer delay for cleanup
    
    // Test HTTPS (now supported with unified client)
    SYS_CONSOLE_PRINT("http_client: testing HTTPS to httpbin.org\r\n");
    response_size = sizeof(response_data);
    success &= http_client_fetch("https://httpbin.org/get", NULL, 0, response_data, &response_size);
    if (success && response_size > 0) {
        SYS_CONSOLE_PRINT("http_client: HTTPS response received: %d bytes\r\n", response_size);
        // Optionally display first part of response
        if (response_size > 0) {
            response_data[response_size < 100 ? response_size : 100] = '\0';
            SYS_CONSOLE_PRINT("http_client: HTTPS response preview: %s\r\n", response_data);
        }
    }
    
    SYS_CONSOLE_PRINT("http_client: test results - HTTP: %s, Local: %s, HTTPS: %s\r\n", 
                     success ? "PASS" : "FAIL", success ? "PASS" : "FAIL", success ? "PASS" : "FAIL");
    
    return success;
}

// Helper function to parse URL
bool parse_url(const char *url, char *hostname, uint16_t *port, char *path, bool *is_https) {
    if (!url || !hostname || !port || !path || !is_https) {
        return false;
    }
    
    // Check for HTTPS
    if (strncmp(url, "https://", 8) == 0) {
        *is_https = true;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        *is_https = false;
        url += 7;
    } else {
        // Default to HTTP if no scheme specified
        *is_https = false;
    }
    
    // Find hostname end (either ':' for port or '/' for path)
    const char *host_end = strchr(url, ':');
    const char *path_start = strchr(url, '/');
    
    if (host_end && (!path_start || host_end < path_start)) {
        // Port is specified
        int host_len = host_end - url;
        if (host_len >= 64) {
            return false;
        }
        strncpy(hostname, url, host_len);
        hostname[host_len] = '\0';
        
        *port = (uint16_t)atoi(host_end + 1);
        if (*port == 0) {
            *port = *is_https ? 443 : 80; // Default ports
        }
        
        if (path_start) {
            strcpy(path, path_start);
        } else {
            strcpy(path, "/");
        }
    } else {
        // No port specified, use default
        const char *end = path_start ? path_start : url + strlen(url);
        int host_len = end - url;
        if (host_len >= 64) {
            return false;
        }
        strncpy(hostname, url, host_len);
        hostname[host_len] = '\0';
        
        *port = *is_https ? 443 : 80; // Default ports
        
        if (path_start) {
            strcpy(path, path_start);
        } else {
            strcpy(path, "/");
        }
    }
    
    return true;
}

// Helper function to check if string is IP address
static bool is_ip_address(const char *str) {
    if (!str) return false;
    
    int dots = 0;
    int digits = 0;
    
    for (int i = 0; str[i]; i++) {
        if (str[i] == '.') {
            dots++;
            digits = 0;
        } else if (str[i] >= '0' && str[i] <= '9') {
            digits++;
            if (digits > 3) return false; // Max 3 digits per octet
        } else {
            return false; // Invalid character
        }
    }
    
    return dots == 3; // Must have exactly 3 dots
}

// Helper function to convert string IP to IPV4_ADDR
static bool string_to_ipv4(const char *ip_str, IPV4_ADDR *ip_addr) {
    if (!ip_str || !ip_addr) return false;
    
    uint8_t octets[4];
    int result = sscanf(ip_str, "%hhu.%hhu.%hhu.%hhu", 
                       &octets[0], &octets[1], &octets[2], &octets[3]);
    
    if (result == 4) {
        ip_addr->v[0] = octets[0];
        ip_addr->v[1] = octets[1];
        ip_addr->v[2] = octets[2];
        ip_addr->v[3] = octets[3];
        return true;
    }
    
    return false;
}

// Helper function to resolve hostname to IP address
bool resolve_hostname(const char *hostname, IPV4_ADDR *ip_addr) {
    if (!hostname || !ip_addr) return false;
    
    // If it's already an IP address, convert it directly
    if (is_ip_address(hostname)) {
        return string_to_ipv4(hostname, ip_addr);
    }
    
    // SYS_CONSOLE_PRINT("http_client: resolving hostname %s\r\n", hostname);
    
    // Start DNS resolution
    TCPIP_DNS_RESULT dns_result = TCPIP_DNS_Resolve(hostname, TCPIP_DNS_TYPE_A);
    
    if (dns_result == TCPIP_DNS_RES_NO_SERVICE) {
        SYS_CONSOLE_PRINT("http_client: DNS service not available\r\n");
        return false;
    }
    
    if (dns_result != TCPIP_DNS_RES_OK && dns_result != TCPIP_DNS_RES_PENDING) {
        SYS_CONSOLE_PRINT("http_client: DNS resolution failed: %d\r\n", dns_result);
        return false;
    }
    
    // Wait for DNS resolution (with timeout)
    uint32_t timeout = 0;
    while (timeout < 10000) { // 10 second timeout
        dns_result = TCPIP_DNS_IsResolved(hostname, NULL, IP_ADDRESS_TYPE_IPV4);
        if (dns_result == TCPIP_DNS_RES_OK) {
            break;
        } else if (dns_result < 0) {
            SYS_CONSOLE_PRINT("http_client: DNS resolution failed: %d\r\n", dns_result);
            return false;
        }
        // Wait a bit before checking again
        for (volatile int i = 0; i < 10000; i++);
        timeout++;
    }
    
    if (timeout >= 10000) {
        SYS_CONSOLE_PRINT("http_client: DNS resolution timeout\r\n");
        return false;
    }
    
    // Get the resolved IP
    if (TCPIP_DNS_IsNameResolved(hostname, ip_addr, NULL) != TCPIP_DNS_RES_OK) {
        SYS_CONSOLE_PRINT("http_client: failed to get resolved IP\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: DNS resolved to %d.%d.%d.%d\r\n", 
                     ip_addr->v[0], ip_addr->v[1], ip_addr->v[2], ip_addr->v[3]);
    return true;
}

void http_init(void) {
    http_server_init();
    // http_test_client_fetch();
}

void http_server_init(void) {
    if (http_server_initialized) {
        return;
    }
    http_server_initialized = true;
    SYS_CONSOLE_PRINT("http_server: init\r\n");
}

// Helper function to create and establish TCP connection
static bool http_client_create_socket(const char *url, http_client_connection_t *conn) {
    if (!url || !conn) {
        SYS_CONSOLE_PRINT("http_client: invalid parameters\r\n");
        return false;
    }
    
    // Parse URL
    char path[128];
    if (!parse_url(url, conn->hostname, &conn->port, path, &conn->is_https)) {
        SYS_CONSOLE_PRINT("http_client: failed to parse URL\r\n");
        return false;
    }
    
    // Check if we have network connectivity
    if (!ethernet_is_ready()) {
        SYS_CONSOLE_PRINT("http_client: network not ready\r\n");
        return false;
    }
    
    // Resolve hostname to IP address
    if (!resolve_hostname(conn->hostname, &conn->server_ip)) {
        SYS_CONSOLE_PRINT("http_client: failed to resolve hostname\r\n");
        return false;
    }
    
    // Create direct TCP socket
    conn->tcp_socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, conn->port, NULL);
    if (conn->tcp_socket == INVALID_SOCKET) {
        SYS_CONSOLE_PRINT("http_client: failed to create direct TCP socket\r\n");
        return false;
    }
    
    // Bind remote address
    IP_MULTI_ADDRESS remote_addr;
    remote_addr.v4Add = conn->server_ip;
    
    if (!TCPIP_TCP_RemoteBind(conn->tcp_socket, IP_ADDRESS_TYPE_IPV4, conn->port, &remote_addr)) {
        SYS_CONSOLE_PRINT("http_client: failed to bind remote address\r\n");
        TCPIP_TCP_Close(conn->tcp_socket);
        return false;
    }
    
    // Connect using direct TCP
    if (!TCPIP_TCP_Connect(conn->tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: direct TCP connect failed\r\n");
        TCPIP_TCP_Close(conn->tcp_socket);
        return false;
    }
    
    // Wait for connection to be established
    uint32_t connect_timeout = 0;
    while (!TCPIP_TCP_IsConnected(conn->tcp_socket) && connect_timeout < 3000) {
        vTaskDelay(1);
        connect_timeout++;
        
        if (connect_timeout % 1000 == 0) {
            // SYS_CONSOLE_PRINT("http_client: waiting for TCP connection... %d\r\n", connect_timeout / 1000);
        }
    }
    
    if (!TCPIP_TCP_IsConnected(conn->tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: TCP connection timeout\r\n");
        TCPIP_TCP_Close(conn->tcp_socket);
        return false;
    }
    
    // Initialize SSL if needed
    if (conn->is_https) {
        if (!http_client_init_ssl(conn)) {
            SYS_CONSOLE_PRINT("http_client: failed to initialize SSL\r\n");
            TCPIP_TCP_Close(conn->tcp_socket);
            return false;
        }
    }
    
    return true;
}

// Helper function to initialize SSL for HTTPS connections
static bool http_client_init_ssl(http_client_connection_t *conn) {
    if (!conn || !conn->is_https) {
        return false;
    }
    
    // Initialize BearSSL client context with minimal engine but accept any key size
    br_ssl_client_zero(&cc);    
    
    // Set up the SSL engine buffer and versions
    br_ssl_engine_set_buffer(&cc.eng, iobuf, sizeof(iobuf), 1);
    br_ssl_engine_set_versions(&cc.eng, BR_TLS12, BR_TLS12);
            
    custom_x509_ctx.vtable = &no_validation_vtable;
    br_ssl_engine_set_x509(&cc.eng, &custom_x509_ctx.vtable);
    
    // Set up cipher suites (add ECDHE suites for better compatibility)
    static const uint16_t suites[] = {
        BR_TLS_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_RSA_WITH_AES_128_CBC_SHA256,
        BR_TLS_RSA_WITH_AES_128_CBC_SHA
    };
    br_ssl_engine_set_suites(&cc.eng, suites, sizeof(suites) / sizeof(suites[0]));
    
    // Set up hash functions
    br_ssl_engine_set_hash(&cc.eng, br_sha256_ID, &br_sha256_vtable);
    br_ssl_engine_set_hash(&cc.eng, br_sha1_ID, &br_sha1_vtable);
    
    // Set up RSA implementations
    br_ssl_client_set_default_rsapub(&cc);
    br_ssl_engine_set_default_rsavrfy(&cc.eng);
    
    // Set up PRF implementations
    br_ssl_engine_set_prf10(&cc.eng, &br_tls10_prf);
    br_ssl_engine_set_prf_sha256(&cc.eng, &br_tls12_sha256_prf);
    
    // Set up symmetric encryption
    br_ssl_engine_set_default_aes_gcm(&cc.eng);
    br_ssl_engine_set_default_aes_cbc(&cc.eng);
    
    // Inject entropy (required for BearSSL)
    uint32_t entropy_data[8] = {
        0x12345678, 0x87654321, 0xdeadbeef, 0xcafebabe,
        0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
    };
    br_ssl_engine_inject_entropy(&cc.eng, entropy_data, sizeof(entropy_data));
    
    // Reset the client context for a new connection
    // Use NULL for server name to disable hostname verification
    if (!br_ssl_client_reset(&cc, NULL, 0)) {
        SYS_CONSOLE_PRINT("https_client: failed to reset client context\r\n");
        
        // Check SSL engine state after failed reset
        unsigned post_reset_state = br_ssl_engine_current_state(&cc.eng);
        SYS_CONSOLE_PRINT("https_client: SSL engine state after failed reset: 0x%x\r\n", post_reset_state);
        
        // Check for any SSL engine errors
        int ssl_err = br_ssl_engine_last_error(&cc.eng);
        if (ssl_err != 0) {
            SYS_CONSOLE_PRINT("https_client: SSL engine error: %d\r\n", ssl_err);
        }
        
        return false;
    }
    
    // Set up the I/O wrapper
    br_sslio_init(&sslio, &cc.eng, bearssl_recv, &conn->tcp_socket, bearssl_send, &conn->tcp_socket);
    
    conn->ssl_ctx = &cc;
    conn->ssl_io = &sslio;
    conn->ssl_initialized = true;
    
    return true;
}

// Helper function to send HTTP request
static bool http_client_send_request(http_client_connection_t *conn, const char *data, size_t data_size) {
    if (!conn) {
        return false;
    }
    
    // Build HTTP request
    if (data && data_size > 0) {
        // POST request
        snprintf(request, sizeof(request),
                "POST / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                conn->hostname, data_size, data);
    } else {
        // GET request
        snprintf(request, sizeof(request),
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "\r\n",
                conn->hostname);
    }
    
    const char *request_ptr = request;
    size_t request_len = strlen(request);
    size_t sent_total = 0;
    
    if (conn->is_https && conn->ssl_initialized) {
        // Send through SSL
        uint32_t send_timeout = 0;
        while (sent_total < request_len && send_timeout < 3000) { // 3 second timeout        
            size_t chunk_size = 1024;
            if (sent_total + chunk_size > request_len) {
                chunk_size = request_len - sent_total;
            }
            
            int write_result = br_sslio_write(conn->ssl_io, request_ptr + sent_total, chunk_size);
            if (write_result < 0) {
                SYS_CONSOLE_PRINT("https_client: failed to send chunk, error=%d\r\n", write_result);
                return false;
            } else if (write_result == 0) {
                // No data written, might be in handshake
                vTaskDelay(10);
                continue; // Try again
            }
            
            sent_total += write_result;
            SYS_CONSOLE_PRINT("https_client: sent chunk %d bytes, total %d/%d\r\n", write_result, sent_total, request_len);
            
            vTaskDelay(1);
            send_timeout++;
        }
        
        // Flush any pending data
        if (br_sslio_flush(conn->ssl_io) < 0) {
            SYS_CONSOLE_PRINT("https_client: failed to flush SSL data\r\n");
            return false;
        }
    } else {
        // Send through plain TCP
        while (sent_total < request_len) {
            // Check if socket is ready for writing
            uint16_t write_ready = TCPIP_TCP_PutIsReady(conn->tcp_socket);
            if (write_ready == 0) {
                SYS_CONSOLE_PRINT("http_client: socket not ready for writing\r\n");
                return false;
            }
            
            // Calculate how much we can send
            size_t to_send = (write_ready < (request_len - sent_total)) ? write_ready : (request_len - sent_total);
            
            // Send chunk
            uint16_t sent = TCPIP_TCP_ArrayPut(conn->tcp_socket, (const uint8_t*)(request_ptr + sent_total), to_send);
            if (sent == 0) {
                SYS_CONSOLE_PRINT("http_client: failed to send data chunk\r\n");
                return false;
            }
            
            sent_total += sent;
            SYS_CONSOLE_PRINT("http_client: sent chunk %d bytes, total %d/%d\r\n", sent, sent_total, request_len);
            
            vTaskDelay(1);
        }
    }
    
    // Wait a moment for data to be transmitted
    vTaskDelay(10);
    
    return true;
}

// Helper function to read HTTP response
static bool http_client_read_data(http_client_connection_t *conn, uint8_t *response_buffer, size_t buffer_size, size_t *received) {
    if (!conn || !response_buffer || !received) {
        return false;
    }
    
    *received = 0;
    uint32_t receive_timeout = 0;
    bool response_complete = false;
    
    while (receive_timeout < 5000 && !response_complete) { // 5 second timeout
        if (conn->is_https && conn->ssl_initialized) {
            // Read through SSL
            // Check if there's already data available in the SSL engine
            unsigned char *buf;
            size_t alen;
            buf = br_ssl_engine_recvapp_buf(&conn->ssl_ctx->eng, &alen);
            if (alen > 0) {
                // There's already data available - read it
                size_t to_read = (alen > buffer_size - *received - 1) ? 
                                (buffer_size - *received - 1) : alen;
                memcpy(response_buffer + *received, buf, to_read);
                br_ssl_engine_recvapp_ack(&conn->ssl_ctx->eng, to_read);
                *received += to_read;
                
                SYS_CONSOLE_PRINT("https_client: read %d bytes from SSL buffer, total %d\r\n", to_read, *received);
                
                // Check if response is complete
                response_buffer[*received] = '\0';
                if (is_http_response_complete((char*)response_buffer, *received)) {
                    response_complete = true;
                    break;
                }
            } else {
                // No data in SSL buffer, try to read from network
                int read = br_sslio_read(conn->ssl_io, response_buffer + *received, 
                                buffer_size - *received - 1);
                if (read > 0) {
                    *received += read;
                    SYS_CONSOLE_PRINT("https_client: received chunk %d bytes, total %d\r\n", read, *received);
                    
                    if (*received >= buffer_size - 1) {
                        SYS_CONSOLE_PRINT("https_client: buffer full, stopping\r\n");
                        break; // Buffer full
                    }
                    
                    // Check if response is complete
                    response_buffer[*received] = '\0';
                    if (is_http_response_complete((char*)response_buffer, *received)) {
                        response_complete = true;
                        break;
                    }
                } else if (read < 0) {
                    // Error or connection closed
                    SYS_CONSOLE_PRINT("https_client: SSL read error: %d\r\n", read);
                    break;
                }
            }
        } else {
            // Read through plain TCP
            uint16_t available = TCPIP_TCP_GetIsReady(conn->tcp_socket);
            if (available > 0) {
                uint16_t to_read = (available > buffer_size - *received - 1) ? 
                                  (buffer_size - *received - 1) : available;
                uint16_t read = TCPIP_TCP_ArrayGet(conn->tcp_socket, response_buffer + *received, to_read);
                *received += read;
                
                SYS_CONSOLE_PRINT("http_client: received chunk %d bytes, total %d\r\n", read, *received);
                
                if (*received >= buffer_size - 1) {
                    SYS_CONSOLE_PRINT("http_client: buffer full, stopping\r\n");
                    break; // Buffer full
                }
                
                // Check if response is complete
                response_buffer[*received] = '\0';
                if (is_http_response_complete((char*)response_buffer, *received)) {
                    response_complete = true;
                    break;
                }
            }
        }
        
        // Check if connection is still active
        if (!TCPIP_TCP_IsConnected(conn->tcp_socket)) {
            SYS_CONSOLE_PRINT("http_client: connection closed by server\r\n");
            break;
        }
        
        // Wait a bit before checking again
        vTaskDelay(1);
        receive_timeout++;
        
        if (receive_timeout % 2000 == 0) {
            SYS_CONSOLE_PRINT("http_client: waiting for response... %d\r\n", receive_timeout / 1000);
        }
    }
    
    return true;
}

// Helper function to close HTTP client connection
static void http_client_close_connection(http_client_connection_t *conn) {
    if (!conn) {
        return;
    }
    
    if (conn->is_https && conn->ssl_initialized) {
        // Close SSL connection using the I/O wrapper
        br_sslio_close(conn->ssl_io);
    }
    
    // Close TCP socket
    TCPIP_TCP_Close(conn->tcp_socket);
}

// Important: This function should be called from a task with at least 2048 bytes of stack
// Otherwise the TCP connection times out
// This function handles both HTTP and HTTPS URLs automatically
bool http_client_fetch(const char *url, const char *data_write, size_t data_write_size, 
                        uint8_t *data_read, size_t *data_read_size) {
    if (!url) {
        SYS_CONSOLE_PRINT("http_client: invalid URL\r\n");
        return false;
    }
    
    if(data_read == NULL){
        data_read = response_buffer;
        response_buffer_size = RESPONSE_SIZE;
        data_read_size = &response_buffer_size;
    }

    SYS_CONSOLE_PRINT("http_client: get url %s\r\n", url);
    
    // Create connection structure
    http_client_connection_t conn = {0};
    
    // Step 1: Create socket and establish connection (automatically handles HTTP/HTTPS)
    if (!http_client_create_socket(url, &conn)) {
        SYS_CONSOLE_PRINT("http_client: failed to create socket\r\n");
        return false;
    }
    
    // Log the protocol being used
    SYS_CONSOLE_PRINT("http_client: using %s protocol\r\n", conn.is_https ? "HTTPS" : "HTTP");
    
    // Step 2: Send HTTP request
    if (!http_client_send_request(&conn, data_write, data_write_size)) {
        SYS_CONSOLE_PRINT("http_client: failed to send request\r\n");
        http_client_close_connection(&conn);
        return false;
    }
    
    // Step 3: Read HTTP response
    size_t received = 0;
    if (!http_client_read_data(&conn, response_buffer, sizeof(response_buffer), &received)) {
        SYS_CONSOLE_PRINT("http_client: failed to read response\r\n");
        http_client_close_connection(&conn);
        return false;
    }
    
    // Parse and display response information
    int status_code = 0;
    if (parse_http_response_status((char*)response_buffer, &status_code)) {
        SYS_CONSOLE_PRINT("http_client: HTTP status: %d\r\n", status_code);
    } else {
        SYS_CONSOLE_PRINT("http_client: could not parse HTTP status\r\n");
    }
    
    const char *body = get_response_body((char*)response_buffer);
    if (body) {
        size_t body_len = strlen(body);
        SYS_CONSOLE_PRINT("http_client: response body: %d bytes\r\n", body_len);
        
        // Copy response body to caller's buffer
        size_t copy_size = (*data_read_size < body_len) ? *data_read_size : body_len;
        memcpy(data_read, body, copy_size);
        *data_read_size = copy_size;
        
        SYS_CONSOLE_PRINT("http_client: copied %d bytes to caller buffer\r\n", copy_size);
    } else {
        SYS_CONSOLE_PRINT("http_client: received: %d bytes (no body found)\r\n", received);
        *data_read_size = 0;
    }
    
    // Step 4: Close connection
    http_client_close_connection(&conn);
    
    SYS_CONSOLE_PRINT("http_client: request completed\r\n");
    
    return true;
}

// BearSSL I/O functions for TCP socket integration
static int bearssl_recv(void *ctx, unsigned char *buf, size_t len) {
    TCP_SOCKET* tcp_socket = (TCP_SOCKET*)ctx;
    
    // Check if socket is still connected
    if (!TCPIP_TCP_IsConnected(*tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: recv - connection closed\r\n");
        return -1; // Connection closed - error
    }
    
    // Block until data is available
    uint16_t available = 0;
    uint32_t timeout = 0;
    while (available == 0 && timeout < 1000) { // 1 second timeout
        available = TCPIP_TCP_GetIsReady(*tcp_socket);
        if (available == 0) {
            vTaskDelay(1); // Small delay to prevent busy waiting
            timeout++;
        }
    }
    
    if (available == 0) {
        SYS_CONSOLE_PRINT("https_client: recv - timeout waiting for data\r\n");
        return -1; // Timeout - error
    }
    
    // Read available data
    uint16_t to_read = (available < len) ? available : len;
    uint16_t read = TCPIP_TCP_ArrayGet(*tcp_socket, buf, to_read);
    
    if (read == 0) {
        SYS_CONSOLE_PRINT("https_client: recv - failed to read data\r\n");
        return -1; // Read failed - error
    }
    
    // SYS_CONSOLE_PRINT("https_client: recv - read %d bytes\r\n", read);
    return read;
}

static int bearssl_send(void *ctx, const unsigned char *buf, size_t len) {
    TCP_SOCKET* tcp_socket = (TCP_SOCKET*)ctx;
    
    // Check if socket is still connected
    if (!TCPIP_TCP_IsConnected(*tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: send - connection closed\r\n");
        return -1; // Connection closed - error
    }
    
    // Block until socket is ready for writing
    uint16_t write_ready = 0;
    uint32_t timeout = 0;
    while (write_ready == 0 && timeout < 1000) { // 1 second timeout
        write_ready = TCPIP_TCP_PutIsReady(*tcp_socket);
        if (write_ready == 0) {
            vTaskDelay(1); // Small delay to prevent busy waiting
            timeout++;
        }
    }
    
    if (write_ready == 0) {
        SYS_CONSOLE_PRINT("https_client: send - timeout waiting for write ready\r\n");
        return -1; // Timeout - error
    }
    
    // Send data
    uint16_t to_write = (write_ready < len) ? write_ready : len;
    uint16_t written = TCPIP_TCP_ArrayPut(*tcp_socket, buf, to_write);
    
    if (written == 0) {
        SYS_CONSOLE_PRINT("https_client: send - failed to write data\r\n");
        return -1; // Write failed - error
    }
    
    // SYS_CONSOLE_PRINT("https_client: send - wrote %d bytes\r\n", written);
    return written;
}

// Convenience function for simple GET requests
bool http_client_get(const char *url, uint8_t *data_read, size_t *data_read_size) {
    return http_client_fetch(url, NULL, 0, data_read, data_read_size);
}

// Convenience function for simple POST requests
bool http_client_post(const char *url, const char *data_write, size_t data_write_size,
                     uint8_t *data_read, size_t *data_read_size) {
    return http_client_fetch(url, data_write, data_write_size, data_read, data_read_size);
}

// Initialize streaming download
bool http_stream_open(const char *url, http_stream_t *stream) {
    if (!url || !stream) {
        SYS_CONSOLE_PRINT("http_stream: invalid parameters\r\n");
        return false;
    }
    
    // Initialize stream structure
    memset(stream, 0, sizeof(http_stream_t));
    
    // Create connection
    if (!http_client_create_socket(url, &stream->conn)) {
        SYS_CONSOLE_PRINT("http_stream: failed to create socket\r\n");
        return false;
    }
    
    // Send GET request
    if (!http_client_send_request(&stream->conn, NULL, 0)) {
        SYS_CONSOLE_PRINT("http_stream: failed to send request\r\n");
        http_client_close_connection(&stream->conn);
        return false;
    }
    
    // Read headers
    size_t received = 0;
    if (!http_client_read_data(&stream->conn, response_buffer, sizeof(response_buffer), &received)) {
        SYS_CONSOLE_PRINT("http_stream: failed to read headers\r\n");
        http_client_close_connection(&stream->conn);
        return false;
    }
    
    // Parse status code
    if (!parse_http_response_status((char*)response_buffer, &stream->status_code)) {
        SYS_CONSOLE_PRINT("http_stream: could not parse HTTP status\r\n");
        http_client_close_connection(&stream->conn);
        return false;
    }
    
    if (stream->status_code != 200) {
        SYS_CONSOLE_PRINT("http_stream: HTTP error status %d\r\n", stream->status_code);
        http_client_close_connection(&stream->conn);
        return false;
    }
    
    // Parse Content-Length
    const char *content_length_header = strstr((char*)response_buffer, "Content-Length:");
    if (content_length_header) {
        stream->content_length = atoi(content_length_header + 15);
        SYS_CONSOLE_PRINT("http_stream: Content-Length: %lu bytes\r\n", stream->content_length);
    }
    
    // Find body start
    stream->body_start = strstr((char*)response_buffer, "\r\n\r\n");
    if (stream->body_start) {
        stream->body_start += 4; // Skip \r\n\r\n
        stream->body_data_in_buffer = received - (stream->body_start - (char*)response_buffer);
        SYS_CONSOLE_PRINT("http_stream: %lu bytes of body data already in buffer\r\n", stream->body_data_in_buffer);
    }
    
    stream->headers_received = true;
    SYS_CONSOLE_PRINT("http_stream: stream opened successfully\r\n");
    
    return true;
}

// Read next chunk of data
int http_stream_read(http_stream_t *stream, uint8_t *buffer, size_t buffer_size) {
    if (!stream || !buffer || buffer_size == 0) {
        return -1;
    }
    
    if (!stream->headers_received) {
        SYS_CONSOLE_PRINT("http_stream: headers not received\r\n");
        return -1;
    }
    
    if (stream->download_complete) {
        return 0; // Download complete
    }
    
    size_t bytes_read = 0;
    
    // First, return any body data that was already in the buffer
    if (stream->body_data_in_buffer > 0) {
        size_t to_copy = (stream->body_data_in_buffer < buffer_size) ? 
                        stream->body_data_in_buffer : buffer_size;
        
        memcpy(buffer, stream->body_start, to_copy);
        stream->body_start += to_copy;
        stream->body_data_in_buffer -= to_copy;
        stream->total_received += to_copy;
        bytes_read = to_copy;
        
        SYS_CONSOLE_PRINT("http_stream: returned %lu bytes from buffer\r\n", to_copy);
        
        // Check if we've read all the data
        if (stream->content_length > 0 && stream->total_received >= stream->content_length) {
            stream->download_complete = true;
            SYS_CONSOLE_PRINT("http_stream: download complete (Content-Length reached)\r\n");
            return bytes_read;
        }
        
        // If we filled the buffer, return what we have
        if (bytes_read >= buffer_size) {
            return bytes_read;
        }
    }
    
    // Now read more data from the network
    size_t remaining_buffer = buffer_size - bytes_read;
    if (remaining_buffer > 0) {
        size_t network_received = 0;
        if (http_client_read_data(&stream->conn, buffer + bytes_read, remaining_buffer, &network_received)) {
            if (network_received > 0) {
                stream->total_received += network_received;
                bytes_read += network_received;
                
                SYS_CONSOLE_PRINT("http_stream: read %lu bytes from network, total: %lu\r\n", 
                                 network_received, stream->total_received);
                
                // Check if download is complete
                if (stream->content_length > 0 && stream->total_received >= stream->content_length) {
                    stream->download_complete = true;
                    SYS_CONSOLE_PRINT("http_stream: download complete (Content-Length reached)\r\n");
                } else if (!TCPIP_TCP_IsConnected(stream->conn.tcp_socket)) {
                    stream->download_complete = true;
                    SYS_CONSOLE_PRINT("http_stream: download complete (connection closed)\r\n");
                }
            } else {
                // No more data available
                if (!TCPIP_TCP_IsConnected(stream->conn.tcp_socket)) {
                    stream->download_complete = true;
                    SYS_CONSOLE_PRINT("http_stream: download complete (connection closed)\r\n");
                }
            }
        } else {
            SYS_CONSOLE_PRINT("http_stream: failed to read from network\r\n");
            return -1;
        }
    }
    
    return bytes_read;
}

// Close streaming connection
void http_stream_close(http_stream_t *stream) {
    if (stream) {
        http_client_close_connection(&stream->conn);
        SYS_CONSOLE_PRINT("http_stream: stream closed\r\n");
    }
}

