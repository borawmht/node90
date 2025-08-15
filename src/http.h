/*
* http.h
* created by: Brad Oraw
* created on: 2025-08-15
*/

#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdbool.h>

void http_init(void);
void http_server_init(void);
bool http_client_get_url(const char *url, const char *data, size_t data_size);

#endif // HTTP_H