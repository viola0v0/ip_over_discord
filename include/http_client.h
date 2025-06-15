// /include/http_client.h
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdint.h>

// 初始化用于发包的 HTTP 客户端（POST /send）
int http_client_init_send(const char *server_url);

// 初始化用于拉包的 HTTP 客户端（GET  /poll）
int http_client_init_poll(const char *server_url);


char  *http_client_poll(size_t *buflen);
int    http_client_send(const uint8_t *data, size_t len);

// 心跳 & cleanup 也可复用
int    http_client_heartbeat(void);
void   http_client_cleanup(void);

#endif
