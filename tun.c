// tun.c - TUN/TAP interface implementation
// tun.h / tun.c
// 职责：在用户态创建并操作 TUN 虚拟网口

// tun_alloc()：打开 /dev/net/tun，配置成 TUN 设备并返回文件描述符

// tun_read() / tun_write()：对外暴露读写 IP 包的接口（底层就是对 fd 的 read/write）

// 注意点

// 确保在 tun_alloc() 中设置好 IFF_TUN、IFF_NO_PI 等必要标志。

// 出错时要打印足够的日志方便定位。

#include "tun.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <errno.h>
 
 /**
  * tun_alloc - allocate or connect to a tun device
  * @dev: buffer containing desired device name; on return set to actual name
  * @flags: IFF_TUN or IFF_TAP, optionally with IFF_NO_PI
  *
  * Returns file descriptor on success, -1 on error.
  */
 int tun_alloc(char *dev, int flags) {
     struct ifreq ifr;
     int fd;
 
     /* Open the clone device */
     if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        fprintf(stderr, "tun_alloc: open /dev/net/tun failed: %s\n", strerror(errno));
        return -1;
     }
 
     memset(&ifr, 0, sizeof(ifr));
     /* Always disable packet info header */
    ifr.ifr_flags = flags | IFF_NO_PI;
 
    /* If a device name was specified, copy it in; otherwise, kernel picks one */
    if (dev && *dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    }
 
     /* Create the device */
     if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        fprintf(stderr, "tun_alloc: ioctl TUNSETIFF failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
 
     /* Copy back the actual name (in case kernel chose one) */
     strcpy(dev, ifr.ifr_name);
     return fd;
 }
 
 /**
  * tun_read - read an IP packet from the tun interface
  * @fd: file descriptor of the tun device
  * @buf: buffer to read into
  * @buflen: maximum buffer length
  *
  * Returns number of bytes read, or -1 on error.
  */
 ssize_t tun_read(int fd, void *buf, size_t len) {
    ssize_t nread = read(fd, buf, len);
    if (nread < 0) {
        fprintf(stderr, "tun_read: read failed: %s\n", strerror(errno));
    }
    return nread;
}

 
 /**
  * tun_write - write an IP packet to the tun interface
  * @fd: file descriptor of the tun device
  * @buf: buffer containing packet data
  * @len: number of bytes to write
  *
  * Returns number of bytes written, or -1 on error.
  */
 ssize_t tun_write(int fd, const void *buf, size_t len) {
    ssize_t nwrite = write(fd, buf, len);
    if (nwrite < 0) {
        fprintf(stderr, "tun_write: write failed: %s\n", strerror(errno));
    }
    return nwrite;
}

 