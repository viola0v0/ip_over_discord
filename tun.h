// tun.h - Header file for TUN device operations
#ifndef TUN_H
#define TUN_H

#include <stddef.h>
#include <sys/types.h>

/**
 * 在 /dev/net/tun 上分配并打开一个 TUN 设备
 *
 * @param dev    想要创建的设备名（可写缓冲区，长度 IFNAMSIZ）；
 *               调用后返回内核实际分配的名称
 * @param flags  IFF_TUN 或 IFF_TAP，可以 OR 上 IFF_NO_PI
 * @return       成功时返回文件描述符；失败返回 -1 并设置 errno
 */
int tun_alloc(char *dev, int flags);

/**
 * 从 TUN 设备读取一个原始 IP 包
 *
 * @param fd     tun_alloc 返回的文件描述符
 * @param buf    用户缓冲区，至少要大于最大 MTU（推荐 1500+）
 * @param len    缓冲区大小
 * @return       读取到的字节数，若出错返回 -1
 */
ssize_t tun_read(int fd, void *buf, size_t len);

/**
 * 向 TUN 设备写入一个原始 IP 包
 *
 * @param fd     tun_alloc 返回的文件描述符
 * @param buf    包数据缓冲区
 * @param len    要写入的字节数
 * @return       写入的字节数，若出错返回 -1
 */
ssize_t tun_write(int fd, const void *buf, size_t len);

#endif /* TUN_H */