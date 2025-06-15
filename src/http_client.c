// src/http_client.c
#include "http_client.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

static pthread_mutex_t curl_mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_CURL() pthread_mutex_lock(&curl_mutex)
#define UNLOCK_CURL() pthread_mutex_unlock(&curl_mutex)

#define SEND_PATH "/send"
#define POLL_PATH "/poll"
#define MAX_PKT 2048
#define HEARTBEAT_INTERVAL_SEC 30

// 两个独立的 easy handle
static CURL *curl_send_handle = NULL;
static CURL *curl_poll_handle = NULL;

static struct curl_slist *send_headers = NULL;
static struct curl_slist *poll_headers = NULL;
// 对应两个 base URL 和完全路径
static char *g_send_base = NULL, *g_send_url  = NULL;
static char *g_poll_base = NULL, *g_poll_url  = NULL;
static time_t last_hb = 0;

struct PollCtx
{
    uint8_t *buf;
    size_t maxlen;
    size_t len;
};

static size_t recv_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t real = size * nmemb;
    struct PollCtx *ctx = userdata;
    if (ctx->len + real > ctx->maxlen)
        return 0;
    memcpy(ctx->buf + ctx->len, ptr, real);
    ctx->len += real;
    return real;
}

int http_client_init_send(const char *base_url)
{
    printf("[H-CLT] init_send → %s/send\n", base_url);
    fflush(stdout);
    free(g_send_base);
    free(g_send_url);
    g_send_base = strdup(base_url);
    size_t L = strlen(g_send_base) + strlen(SEND_PATH) + 1;
    g_send_url = malloc(L);
    snprintf(g_send_url, L, "%s" SEND_PATH, g_send_base);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_send_handle = curl_easy_init();
    if (!curl_send_handle)
        return -1;
    // 初始化 send handle 的 header
    send_headers = curl_slist_append(NULL, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl_send_handle, CURLOPT_HTTPHEADER, send_headers);
    curl_easy_setopt(curl_send_handle, CURLOPT_TCP_KEEPALIVE, 1L);
    return 0;
}

// 初始化用于“拉包”的 handle
int http_client_init_poll(const char *base_url)
{
    printf("[H-CLT] init_poll → %s/poll\n", base_url);
    fflush(stdout);
    free(g_poll_base);
    free(g_poll_url);
    g_poll_base = strdup(base_url);
    size_t L = strlen(g_poll_base) + strlen(POLL_PATH) + 1;
    g_poll_url = malloc(L);
    snprintf(g_poll_url, L, "%s" POLL_PATH, g_poll_base);

    curl_poll_handle = curl_easy_init();
    if (!curl_poll_handle)
        return -1;
    // 初始化 poll handle 的 header
    poll_headers = curl_slist_append(NULL, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl_poll_handle, CURLOPT_HTTPHEADER, poll_headers);
    curl_easy_setopt(curl_poll_handle, CURLOPT_TIMEOUT_MS, 500L);
    return 0;
}

char *http_client_poll(size_t *buflen)
{
    struct PollCtx ctx = {malloc(MAX_PKT), MAX_PKT, 0};

    // 串行化整个 reset + perform 流程
    LOCK_CURL();
    curl_easy_reset(curl_poll_handle);
    curl_easy_setopt(curl_poll_handle, CURLOPT_URL, g_poll_url);
    curl_easy_setopt(curl_poll_handle, CURLOPT_WRITEFUNCTION, recv_cb);
    curl_easy_setopt(curl_poll_handle, CURLOPT_WRITEDATA, &ctx);
    CURLcode res = curl_easy_perform(curl_poll_handle);
    UNLOCK_CURL();

    if (res != CURLE_OK || ctx.len == 0)
    {
        free(ctx.buf);
        *buflen = 0;
        return NULL;
    }
    *buflen = ctx.len;
    return (char *)ctx.buf;
}

// 发包也要串行化 Reset → Setopts → Perform
int http_client_send(const uint8_t *data, size_t len)
{
    LOCK_CURL();
    curl_easy_reset(curl_send_handle);
    curl_easy_setopt(curl_send_handle, CURLOPT_URL, g_send_url);
    curl_easy_setopt(curl_send_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_send_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl_send_handle, CURLOPT_POSTFIELDSIZE, (long)len);
    CURLcode r = curl_easy_perform(curl_send_handle);
    UNLOCK_CURL();
    return (r == CURLE_OK) ? 0 : -1;
}

int http_client_heartbeat(void)
{
    time_t now = time(NULL);
    if (now - last_hb < HEARTBEAT_INTERVAL_SEC)
        return 0;
    int r = http_client_send(NULL, 0);
    if (r == 0)
        last_hb = now;
    return r;
}

void http_client_cleanup(void)
{
    printf("[H-CLT] cleanup curl handles\n");
    fflush(stdout);
    if (curl_send_handle)
    {
        curl_easy_cleanup(curl_send_handle);
        curl_send_handle = NULL;
    }
    if (curl_poll_handle)
    {
        curl_easy_cleanup(curl_poll_handle);
        curl_poll_handle = NULL;
    }
    curl_global_cleanup();
    if (send_headers) curl_slist_free_all(send_headers);
    if (poll_headers) curl_slist_free_all(poll_headers);
    free(g_send_base);
    free(g_send_url);
    free(g_poll_base);
    free(g_poll_url);
}
