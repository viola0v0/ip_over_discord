// tun_test.c — TUN 模块功能验证
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "tun.h"

#define BUF_SIZE 2000

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <tun_name>\n", argv[0]);
        return 1;
    }
    char if_name[IFNAMSIZ] = {0};
    strncpy(if_name, argv[1], IFNAMSIZ-1);

    // 1) 创建 TUN 设备（无包头信息）
    int tun_fd = tun_alloc(if_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        perror("tun_alloc");
        return 1;
    }
    printf("✔  已分配 TUN 设备：%s (fd=%d)\n\n", if_name, tun_fd);

    // 2) 提示用户配置
    printf("→ 请在另一个终端执行：\n");
    printf("    sudo ip addr add 10.0.0.1/24 dev %s\n", if_name);
    printf("    sudo ip link set dev %s up\n\n", if_name);
    printf("然后从任意终端 ping 10.0.0.1，程序将捕获第一个 ICMP 包并打印。\n\n");

    // 3) 阻塞读取
    unsigned char buf[BUF_SIZE];
    ssize_t n = tun_read(tun_fd, buf, sizeof(buf));
    if (n < 0) {
        perror("tun_read");
        close(tun_fd);
        return 1;
    }

    // 4) 打印包内容
    printf("← 从 TUN 读到 %zd 字节：\n", n);
    for (ssize_t i = 0; i < n; i++) {
        printf("%02x ", buf[i]);
        if ((i+1) % 16 == 0) putchar('\n');
    }
    putchar('\n');

    close(tun_fd);
    return 0;
}
