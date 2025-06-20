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

// Packet destination → Client
void *reader_to_client(void *arg)
{
    int tun_fd = *(int *)arg;
    uint8_t buf[65536];
    while (1)
    {
        ssize_t n = tun_read(tun_fd, buf, sizeof(buf));
        if (n > 0)
        {
            http_client_send(buf, n); // 发往 Client 的 /send
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <port> <client-ip> <client-port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *port = argv[1];
    const char *client_ip = argv[2];
    const char *client_port = argv[3];

    printf("[S] main begins: server_port=%s, client=%s:%s\n", port, client_ip, client_port);
    fflush(stdout);

    // create and open tun0
    char tun_name[IF_NAMESIZE] = "tun0";
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    printf("[S] tun0 fd=%d\n", tun_fd);
    fflush(stdout);
    if (tun_fd < 0)
    {
        perror("tun_alloc");
        return EXIT_FAILURE;
    }

    // add route
    injector_add_route("10.0.100.0/24", tun_name);

    // start the local HTTP server to receive /send requests from the Client
    if (http_server_start(port) != 0)
    {
        fprintf(stderr, "Failed to start HTTP server on port %s\n", port);
        return EXIT_FAILURE;
    }
    printf("[S] HTTP server up on %s\n", port);
    fflush(stdout);

    // Construct two base URLs:
    //    client_url is used to send packets (/send) to the Client
    //    server_url is used to poll packets (/poll) from itself
    char client_url[64];
    snprintf(client_url, sizeof(client_url), "http://%s:%s", client_ip, client_port);
    char server_url[64];
    snprintf(server_url, sizeof(server_url), "http://127.0.0.1:%s", port);

    // Initialize send-handle (for sending packets to Client)
    if (http_client_init_send(client_url) != 0)
    {
        fprintf(stderr, "http_client_init_send(%s) failed\n", client_url);
        return EXIT_FAILURE;
    }
    printf("[S] send-handle ready\n");
    fflush(stdout);

    // Initialize poll-handle (poll packets from its own /poll)
    if (http_client_init_poll(server_url) != 0)
    {
        fprintf(stderr, "http_client_init_poll(%s) failed\n", server_url);
        return EXIT_FAILURE;
    }
    printf("[S] poll-handle ready\n");
    fflush(stdout);

    // Start the packet sending thread: tun0 -> HTTP send -> Client
    pthread_t send_thr;
    if (pthread_create(&send_thr, NULL, reader_to_client, &tun_fd) != 0)
    {
        perror("pthread_create reader_to_client");
        return EXIT_FAILURE;
    }
    printf("[S] reader_to_client thread spawned\n");
    fflush(stdout);

    // Start the packet polling thread: HTTP poll -> tun0
    //    injector_start will spawn a thread to run injector_loop()
    injector_start(tun_fd);
    printf("[S] injector thread spawned\n");
    fflush(stdout);

    // Wait for user to press Enter, then clean up
    getchar();

    injector_stop();
    http_server_stop();
    http_client_cleanup();
    close(tun_fd);
    return EXIT_SUCCESS;
}
