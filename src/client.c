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

// 从 Client 本地 HTTP Server 的 /poll 拉回包，然后注入到 tun0
void *injector_from_server(void *arg)
{
    int tun_fd = *(int *)arg;
    while (1)
    {
        size_t len = 0;
        char *pkt = http_client_poll(&len);  // ← 现在它会 GET http://127.0.0.1:port/poll
        if (pkt && len > 0)
        {
            printf("[C] got %zu bytes from poll, writing to tun\n", len);
            fflush(stdout);
            tun_write(tun_fd, (uint8_t *)pkt, len);
            free(pkt);
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

    // ① 发包目标 → Server
    char server_url[256];
    snprintf(server_url, sizeof(server_url), "http://%s:%s", argv[1], argv[2]);

    // ② 拉包目标 → 自己的 HTTP Server
    char client_url[256];
    snprintf(client_url, sizeof(client_url), "http://127.0.0.1:%s", argv[2]);  // ← 修改

    printf("[C] main begins: server_url=%s, client_url=%s\n", server_url, client_url);
    fflush(stdout);

    // 1) 创建并打开 tun0
    char tun_name[IF_NAMESIZE] = "tun0";
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    printf("[C] tun_alloc -> %s fd=%d\n", tun_name, tun_fd);
    fflush(stdout);
    if (tun_fd < 0)
    {
        perror("tun_alloc");
        return EXIT_FAILURE;
    }

    // 2) 启动 Client 本地 HTTP Server，用于接收 Server 发来的 /send
    if (http_server_start(argv[2]) != 0)
    {
        fprintf(stderr, "Failed to start client HTTP server on port %s\n", argv[2]);
        return EXIT_FAILURE;
    }
    printf("[C] HTTP server up on %s\n", argv[2]);
    fflush(stdout);

    // 3) 初始化 send-handle：POST → Server 的 /send
    if (http_client_init_send(server_url) != 0)
    {
        fprintf(stderr, "http_client_init_send(%s) failed\n", server_url);
        return EXIT_FAILURE;
    }
    printf("[C] send-handle ready\n");
    fflush(stdout);

    // 4) 初始化 poll-handle：GET ← Client 本地 HTTP Server 的 /poll
    if (http_client_init_poll(client_url) != 0)  // ← 修改
    {
        fprintf(stderr, "http_client_init_poll(%s) failed\n", client_url);
        return EXIT_FAILURE;
    }
    printf("[C] poll-handle ready\n");
    fflush(stdout);

    // 5) 启动拉包线程：HTTP poll -> tun0
    pthread_t poll_thr;
    if (pthread_create(&poll_thr, NULL, injector_from_server, &tun_fd) != 0)
    {
        perror("pthread_create injector_from_server");
        return EXIT_FAILURE;
    }
    printf("[C] injector_from_server thread spawned\n");
    fflush(stdout);

    // 6) 主循环：从 tun0 读包，POST 到 Server 的 /send
    while (1)
    {
        ssize_t n = tun_read(tun_fd, buf, BUF_MAX);
        if (n > 0)
        {
            printf("[C] read %zd bytes from tun, sending\n", n);
            fflush(stdout);
            http_client_send(buf, n);
        }
        http_client_heartbeat();  // 心跳保持连接
    }

    // 理论上不会走到这里
    http_client_cleanup();
    http_server_stop();
    close(tun_fd);
    return EXIT_SUCCESS;
}
