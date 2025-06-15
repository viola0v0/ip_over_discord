// tests/test_client.c
#include <stdlib.h>
#include <net/if.h>
#include "tun.h"
#include "http_client.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <linux/if_tun.h>

static int tun_fd;
static volatile int done = 0;
static const uint8_t packet[20] = {
    0x45,0x00,0x00,0x14,0x00,0x00,0x00,0x00,
    0x40,0x01,0x66,0xe7,10,0,0,1,10,0,0,2
};

void *reader(void *_) {
    uint8_t buf[1500];
    while (!done) {
        ssize_t n = tun_read(tun_fd, buf, sizeof(buf));
        if (n == 20) {
            if (memcmp(buf, packet, 20) == 0) printf("PASS\n");
            else printf("FAIL: mismatch\n");
            done = 1;
            break;
        }
    }
    return NULL;
}

void *writer(void *_) {
    sleep(1);
    printf("[client] writing to tun0 and sending via HTTP\n");
    tun_write(tun_fd, packet, 20);
    if (http_client_send(packet, 20) == 0)
        printf("[client] http_client_send OK\n");
    else
        printf("[client] http_client_send FAILED\n");
    return NULL;
}

int main(){
    char dev[IF_NAMESIZE] = "tun0";
    tun_fd = tun_alloc(dev, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) return 1;

    http_client_init("http://192.168.100.2:8000");

    pthread_t rt, wt;
    pthread_create(&rt, NULL, reader, NULL);
    pthread_create(&wt, NULL, writer, NULL);

    while (!done) {
        size_t len = 0;
        char *p = http_client_poll(&len);
        if (p) {
            printf("[client] http_client_poll got %zu bytes\n", len);
            tun_write(tun_fd, (uint8_t*)p, len);
            free(p);
        }
        usleep(100000);
    }

    pthread_join(rt, NULL);
    http_client_cleanup();
    close(tun_fd);
    return 0;
}
