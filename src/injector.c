// src/injector.c
#include "injector.h"
#include "http_client.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static volatile int injector_running = 0;
static pthread_t injector_thread;
static int g_tun_fd;

static void *injector_loop(void *arg)
{
    while (injector_running)
    {
        size_t pkt_len = 0;
        char *pkt = http_client_poll(&pkt_len);
        if (pkt_len > 0 && pkt)
        {
            printf("[injector] got %zu bytes from HTTP\n", pkt_len);
            write(g_tun_fd, pkt, pkt_len);
            free(pkt);
        }
        else
        {
            usleep(10000);
        }
    }
    return NULL;
}

int injector_start(int tun_fd, const char *server_host, int server_port)
{
    g_tun_fd = tun_fd;
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d", server_host, server_port);
    http_client_init(url);
    injector_running = 1;
    pthread_create(&injector_thread, NULL, injector_loop, NULL);
    return 0;
}

void injector_stop(void)
{
    injector_running = 0;
    pthread_join(injector_thread, NULL);
    http_client_cleanup();
}

int injector_add_route(const char *subnet_cidr, const char *tun_name)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip route replace %s dev %s", subnet_cidr, tun_name);
    (void)system(cmd); // 忽略返回值——总之要写上去
    return 0;
}
