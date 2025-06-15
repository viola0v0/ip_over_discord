//  /include/tun.h
#ifndef TUN_H
#define TUN_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int tun_alloc(char *dev, int flags);

ssize_t tun_read(int fd, uint8_t *buf, size_t bufsiz);

ssize_t tun_write(int fd, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // TUN_H