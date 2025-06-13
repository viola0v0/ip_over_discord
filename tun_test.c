// tun_test.c — TUN 模块功能验证
// 当收到第一个包时：
//   1) 打印包内容
//   2) 原样写回去
//   3) 退出

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>         // for ioctl
#include <net/if.h>            // for IFNAMSIZ
#include <linux/if_tun.h>      // for IFF_TUN, IFF_NO_PI
#include <netinet/ip.h>        // struct iphdr
#include <netinet/ip_icmp.h>   // struct icmphdr
#include <arpa/inet.h>         // ntohs, htons
#include "tun.h"

#define BUF_SIZE 2000

// 计算校验和（网络字节序的 uint16_t 数组）
static uint16_t checksum(void *buf, size_t len) {
    uint32_t sum = 0;
    uint16_t *data = buf;
    for (size_t i = 0; i + 1 < len/2*2; i++) {
        sum += ntohs(data[i]);
    }
    if (len & 1) {
        // 奇数字节
        uint8_t *b = (uint8_t*)buf;
        sum += b[len-1] << 8;
    }
    // 折叠
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return htons(~sum);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <tun_name>\n", argv[0]);
        return 1;
    }

    // 准备设备名
    char if_name[IFNAMSIZ] = {0};
    strncpy(if_name, argv[1], IFNAMSIZ-1);

    // 创建 TUN 设备（无包头信息）
    int tun_fd = tun_alloc(if_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        perror("tun_alloc");
        return 1;
    }
    printf("✔ 已分配 TUN 设备：%s (fd=%d)\n\n", if_name, tun_fd);

    // 提示如何在另一个终端配置
    printf(
        "Now in another terminal, configure and ping:\n"
        "    sudo ip addr add 10.0.0.1 peer 10.0.0.2 dev %s\n"
        "    sudo ip link set dev %s up\n"
        "Then run:\n"
        "    ping -c1 -I %s 10.0.0.2\n"
        "This program will capture the first ICMP Echo-Request,\n"
        "print it, forge a proper Echo-Reply and write it back.\n\n",
        if_name, if_name, if_name
    );

    // 阻塞读取第一个 Echo-Request
    unsigned char buf[BUF_SIZE];
    ssize_t n;
    while (1) {
        n = tun_read(tun_fd, buf, sizeof(buf));
        if (n < 0) {
            perror("tun_read");
            close(tun_fd);
            return 1;
        }
        // 判断：IPv4 + ICMP + Echo-Request
        if ((buf[0] >> 4) == 4 && buf[9] == IPPROTO_ICMP && buf[20] == ICMP_ECHO) {
            break;
        }
    }

    // 打印原始请求
    printf("← 从 TUN 读到 %zd 字节（Echo-Request）：\n", n);
    for (ssize_t i = 0; i < n; i++) {
        printf("%02x ", buf[i]);
        if ((i+1)%16 == 0) putchar('\n');
    }
    putchar('\n');

    // 伪造 Echo-Reply
    struct iphdr *ip    = (struct iphdr*) buf;
    struct icmphdr *icm = (struct icmphdr*)(buf + ip->ihl*4);

    // 1) 交换 IP 源/目的
    uint32_t tmp_ip = ip->saddr;
    ip->saddr = ip->daddr;
    ip->daddr = tmp_ip;

    // 2) 改 ICMP 类型 → Echo-Reply
    icm->type = ICMP_ECHOREPLY;
    icm->checksum = 0;

    // 3) 重算 ICMP 校验和（仅 ICMP 部分）
    size_t icmp_len = ntohs(ip->tot_len) - ip->ihl*4;
    icm->checksum = checksum(icm, icmp_len);

    // 4) 重算 IP 校验和
    ip->check = 0;
    ip->check = checksum(ip, ip->ihl*4);

    // 写回 TUN
    ssize_t w = tun_write(tun_fd, buf, ntohs(ip->tot_len));
    if (w < 0) {
        perror("tun_write");
    } else {
        printf("✔ 伪造并写回 %zd 字节 (Echo-Reply) 到 TUN\n", w);
    }

    // 给 ping 进程一点时间在 tun0 上接收 reply
    sleep(15);

    close(tun_fd);
    return 0;
}