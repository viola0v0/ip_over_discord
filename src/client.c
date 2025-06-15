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
        char *pkt = http_client_poll(&len);
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

    // Packet destination → Server
    char server_url[256];
    snprintf(server_url, sizeof(server_url), "http://%s:%s", argv[1], argv[2]);

    // Packet polling target → local HTTP Server
    char client_url[256];
    snprintf(client_url, sizeof(client_url), "http://127.0.0.1:%s", argv[2]);  // ← 修改

    printf("[C] main begins: server_url=%s, client_url=%s\n", server_url, client_url);
    fflush(stdout);

    // Create and open tun0
    char tun_name[IF_NAMESIZE] = "tun0";
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    printf("[C] tun_alloc -> %s fd=%d\n", tun_name, tun_fd);
    fflush(stdout);
    if (tun_fd < 0)
    {
        perror("tun_alloc");
        return EXIT_FAILURE;
    }

    // Start the local client HTTP server to receive /send requests from the server
    if (http_server_start(argv[2]) != 0)
    {
        fprintf(stderr, "Failed to start client HTTP server on port %s\n", argv[2]);
        return EXIT_FAILURE;
    }
    printf("[C] HTTP server up on %s\n", argv[2]);
    fflush(stdout);

    // Initialize send-handle: POST to server's /send endpoint
    if (http_client_init_send(server_url) != 0)
    {
        fprintf(stderr, "http_client_init_send(%s) failed\n", server_url);
        return EXIT_FAILURE;
    }
    printf("[C] send-handle ready\n");
    fflush(stdout);

    // Initialize poll-handle: GET from the client's local HTTP server /poll endpoint
    if (http_client_init_poll(client_url) != 0)
    {
        fprintf(stderr, "http_client_init_poll(%s) failed\n", client_url);
        return EXIT_FAILURE;
    }
    printf("[C] poll-handle ready\n");
    fflush(stdout);

    // Start the packet pulling thread: HTTP poll -> tun0
    pthread_t poll_thr;
    if (pthread_create(&poll_thr, NULL, injector_from_server, &tun_fd) != 0)
    {
        perror("pthread_create injector_from_server");
        return EXIT_FAILURE;
    }
    printf("[C] injector_from_server thread spawned\n");
    fflush(stdout);

    // Main loop: read packets from tun0 and POST to the server's /send endpoint
    while (1)
    {
        ssize_t n = tun_read(tun_fd, buf, BUF_MAX);
        if (n > 0)
        {
            printf("[C] read %zd bytes from tun, sending\n", n);
            fflush(stdout);
            http_client_send(buf, n);
        }
        http_client_heartbeat();
    }

    http_client_cleanup();
    http_server_stop();
    close(tun_fd);
    return EXIT_SUCCESS;
}
