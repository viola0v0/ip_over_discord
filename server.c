// server.c - Discord TUN server
// 职责：服务端主程序，逻辑与 client.c 对称，只不过读写的是另一端的 TUN 设备

// 线程部分也是 “Discord → TUN”

// 主循环是 “TUN → Discord”

// 两端只要在同一 channel，用同一 bot token，就能互相隧道通信。

extern pthread_mutex_t curl_mutex;  // 在 discord.c 中已经初始化

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/if_tun.h>  // IFF_TUN, IFF_NO_PI
#include "tun.h"
#include "discord.h"
#include "base64.h"  // for IFF_TUN, IFF_NO_PI

#define MTU 1500
#define POLL_INTERVAL 5  // seconds

// Poll Discord channel for incoming messages and write to TUN
void *poll_loop(void *arg) {
    int tun_fd = *(int *)arg;
    while (1) {
        discord_message_t *msgs = NULL;
        // Thread-safe receive
        pthread_mutex_lock(&curl_mutex);
        int count = discord_receive(&msgs);
        pthread_mutex_unlock(&curl_mutex);

        if (count <= 0) {
            free(msgs);
            sleep(POLL_INTERVAL);
            continue;
        }

        for (int i = 0; i < count; i++) {
            size_t out_len = 0;
            unsigned char *packet = discord_base64_decode(msgs[i].content, &out_len);
            if (packet && out_len > 0) {
                tun_write(tun_fd, packet, out_len);
            }
            // Delete and free
            pthread_mutex_lock(&curl_mutex);
            discord_delete(msgs[i].id);
            pthread_mutex_unlock(&curl_mutex);
            free(msgs[i].id);
            free(msgs[i].content);
            free(packet);
        }
        free(msgs);
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
    strncpy(tun_name, argv[3], IFNAMSIZ - 1);
    tun_name[IFNAMSIZ - 1] = '\0';

    // Initialize Discord API
    if (discord_init(token, channel_id) != 0) {
        fprintf(stderr, "[service] Failed to initialize Discord API.\n");
        exit(EXIT_FAILURE);
    }

    // Allocate TUN device
    int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        fprintf(stderr, "[service] Error opening TUN device '%s'\n", tun_name);
        exit(EXIT_FAILURE);
    }
    printf("[service] TUN device %s opened, fd=%d\n", tun_name, tun_fd);

    // Start polling thread (Discord -> TUN)
    pthread_t tid;
    if (pthread_create(&tid, NULL, poll_loop, &tun_fd) != 0) {
        perror("pthread_create");
        close(tun_fd);
        exit(EXIT_FAILURE);
    }

    // Main loop: read from TUN and send to Discord (TUN -> Discord)
    while (1) {
        unsigned char buffer[MTU];
        int nread = tun_read(tun_fd, buffer, sizeof(buffer));
        if (nread < 0) {
            perror("tun_read");
            break;
        }
        size_t enc_len;
        char *b64 = base64_encode(buffer, nread, &enc_len);
        if (!b64) {
            fprintf(stderr, "[service] Base64 encoding failed\n");
            sleep(POLL_INTERVAL);
            continue;
        }
        // Thread-safe send
        pthread_mutex_lock(&curl_mutex);
        if (discord_send(b64) != 0) {
            fprintf(stderr, "[service] Failed to send message to Discord\n");
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
