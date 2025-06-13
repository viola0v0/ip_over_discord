// client.c - Discord TUN Client
// 职责：客户端主程序，把本地 TUN ↔ Discord 隧道化

// 初始化：

// discord_init()

// tun_alloc()

// 线程 poll_loop：

// 周期性调用 discord_receive()

// discord_base64_decode() → tun_write()

// discord_delete()

// 主循环：

// tun_read()

// base64_encode()

// discord_send()

// 在 discord.c 中定义的全局互斥锁，保证 libcurl 调用线程安全
extern pthread_mutex_t curl_mutex;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#ifdef _WIN32
  // Windows 下改用 Wintun/OpenVPN TAP 等
  #include "wintun.h"    // 或者 packet32.h 等
#else
  #include <linux/if_tun.h>
#endif
#include "tun.h"
#include "discord.h"
#include "base64.h"

#define MTU 1500
#define POLL_INTERVAL 5  // seconds

// Poll Discord channel for incoming IP packets
void *poll_loop(void *arg) {
    int tun_fd = *(int *)arg;
    while (1) {
        // Fetch messages from Discord
        discord_message_t *msgs = NULL;

        // 线程安全地拉取消息
        pthread_mutex_lock(&curl_mutex);
        int count = discord_receive(&msgs);
        pthread_mutex_unlock(&curl_mutex);

        if (count > 0 && msgs) {
            for (int i = 0; i < count; i++) {
                size_t out_len = 0;
                unsigned char *packet = discord_base64_decode(msgs[i].content, &out_len);
                if (packet && out_len > 0) {
                    tun_write(tun_fd, packet, out_len);
                }
                // 删除消息并释放资源
                pthread_mutex_lock(&curl_mutex);
                discord_delete(msgs[i].id);
                pthread_mutex_unlock(&curl_mutex);
                free(msgs[i].id);
                free(msgs[i].content);
                free(packet);
            }
            free(msgs);
        }

        sleep(POLL_INTERVAL);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <bot_token> <channel_id> <tun_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *token      = argv[1];
    const char *channel_id = argv[2];
    char tun_name[IFNAMSIZ];

    // 复制用户传入的设备名，确保以 '\0' 结尾
    strncpy(tun_name, argv[3], IFNAMSIZ - 1);
    tun_name[IFNAMSIZ - 1] = '\0';

    // Initialize Discord REST & polling
    if (discord_init(token, channel_id) != 0) {
        fprintf(stderr, "Failed to initialize Discord API.\n");
        exit(EXIT_FAILURE);
    }

    // 在用户态分配 TUN 设备，带上 IFF_NO_PI 去掉额外的包信息头
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        fprintf(stderr, "[client] Error opening TUN device '%s'\n", tun_name);
        exit(EXIT_FAILURE);
    }
    printf("[client] TUN device %s opened, fd=%d\n", tun_name, tun_fd);

    // Start polling thread
    pthread_t tid;
    if (pthread_create(&tid, NULL, poll_loop, &tun_fd) != 0) {
        perror("[client] pthread_create");
        close(tun_fd);
        exit(EXIT_FAILURE);
    }

    // Main loop: read from TUN and send to Discord
    while (1) {
        unsigned char buffer[MTU];
        int nread = tun_read(tun_fd, buffer, sizeof(buffer));
        if (nread < 0) {
            perror("tun_read");
            break;
        }
        // Encode packet to Base64
        size_t enc_len;
        char *b64 = base64_encode(buffer, nread, &enc_len);
        if (!b64) {
            fprintfprintf(stderr, "[client] Base64 encoding failed\n");
            sleep(POLL_INTERVAL);
            continue;
        }

        // 线程安全地发送消息
        pthread_mutex_lock(&curl_mutex);
        if (discord_send(b64) != 0) {
            fprintf(stderr, "[client] Failed to send message to Discord\n");
        }
        pthread_mutex_unlock(&curl_mutex);

        free(b64);
        sleep(POLL_INTERVAL);
    }

    // Cleanup
    pthread_cancel(tid);
    pthread_join(tid, NULL);
    close(tun_fd);
    discord_cleanup();

    return 0;
}
