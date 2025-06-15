#include "http_server.h"
#include "http_client.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(){
    http_server_start("8888");
    sleep(1);
    http_client_init("http://127.0.0.1:8888");
    const char msg[] = "hello world";
    http_client_send((const uint8_t*)msg, strlen(msg));
    size_t len = 0;
    char *resp = http_client_poll(&len);
    if(resp){
        printf("recv %zu: %.*s\n", len, (int)len, resp);
        free(resp);
    } else {
        printf("no data\n");
    }
    http_client_cleanup();
    http_server_stop();
    return 0;
}
