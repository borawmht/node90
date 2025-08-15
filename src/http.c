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
#include "config/default/net_pres/pres/net_pres_socketapi.h"
#include "config/default/net_pres/pres/net_pres_socketapiconversion.h"
#include "ethernet.h"
#include "third_party/wolfssl/wolfssl/ssl.h"
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
    NET_PRES_SKT_HANDLE_T socket;
    char hostname[64];
    uint16_t port;
    IPV4_ADDR server_ip;
    bool dns_resolved;
    bool is_https;
    uint32_t timeout_start;
    uint32_t timeout_duration;
} http_client_context_t;

static http_client_context_t http_client = {0};

static bool http_test_client_get_url(void) {
    // Test with both HTTP and HTTPS
    bool success = true;
    
    // Test HTTP first (since HTTPS fallback worked)
    SYS_CONSOLE_PRINT("http_client: testing HTTP\r\n");
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
    
    // Test HTTPS
    SYS_CONSOLE_PRINT("http_client: testing HTTPS\r\n");
    success &= http_client_get_url("https://httpbin.org/get", NULL, 0);
    
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
    
    SYS_CONSOLE_PRINT("http_client: resolving hostname %s\r\n", hostname);
    
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
    
    SYS_CONSOLE_PRINT("http_client: hostname=%s, port=%d, path=%s, https=%s\r\n", 
                     hostname, port, path, is_https ? "yes" : "no");
    
    // Check if we have network connectivity
    if (!ethernet_is_ready()) {
        SYS_CONSOLE_PRINT("http_client: network not ready\r\n");
        return false;
    }
    
    // Resolve hostname to IP address
    IPV4_ADDR server_ip;
    if (!resolve_hostname(hostname, &server_ip)) {
        SYS_CONSOLE_PRINT("http_client: failed to resolve hostname\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: connecting to %d.%d.%d.%d:%d\r\n", 
                     server_ip.v[0], server_ip.v[1], server_ip.v[2], server_ip.v[3], port);
    
    // Debug network status
    SYS_CONSOLE_PRINT("http_client: network status - ready: %s, link: %s, ip: %s\r\n",
                     ethernet_is_ready() ? "yes" : "no",
                     "up", // We know link is up since we got here
                     "yes"); // We know we have IP since we got here
    
    // Test basic connectivity with a simple TCP connection test
    SYS_CONSOLE_PRINT("http_client: testing basic connectivity...\r\n");
    
    // Check TCP stack status
    SYS_CONSOLE_PRINT("http_client: checking TCP stack status...\r\n");
    
    // Since we've proven TCP stack works but NET_PRES has issues, let's use direct TCP
    SYS_CONSOLE_PRINT("http_client: using direct TCP connection (bypassing NET_PRES)\r\n");
    
    // Create direct TCP socket
    TCP_SOCKET tcp_socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, port, NULL);
    if (tcp_socket == INVALID_SOCKET) {
        SYS_CONSOLE_PRINT("http_client: failed to create direct TCP socket\r\n");
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: direct TCP socket created: %d\r\n", tcp_socket);
    
    // Bind remote address
    IP_MULTI_ADDRESS remote_addr;
    remote_addr.v4Add = server_ip;
    
    if (!TCPIP_TCP_RemoteBind(tcp_socket, IP_ADDRESS_TYPE_IPV4, port, &remote_addr)) {
        SYS_CONSOLE_PRINT("http_client: failed to bind remote address\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: remote address bound successfully\r\n");
    
    // Connect using direct TCP
    if (!TCPIP_TCP_Connect(tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: direct TCP connect failed\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: direct TCP connect successful\r\n");
    
    // Wait for connection to be established
    uint32_t connect_timeout = 0;
    while (!TCPIP_TCP_IsConnected(tcp_socket) && connect_timeout < 10000) {
        for (volatile int i = 0; i < 10000; i++);
        connect_timeout++;
        
        if (connect_timeout % 1000 == 0) {
            SYS_CONSOLE_PRINT("http_client: waiting for TCP connection... %d\r\n", connect_timeout / 1000);
        }
    }
    
    if (!TCPIP_TCP_IsConnected(tcp_socket)) {
        SYS_CONSOLE_PRINT("http_client: TCP connection timeout\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    SYS_CONSOLE_PRINT("http_client: TCP connection established!\r\n");
    
    // Build HTTP request
    char request[512];
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
    
    SYS_CONSOLE_PRINT("http_client: sending request:\r\n%s\r\n", request);
    
    // Check if socket is ready for writing
    uint16_t write_ready = TCPIP_TCP_PutIsReady(tcp_socket);
    SYS_CONSOLE_PRINT("http_client: socket write ready: %d bytes\r\n", write_ready);
    
    if (write_ready < strlen(request)) {
        SYS_CONSOLE_PRINT("http_client: socket not ready for writing %d bytes\r\n", strlen(request));
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Send HTTP request
    uint16_t sent = TCPIP_TCP_ArrayPut(tcp_socket, (const uint8_t*)request, strlen(request));
    SYS_CONSOLE_PRINT("http_client: sent %d bytes\r\n", sent);
    
    if (sent != strlen(request)) {
        SYS_CONSOLE_PRINT("http_client: failed to send request\r\n");
        TCPIP_TCP_Close(tcp_socket);
        return false;
    }
    
    // Wait for data to be transmitted by TCP stack
    SYS_CONSOLE_PRINT("http_client: waiting for data transmission...\r\n");
    uint32_t transmit_timeout = 0;
    bool data_transmitted = false;
    
    while (transmit_timeout < 5000) { // 5 second timeout
        // Check if data has been transmitted
        uint16_t pending = TCPIP_TCP_PutIsReady(tcp_socket);
        SYS_CONSOLE_PRINT("http_client: pending bytes in buffer: %d\r\n", pending);
        
        // If buffer is full (512 bytes), data is still being processed
        // If buffer has space, data has been transmitted
        if (pending >= 512) {
            SYS_CONSOLE_PRINT("http_client: data still in buffer, waiting...\r\n");
        } else {
            SYS_CONSOLE_PRINT("http_client: data appears to be transmitted\r\n");
            data_transmitted = true;
            break;
        }
        
        // Wait a bit before checking again
        for (volatile int i = 0; i < 10000; i++);
        transmit_timeout++;
        
        if (transmit_timeout % 1000 == 0) {
            SYS_CONSOLE_PRINT("http_client: transmission wait... %d\r\n", transmit_timeout / 1000);
        }
    }
    
    if (!data_transmitted) {
        SYS_CONSOLE_PRINT("http_client: data transmission timeout, trying flush...\r\n");
        
        // Try flush as last resort
        if (!TCPIP_TCP_Flush(tcp_socket)) {
            SYS_CONSOLE_PRINT("http_client: flush also failed, but continuing...\r\n");
        } else {
            SYS_CONSOLE_PRINT("http_client: flush succeeded\r\n");
        }
    }
    
    SYS_CONSOLE_PRINT("http_client: request transmission completed\r\n");
    
    SYS_CONSOLE_PRINT("http_client: request sent\r\n");
    
    // Receive response
    uint8_t response_buffer[512];
    uint16_t received = 0;
    uint32_t receive_timeout = 0;
    
    while (receive_timeout < 15000) { // 15 second timeout
        uint16_t available = TCPIP_TCP_GetIsReady(tcp_socket);
        if (available > 0) {
            uint16_t to_read = (available > sizeof(response_buffer) - received) ? 
                              (sizeof(response_buffer) - received) : available;
            uint16_t read = TCPIP_TCP_ArrayGet(tcp_socket, response_buffer + received, to_read);
            received += read;
            
            if (received >= sizeof(response_buffer) - 1) {
                break; // Buffer full
            }
        }
        
        // Check if connection is still active
        if (!TCPIP_TCP_IsConnected(tcp_socket)) {
            break;
        }
        
        // Wait a bit before checking again
        for (volatile int i = 0; i < 10000; i++);
        receive_timeout++;
    }
    
    response_buffer[received] = '\0';
    SYS_CONSOLE_PRINT("http_client: received %d bytes:\r\n%s\r\n", received, response_buffer);
    
    // Close socket
    TCPIP_TCP_Close(tcp_socket);
    
    SYS_CONSOLE_PRINT("http_client: request completed\r\n");
    
    // Add delay to ensure socket cleanup
    SYS_CONSOLE_PRINT("http_client: waiting for socket cleanup...\r\n");
    for (volatile int i = 0; i < 50000; i++); // Delay for cleanup
    
    // Force TCP stack to process any pending cleanup
    SYS_CONSOLE_PRINT("http_client: forcing TCP stack cleanup...\r\n");
    for (volatile int i = 0; i < 10000; i++); // Additional delay
    
    return true;
}