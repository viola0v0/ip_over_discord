//  /includehttp_server.h
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

int http_server_start(const char *port_str);

void http_server_stop(void);

#endif // HTTP_SERVER_H
