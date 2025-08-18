/*
* http.h
* created by: Brad Oraw
* created on: 2025-08-15
*/

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "definitions.h"

#define REQUEST_SIZE 512
extern char request[REQUEST_SIZE];

#define RESPONSE_SIZE 2048
extern uint8_t response_buffer[RESPONSE_SIZE];

void http_init(void);
void http_server_init(void);
bool http_client_get_url(const char *url, const char *data, size_t data_size);
bool https_client_get_url(const char *url, const char *data, size_t data_size);

// Helper functions for URL parsing and network operations
bool parse_url(const char *url, char *hostname, uint16_t *port, char *path, bool *is_https);
bool resolve_hostname(const char *hostname, IPV4_ADDR *ip_addr);
bool parse_http_response_status(const char *response, int *status_code);

#endif // HTTP_H