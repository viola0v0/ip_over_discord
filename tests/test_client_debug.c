// tests/test_client_debug.c
#include <stdlib.h>
#include <net/if.h>
#include <net/if.h>
#include "tun.h"
#include "http_client.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <linux/if_tun.h>
#include <time.h>

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
        printf("[reader] tun_read returned %zd\n", n);
        if (n == 20) {
            if (memcmp(buf, packet, 20) == 0) printf("[reader] PASS\n");
            else printf("[reader] FAIL: mismatch\n");
            done = 1;
        }
        usleep(100000);
    }
    return NULL;
}

void *writer(void *_) {
    sleep(1);
    printf("[writer] writing to tun0 and http_client_send\n");
    ssize_t w = tun_write(tun_fd, packet, 20);
    printf("[writer] tun_write wrote %zd\n", w);
    int r = http_client_send(packet, 20);
    printf("[writer] http_client_send returned %d\n", r);
    return NULL;
}

int main(){
    char dev[IF_NAMESIZE] = "tun0";
    tun_fd = tun_alloc(dev, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        perror("tun_alloc");
        return 1;
    }
    printf("[main] allocated %s (fd=%d)\n", dev, tun_fd);

    printf("[main] http_client_init -> http://192.168.100.2:8000\n");
    if (http_client_init("http://192.168.100.2:8000") != 0) {
        fprintf(stderr, "[main] http_client_init FAILED\n");
        return 1;
    }

    pthread_t rt, wt;
    pthread_create(&rt, NULL, reader, NULL);
    pthread_create(&wt, NULL, writer, NULL);

    while (!done) {
        printf("[main] about to http_client_poll()\n");
        size_t len = 0;
        char *p = http_client_poll(&len);
        printf("[main] http_client_poll returned len=%zu ptr=%p\n", len, (void*)p);
        if (p) {
            printf("[main] writing %zu bytes back into tun0\n", len);
            tun_write(tun_fd, (uint8_t*)p, len);
            free(p);
        }
        sleep(1);
    }

    pthread_join(rt, NULL);
    pthread_join(wt, NULL);
    http_client_cleanup();
    close(tun_fd);
    return 0;
}
