// /include/http_client.h
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdint.h>

int    http_client_init(const char *server_url);
char  *http_client_poll(size_t *buflen);
int    http_client_send(const uint8_t *data, size_t len);
int    http_client_heartbeat(void);
void   http_client_cleanup(void);

#endif
