//  /src/tun.c
#include "tun.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tun_alloc(char *dev, int flags)
{
    struct ifreq ifr;
    int fd, err;

    // 打开 TUN 设备节点
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    {
        perror("open /dev/net/tun");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags; // IFF_TUN 或 IFF_TAP + 可选 IFF_NO_PI
    if (*dev)
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    // ioctl 创建/绑定设备
    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
    {
        perror("ioctl TUNSETIFF");
        close(fd);
        return -1;
    }

    // 返回实际分配的设备名称
    strcpy(dev, ifr.ifr_name);
    // —— 插桩：tun_alloc 成功
    printf("[TUN] allocated %s (fd=%d)\n", dev, fd);
    fflush(stdout);
    return fd;
}

ssize_t tun_read(int fd, uint8_t *buf, size_t bufsiz)
{
    ssize_t nread;
    nread = read(fd, buf, bufsiz);
    if (nread < 0)
    {
        perror("tun_read");
    }
    return nread;
}

ssize_t tun_write(int fd, const uint8_t *buf, size_t len)
{
    ssize_t nwrite;
    nwrite = write(fd, buf, len);
    if (nwrite < 0)
    {
        perror("tun_write");
    }
    return nwrite;
}