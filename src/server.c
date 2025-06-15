// src/server.c
#include <linux/if_tun.h>
#include <net/if.h>
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include "tun.h"
#include "injector.h"
#include "http_server.h"
#include "http_client.h"

void *reader_to_client(void *arg)
{
    int tun_fd = *(int *)arg;
    uint8_t buf[65536];
    while (1)
    {
        ssize_t n = tun_read(tun_fd, buf, sizeof(buf));
        if (n > 0)
        {
            http_client_send(buf, n); // 发往 Client /send
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <client-port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *port = argv[1];
    const char *client_port = argv[2];

    char tun_name[IF_NAMESIZE] = "tun0";
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0)
    {
        perror("tun_alloc");
        return EXIT_FAILURE;
    }

    injector_add_route("10.0.100.0/24", tun_name); // 路由 Client 子网

    http_server_start(port); // Server HTTP 服务

    char client_url[64];
    snprintf(client_url, sizeof(client_url), "http://127.0.0.1:%s", client_port);
    http_client_init(client_url); // HTTP 客户端指向 Client

    pthread_t thr;
    pthread_create(&thr, NULL, reader_to_client, &tun_fd);

    injector_start(tun_fd, "127.0.0.1", atoi(port)); // 注入 Client 发来的包

    getchar();

    injector_stop();
    http_server_stop();
    http_client_cleanup();
    close(tun_fd);
    return EXIT_SUCCESS;
}
