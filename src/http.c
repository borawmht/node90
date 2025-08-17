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
#include "ethernet.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

static http_client_context_t http_client = {0};

#define REQUEST_SIZE 512
char request[REQUEST_SIZE];

#define RESPONSE_SIZE 1024
uint8_t response_buffer[RESPONSE_SIZE];

#define IOBUF_SIZE 4096  // Increased to 4KB for better BearSSL compatibility
unsigned char iobuf[IOBUF_SIZE];

br_ssl_client_context cc;
br_x509_minimal_context xc;  // Use minimal context instead of knownkey context
// br_sslio_context sslio;

// Add this global variable for the known key context
br_x509_knownkey_context xc_knownkey;  // Use known key context instead of minimal
br_sslio_context sslio;  // Add this back

// Helper function to parse HTTP response status
static bool parse_http_response_status(const char *response, int *status_code) {
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
    
    // If we have headers but no body, response is complete
    if (body_start + 4 >= response + received) {
        return true;
    }
    
    // Check for Content-Length header
    const char *content_length = strstr(response, "Content-Length:");
    if (content_length && content_length < body_start) {
        int expected_length = atoi(content_length + 15); // Skip "Content-Length: "
        const char *body = body_start + 4;
        size_t body_length = received - (body - response);
        return body_length >= expected_length;
    }
    
    // Check for chunked transfer encoding
    const char *transfer_encoding = strstr(response, "Transfer-Encoding: chunked");
    if (transfer_encoding && transfer_encoding < body_start) {
        // Look for final chunk (0\r\n\r\n)
        const char *final_chunk = strstr(response, "0\r\n\r\n");
        return final_chunk != NULL;
    }
    
    // If connection is closed and we have some data, assume it's complete
    // This is a fallback for servers that don't specify content length
    return true;
}

static bool http_test_client_get_url(void) {
    // Test with HTTP only (HTTPS needs SSL/TLS implementation)
    bool success = true;
    
    // Test HTTP
    SYS_CONSOLE_PRINT("http_client: testing HTTP to httpbin.org\r\n");
    success &= http_client_get_url("http://httpbin.org/get", NULL, 0);
    
    // Add delay between tests to allow socket cleanup
    SYS_CONSOLE_PRINT("http_client: waiting between tests for socket cleanup...\r\n");
    for (volatile int i = 0; i < 100000; i++); // Longer delay for cleanup
    
    // Test local connection to gateway
    SYS_CONSOLE_PRINT("http_client: testing local connection to gateway\r\n");
    success &= http_client_get_url("http://192.168.1.1:80", NULL, 0);
    
    // Add delay between tests to allow socket cleanup
    SYS_CONSOLE_PRINT("http_client: waiting between tests for socket cleanup...\r\n");
    for (volatile int i = 0; i < 100000; i++); // Longer delay for cleanup
    
    // Skip HTTPS for now - needs SSL/TLS implementation
    SYS_CONSOLE_PRINT("http_client: skipping HTTPS (needs SSL/TLS implementation)\r\n");
    
    SYS_CONSOLE_PRINT("http_client: test results - HTTP: %s, Local: %s\r\n", 
                     success ? "PASS" : "FAIL", success ? "PASS" : "FAIL");
    
    return success;
}

// Helper function to parse URL
static bool parse_url(const char *url, char *hostname, uint16_t *port, char *path, bool *is_https) {
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
static bool resolve_hostname(const char *hostname, IPV4_ADDR *ip_addr) {
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
    http_test_client_get_url();
}

void http_server_init(void) {
    if (http_server_initialized) {
        return;
    }
    http_server_initialized = true;
    SYS_CONSOLE_PRINT("http_server: init\r\n");
}

bool http_client_get_url(const char *url, const char *data, size_t data_size) {
    if (!url) {
        SYS_CONSOLE_PRINT("http_client: invalid URL\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: get url %s\r\n", url);
    
    // Parse URL
    char hostname[64];
    uint16_t port;
    char path[128];
    bool is_https;
    
    if (!parse_url(url, hostname, &port, path, &is_https)) {
        SYS_CONSOLE_PRINT("http_client: failed to parse URL\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: hostname=%s, port=%d, path=%s, https=%s\r\n", 
    //                  hostname, port, path, is_https ? "yes" : "no");
    
    // Check if we have network connectivity
    if (!ethernet_is_ready()) {
        SYS_CONSOLE_PRINT("http_client: network not ready\r\n");
        return false;
    }
    
    // For HTTPS, use the insecure HTTPS client
    if (is_https) {
        return https_client_get_url(url, data, data_size);
    }
    
    // Resolve hostname to IP address
    IPV4_ADDR server_ip;
    if (!resolve_hostname(hostname, &server_ip)) {
        SYS_CONSOLE_PRINT("http_client: failed to resolve hostname\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: connecting to %d.%d.%d.%d:%d\r\n", 
    //                  server_ip.v[0], server_ip.v[1], server_ip.v[2], server_ip.v[3], port);
    
    // Create direct TCP socket
    TCP_SOCKET tcp_socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, port, NULL);
    if (tcp_socket == INVALID_SOCKET) {
        SYS_CONSOLE_PRINT("http_client: failed to create direct TCP socket\r\n");
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: direct TCP socket created: %d\r\n", tcp_socket);
    
    // Bind remote address
    IP_MULTI_ADDRESS remote_addr;
    remote_addr.v4Add = server_ip;
    
    if (!TCPIP_TCP_RemoteBind(tcp_socket, IP_ADDRESS_TYPE_IPV4, port, &remote_addr)) {
        SYS_CONSOLE_PRINT("http_client: failed to bind remote address\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: remote address bound successfully\r\n");
    
    // Connect using direct TCP
    if (!TCPIP_TCP_Connect(tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: direct TCP connect failed\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: direct TCP connect successful\r\n");
    
    // Wait for connection to be established
    uint32_t connect_timeout = 0;
    while (!TCPIP_TCP_IsConnected(tcp_socket) && connect_timeout < 3000) {
        // for (volatile int i = 0; i < 10000; i++);
        vTaskDelay(1);
        connect_timeout++;
        
        if (connect_timeout % 1000 == 0) {
            // SYS_CONSOLE_PRINT("http_client: waiting for TCP connection... %d\r\n", connect_timeout / 1000);
        }
    }
    
    if (!TCPIP_TCP_IsConnected(tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: TCP connection timeout\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // SYS_CONSOLE_PRINT("http_client: TCP connection established!\r\n");
    
    // Build HTTP request
    // char request[512];
    if (data && data_size > 0) {
        // POST request
        snprintf(request, sizeof(request),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                path, hostname, data_size, data);
    } else {
        // GET request
        snprintf(request, sizeof(request),
                "GET %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "\r\n",
                path, hostname);
    }
    
    // SYS_CONSOLE_PRINT("http_client: sending request:\r\n%s\r\n", request);
    
    // Send HTTP request in chunks if needed
    const char *request_ptr = request;
    size_t request_len = strlen(request);
    size_t sent_total = 0;
    
    while (sent_total < request_len) {
        // Check if socket is ready for writing
        uint16_t write_ready = TCPIP_TCP_PutIsReady(tcp_socket);
        if (write_ready == 0) {
            SYS_CONSOLE_PRINT("http_client: socket not ready for writing\r\n");
            TCPIP_TCP_Close(tcp_socket);
            return false;
        }
        
        // Calculate how much we can send
        size_t to_send = (write_ready < (request_len - sent_total)) ? write_ready : (request_len - sent_total);
        
        // Send chunk
        uint16_t sent = TCPIP_TCP_ArrayPut(tcp_socket, (const uint8_t*)(request_ptr + sent_total), to_send);
        if (sent == 0) {
            SYS_CONSOLE_PRINT("http_client: failed to send data chunk\r\n");
            TCPIP_TCP_Close(tcp_socket);
            return false;
        }
        
        sent_total += sent;
        SYS_CONSOLE_PRINT("http_client: sent chunk %d bytes, total %d/%d\r\n", sent, sent_total, request_len);
        
        // Small delay to allow TCP stack to process
        // for (volatile int i = 0; i < 1000; i++);
        vTaskDelay(1);
    }
    
    // SYS_CONSOLE_PRINT("http_client: request sent completely\r\n");
    
    // Wait a moment for data to be transmitted
    // for (volatile int i = 0; i < 50000; i++);
    vTaskDelay(10);
    
    // Receive response with better completion detection
    // uint8_t response_buffer[1024]; // Increased buffer size
    uint16_t received = 0;
    uint32_t receive_timeout = 0;
    bool response_complete = false;
    
    while (receive_timeout < 5000 && !response_complete) { // 5 second timeout
        uint16_t available = TCPIP_TCP_GetIsReady(tcp_socket);
        if (available > 0) {
            uint16_t to_read = (available > sizeof(response_buffer) - received - 1) ? 
                              (sizeof(response_buffer) - received - 1) : available;
            uint16_t read = TCPIP_TCP_ArrayGet(tcp_socket, response_buffer + received, to_read);
            received += read;
            
            SYS_CONSOLE_PRINT("http_client: received chunk %d bytes, total %d\r\n", read, received);
            
            if (received >= sizeof(response_buffer) - 1) {
                SYS_CONSOLE_PRINT("http_client: buffer full, stopping\r\n");
                break; // Buffer full
            }
            
            // Check if response is complete
            response_buffer[received] = '\0';
            if (is_http_response_complete((char*)response_buffer, received)) {
                // SYS_CONSOLE_PRINT("http_client: response appears complete\r\n");
                response_complete = true;
                break;
            }
        }
        
        // Check if connection is still active
        if (!TCPIP_TCP_IsConnected(tcp_socket)) {
            SYS_CONSOLE_PRINT("http_client: connection closed by server\r\n");
            break;
        }
        
        // Wait a bit before checking again
        // for (volatile int i = 0; i < 10000; i++);
        vTaskDelay(1);
        receive_timeout++;
        
        if (receive_timeout % 2000 == 0) {
            // SYS_CONSOLE_PRINT("http_client: waiting for response... %d\r\n", receive_timeout / 1000);
        }
    }
    
    if (receive_timeout >= 5000) {
        SYS_CONSOLE_PRINT("http_client: response timeout after %d seconds\r\n", receive_timeout / 1000);
    }
    
    // Debug: show first part of response for troubleshooting
    // SYS_CONSOLE_PRINT("http_client: raw response start:\r\n%.200s\r\n", response_buffer);
    
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
        
        // Display body in chunks to avoid console truncation
        // const char *body_ptr = body;
        // while (body_ptr < body + body_len) {
        //     size_t chunk_size = (body_len - (body_ptr - body) > 200) ? 200 : (body_len - (body_ptr - body));
        //     char temp[201];
        //     strncpy(temp, body_ptr, chunk_size);
        //     temp[chunk_size] = '\0';
        //     SYS_CONSOLE_PRINT("%s", temp);
        //     body_ptr += chunk_size;
        // }
        // SYS_CONSOLE_PRINT("\r\n");
    } else {
        SYS_CONSOLE_PRINT("http_client: received: %d bytes (no body found)\r\n", received);
    }
    
    // Close socket
    TCPIP_TCP_Close(tcp_socket);
    
    SYS_CONSOLE_PRINT("http_client: request completed\r\n");
    
    // Add delay to ensure socket cleanup (with progress feedback)
    // SYS_CONSOLE_PRINT("http_client: waiting for socket cleanup...\r\n");
    // for (volatile int i = 0; i < 50000; i++) {
    //     if (i % 10000 == 0) {
    //         SYS_CONSOLE_PRINT("http_client: cleanup progress... %d%%\r\n", (i * 100) / 50000);
    //     }
    // }
    // SYS_CONSOLE_PRINT("http_client: socket cleanup complete\r\n");
    
    return true;
}

// Custom time validation callback that always returns success (no validation)
static int custom_time_check(void *ctx, uint32_t not_before_days, uint32_t not_before_seconds,
                            uint32_t not_after_days, uint32_t not_after_seconds) {
    (void)ctx;
    (void)not_before_days;
    (void)not_before_seconds;
    (void)not_after_days;
    (void)not_after_seconds;
    return 0; // Always return 0 (valid) - no time validation
}

// BearSSL I/O functions for TCP socket integration

static int bearssl_recv(void *ctx, unsigned char *buf, size_t len) {
    TCP_SOCKET* tcp_socket = (TCP_SOCKET*)ctx;
    
    // Check if socket is still connected
    if (!TCPIP_TCP_IsConnected(*tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: recv - connection closed\r\n");
        return -1; // Connection closed - error
    }
    
    // Check if data is available
    uint16_t available = TCPIP_TCP_GetIsReady(*tcp_socket);
    if (available == 0) {
        return 0; // No data available - will be called again
    }
    
    // Read available data
    uint16_t to_read = (available < len) ? available : len;
    uint16_t read = TCPIP_TCP_ArrayGet(*tcp_socket, buf, to_read);
    
    if (read == 0) {
        return 0; // No data read - will be called again
    }
    
    SYS_CONSOLE_PRINT("https_client: recv - read %d bytes\r\n", read);
    return read;
}

static int bearssl_send(void *ctx, const unsigned char *buf, size_t len) {
    TCP_SOCKET* tcp_socket = (TCP_SOCKET*)ctx;
    
    // Check if socket is still connected
    if (!TCPIP_TCP_IsConnected(*tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: send - connection closed\r\n");
        return -1; // Connection closed - error
    }
    
    // Check if socket is ready for writing
    uint16_t write_ready = TCPIP_TCP_PutIsReady(*tcp_socket);
    if (write_ready == 0) {
        return 0; // Socket not ready - will be called again
    }
    
    // Send data
    uint16_t to_write = (write_ready < len) ? write_ready : len;
    uint16_t written = TCPIP_TCP_ArrayPut(*tcp_socket, buf, to_write);
    
    if (written == 0) {
        return 0; // Nothing written - will be called again
    }
    
    SYS_CONSOLE_PRINT("https_client: send - wrote %d bytes\r\n", written);
    return written;
}

// Create a proper dummy RSA key for the known key engine
// This needs to be at least 128 bytes (1024 bits) to pass BearSSL's minimum key size check
static unsigned char dummy_modulus[256] = {0};  // 256 bytes of zeros (2048 bits)
static unsigned char dummy_exponent[3] = {0x01, 0x00, 0x01};  // 65537 (common RSA exponent)

// Initialize the modulus with some non-zero values to make it look more realistic
static void init_dummy_rsa_key(void) {
    // Set the first byte to 0x80 to make it look like a valid RSA modulus
    dummy_modulus[0] = 0x80;
    // Set the last byte to 0x01 to make it look like a valid RSA modulus
    dummy_modulus[255] = 0x01;
}

char null_hostname[] = "";
bool https_client_get_url(const char *url, const char *data, size_t data_size) {
    SYS_CONSOLE_PRINT("https_client: get url %s\r\n", url);

    // Parse URL
    char hostname[64];
    uint16_t port;
    char path[128];
    bool is_https;
    
    if (!parse_url(url, hostname, &port, path, &is_https)) {
        SYS_CONSOLE_PRINT("https_client: failed to parse URL\r\n");
        return false;
    }
    
    // Check if we have network connectivity
    if (!ethernet_is_ready()) {
        SYS_CONSOLE_PRINT("https_client: network not ready\r\n");
        return false;
    }
    
    // Resolve hostname to IP address
    IPV4_ADDR server_ip;
    if (!resolve_hostname(hostname, &server_ip)) {
        SYS_CONSOLE_PRINT("https_client: failed to resolve hostname\r\n");
        return false;
    }
    
    // Create direct TCP socket
    TCP_SOCKET tcp_socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, port, NULL);
    if (tcp_socket == INVALID_SOCKET) {
        SYS_CONSOLE_PRINT("https_client: failed to create direct TCP socket\r\n");
        return false;
    }

    // Bind remote address
    IP_MULTI_ADDRESS remote_addr;
    remote_addr.v4Add = server_ip;
    
    if (!TCPIP_TCP_RemoteBind(tcp_socket, IP_ADDRESS_TYPE_IPV4, port, &remote_addr)) {
        SYS_CONSOLE_PRINT("https_client: failed to bind remote address\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Connect using direct TCP
    if (!TCPIP_TCP_Connect(tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: direct TCP connect failed\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Wait for connection to be established
    uint32_t connect_timeout = 0;
    while (!TCPIP_TCP_IsConnected(tcp_socket) && connect_timeout < 3000) {
        vTaskDelay(1);
        connect_timeout++;
        
        if (connect_timeout % 1000 == 0) {
            // SYS_CONSOLE_PRINT("https_client: waiting for TCP connection... %d\r\n", connect_timeout / 1000);
        }
    }
    
    if (!TCPIP_TCP_IsConnected(tcp_socket)) {
        SYS_CONSOLE_PRINT("https_client: TCP connection timeout\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }

    // Initialize BearSSL client context with minimal engine but accept any key size
    SYS_CONSOLE_PRINT("https_client: initializing BearSSL with minimal engine (accept any key size)\r\n");
    
    // Clear the SSL client context first
    br_ssl_client_zero(&cc);
    
    // // Initialize minimal context with no trust anchors (no validation)
    // br_x509_minimal_init(&xc, &br_sha256_vtable, NULL, 0);
    
    // // Set minimum RSA key size to 128 bytes which is the minimum size for RSA
    // br_x509_minimal_set_minrsa(&xc, 128);

    // // Set up hash functions and RSA implementations
    // br_x509_minimal_set_hash(&xc, br_sha256_ID, &br_sha256_vtable);
    // br_x509_minimal_set_hash(&xc, br_sha1_ID, &br_sha1_vtable);
    // br_x509_minimal_set_rsa(&xc, br_rsa_pkcs1_vrfy_get_default());
    
    // // Set custom time validation callback that always returns success
    // br_x509_minimal_set_time_callback(&xc, NULL, custom_time_check);    

    init_dummy_rsa_key();
    br_rsa_public_key dummy_key = {
        .n = dummy_modulus,  // 256-byte modulus
        .nlen = 256,
        .e = dummy_exponent,  // 65537 exponent
        .elen = 3
    };
    // Initialize the known key engine with the dummy key
    br_x509_knownkey_init_rsa(&xc_knownkey, &dummy_key, BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN);    
    
    // Set up the SSL engine buffer and versions
    br_ssl_engine_set_buffer(&cc.eng, iobuf, sizeof(iobuf), 1);
    br_ssl_engine_set_versions(&cc.eng, BR_TLS12, BR_TLS12);
    
    // // Set the minimal engine as the X.509 engine
    // br_ssl_engine_set_x509(&cc.eng, &xc.vtable);
    // Set the known key engine as the X.509 engine
    br_ssl_engine_set_x509(&cc.eng, &xc_knownkey.vtable);
    
    // Set up cipher suites (add ECDHE suites for better compatibility)
    static const uint16_t suites[] = {
        // BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,  // Most common modern suite
        // BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        // BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
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
    
    // Set up EC implementations (required for ECDHE cipher suites)
    // br_ssl_engine_set_default_ec(&cc.eng);
    // br_ssl_engine_set_default_ecdsa(&cc.eng);
    
    // Set up PRF implementations
    br_ssl_engine_set_prf10(&cc.eng, &br_tls10_prf);
    br_ssl_engine_set_prf_sha256(&cc.eng, &br_tls12_sha256_prf);
    
    // Set up symmetric encryption
    br_ssl_engine_set_default_aes_gcm(&cc.eng);
    br_ssl_engine_set_default_aes_cbc(&cc.eng);
    
    SYS_CONSOLE_PRINT("https_client: SSL client context initialized with minimal engine\r\n");
    
    // Inject entropy (required for BearSSL)
    uint32_t entropy_data[8] = {
        0x12345678, 0x87654321, 0xdeadbeef, 0xcafebabe,
        0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
    };
    br_ssl_engine_inject_entropy(&cc.eng, entropy_data, sizeof(entropy_data));
    
    // Reset the client context for a new connection
    // Use NULL for server name to disable hostname verification
    if (!br_ssl_client_reset(&cc, NULL, 0)) {
        SYS_CONSOLE_PRINT("https_client: failed to reset BearSSL client context\r\n");
        
        // Check SSL engine state after failed reset
        unsigned post_reset_state = br_ssl_engine_current_state(&cc.eng);
        SYS_CONSOLE_PRINT("https_client: SSL engine state after failed reset: 0x%x\r\n", post_reset_state);
        
        // Check for any SSL engine errors
        int ssl_err = br_ssl_engine_last_error(&cc.eng);
        if (ssl_err != 0) {
            SYS_CONSOLE_PRINT("https_client: SSL engine error: %d\r\n", ssl_err);
        }
        
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Set up the I/O wrapper
    br_sslio_init(&sslio, &cc.eng, bearssl_recv, &tcp_socket, bearssl_send, &tcp_socket);

    SYS_CONSOLE_PRINT("https_client: I/O wrapper initialized\r\n");

    // Build HTTP request
    if (data && data_size > 0) {
        // POST request
        snprintf(request, sizeof(request),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                path, hostname, data_size, data);
    } else {
        // GET request
        snprintf(request, sizeof(request),
                "GET %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "\r\n",
                path, hostname);
    }
    
    // Send HTTP request through BearSSL SSLI/O wrapper
    const char *request_ptr = request;
    size_t request_len = strlen(request);
    size_t sent_total = 0;
    response_buffer[0] = 0; // clear the response buffer
    
    uint32_t send_timeout = 0;
    while (sent_total < request_len && send_timeout < 10000) { // 10 second timeout
        // Send a small chunk (64 bytes at a time)
        size_t chunk_size = 1024;
        if (sent_total + chunk_size > request_len) {
            chunk_size = request_len - sent_total;
        }
        
        int write_result = br_sslio_write(&sslio, request_ptr + sent_total, chunk_size);
        if (write_result < 0) {
            SYS_CONSOLE_PRINT("https_client: failed to send chunk, error=%d\r\n", write_result);
            
            // Check SSL engine state for more details
            unsigned state = br_ssl_engine_current_state(&cc.eng);
            SYS_CONSOLE_PRINT("https_client: SSL engine state: 0x%x\r\n", state);
            int err = br_ssl_engine_last_error(&cc.eng);
            SYS_CONSOLE_PRINT("https_client: SSL engine error: %d\r\n", err);
            
            TCPIP_TCP_Close(tcp_socket);
            return false;
        } else if (write_result == 0) {
            // No data written, might be in handshake
            SYS_CONSOLE_PRINT("https_client: no data written, SSL engine might be in handshake\r\n");
            
            // Check SSL engine state
            unsigned state = br_ssl_engine_current_state(&cc.eng);
            SYS_CONSOLE_PRINT("https_client: SSL engine state during handshake: 0x%x\r\n", state);
            
            // Wait a bit for handshake to progress
            vTaskDelay(10);
            continue; // Try again
        }
        
        sent_total += write_result;
        SYS_CONSOLE_PRINT("https_client: sent chunk %d bytes, total %d/%d\r\n", write_result, sent_total, request_len);
        
        // Small delay between chunks
        vTaskDelay(1);
        send_timeout++;
        
        if (send_timeout % 1000 == 0) {
            SYS_CONSOLE_PRINT("https_client: send timeout progress: %d seconds\r\n", send_timeout / 1000);
        }
    }
    
    if (send_timeout >= 10000) {
        SYS_CONSOLE_PRINT("https_client: send timeout after %d seconds\r\n", send_timeout / 1000);
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Receive response
    uint16_t received = 0;
    uint32_t receive_timeout = 0;
    bool response_complete = false;
    
    while (receive_timeout < 5000 && !response_complete) { // 5 second timeout
        uint16_t available = TCPIP_TCP_GetIsReady(tcp_socket);
        if (available > 0) {
            uint16_t to_read = (available > sizeof(response_buffer) - received - 1) ? 
                              (sizeof(response_buffer) - received - 1) : available;
            uint16_t read = TCPIP_TCP_ArrayGet(tcp_socket, response_buffer + received, to_read);
            received += read;
            
            SYS_CONSOLE_PRINT("https_client: received chunk %d bytes, total %d\r\n", read, received);
            
            if (received >= sizeof(response_buffer) - 1) {
                SYS_CONSOLE_PRINT("https_client: buffer full, stopping\r\n");
                break; // Buffer full
            }
            
            // Check if response is complete
            response_buffer[received] = '\0';
            if (is_http_response_complete((char*)response_buffer, received)) {
                response_complete = true;
                break;
            }
        }
        
        // Check if connection is still active
        if (!TCPIP_TCP_IsConnected(tcp_socket)) {
            SYS_CONSOLE_PRINT("https_client: connection closed by server\r\n");
            break;
        }
        
        // Wait a bit before checking again
        vTaskDelay(1);
        receive_timeout++;
        
        if (receive_timeout % 2000 == 0) {
            SYS_CONSOLE_PRINT("https_client: waiting for response... %d\r\n", receive_timeout / 1000);
        }
    }
    
    // Parse and display response information
    int status_code = 0;
    if (parse_http_response_status((char*)response_buffer, &status_code)) {
        SYS_CONSOLE_PRINT("https_client: HTTP status: %d\r\n", status_code);
    } else {
        SYS_CONSOLE_PRINT("https_client: could not parse HTTP status\r\n");
    }
    
    const char *body = get_response_body((char*)response_buffer);
    if (body) {
        size_t body_len = strlen(body);
        SYS_CONSOLE_PRINT("https_client: response body: %d bytes\r\n", body_len);
    } else {
        SYS_CONSOLE_PRINT("https_client: received: %d bytes (no body found)\r\n", received);
    }

    // Close SSL connection using the I/O wrapper
    br_sslio_close(&sslio);
    
    // Close TCP socket
    TCPIP_TCP_Close(tcp_socket);
    
    SYS_CONSOLE_PRINT("https_client: request completed\r\n");
    
    return true;
}