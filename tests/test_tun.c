// /tests/test_tun.c
#include <net/if.h>
#include <linux/if_tun.h>
#include "tun.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(){
    char dev[IF_NAMESIZE] = "tun0";
    int fd = tun_alloc(dev, IFF_TUN | IFF_NO_PI);
    if(fd < 0){
        perror("tun_alloc");
        return 1;
    }
    printf("Allocated %s (fd=%d)\n", dev, fd);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    char buf[100];
    ssize_t n = tun_read(fd, buf, sizeof(buf));
    if(n < 0 && errno == EAGAIN){
        printf("tun_read non-blocking ok\n");
    } else if(n < 0){
        perror("tun_read");
    } else {
        printf("tun_read returned %zd bytes\n", n);
    }
    unsigned char pkt[20] = {
        0x45,0x00,0x00,0x14,0x00,0x00,0x00,0x00,
        0x40,0x01,0x00,0x00,
        192,168,0,1,192,168,0,2
    };
    ssize_t w = tun_write(fd, pkt, sizeof(pkt));
    if(w < 0) perror("tun_write");
    else printf("tun_write wrote %zd bytes\n", w);
    close(fd);
    return 0;
}
