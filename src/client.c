// src/client.c
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
#include "http_client.h"
#include "http_server.h"

#define BUF_MAX 65536
static uint8_t buf[BUF_MAX];

void *injector_from_server(void *arg)
{
    int tun_fd = *(int *)arg;
    while (1)
    {
        size_t len = 0;
        char *p = http_client_poll(&len); // 拉 Server /poll
        if (len > 0)
        {
            tun_write(tun_fd, (uint8_t *)p, len);
            free(p);
        }
        usleep(10000);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server-ip> <server-port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%s", argv[1], argv[2]);

    char tun_name[IF_NAMESIZE] = "tun0";
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0)
    {
        perror("tun_alloc");
        return EXIT_FAILURE;
    }

    http_server_start("9000"); // 启动 Client HTTP 服务

    if (http_client_init(url) != 0)
    {
        return EXIT_FAILURE;
    }

    pthread_t thr;
    pthread_create(&thr, NULL, injector_from_server, &tun_fd);

    while (1)
    {
        ssize_t n = tun_read(tun_fd, buf, BUF_MAX);
        if (n > 0)
        {
            http_client_send(buf, n);
        }
        http_client_heartbeat();
    }

    http_client_cleanup();
    http_server_stop();
    return EXIT_SUCCESS;
}
