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
    printf("[INJ] injector_loop running\n");
    fflush(stdout);

    while (injector_running)
    {
        size_t pkt_len = 0;
        char *pkt = http_client_poll(&pkt_len);
        if (pkt_len > 0 && pkt)
        {
            printf("[INJ] got %zu bytes from HTTP\n", pkt_len);
            fflush(stdout);

            ssize_t w = write(g_tun_fd, pkt, pkt_len);
            if (w < 0)
            {
                perror("[INJ] write to tun0 failed");
            }
            else
            {
                printf("[INJ] wrote %zd bytes to tun0\n", w);
            }
            fflush(stdout);

            free(pkt);
        }
        else
        {
            usleep(10000);
        }
    }
    return NULL;
}

int injector_start(int tun_fd)
{
    g_tun_fd = tun_fd;
    printf("[INJ] start injector on tun fd=%d\n", tun_fd);
    fflush(stdout);
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
