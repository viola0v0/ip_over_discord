// src/http_client.c
#include "http_client.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <stdint.h>

#define SEND_PATH "/send"
#define POLL_PATH "/poll"
#define MAX_PKT 2048
#define HEARTBEAT_INTERVAL_SEC 30

static CURL *curl_handle = NULL;
static struct curl_slist *curl_headers = NULL;
static char *g_base_url = NULL;
static char *g_send_url = NULL;
static char *g_poll_url = NULL;
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

int http_client_init(const char *server_url)
{
    free(g_base_url);
    free(g_send_url);
    free(g_poll_url);
    g_base_url = strdup(server_url);
    size_t base_len = strlen(g_base_url) + 1;
    g_send_url = malloc(base_len + strlen(SEND_PATH));
    g_poll_url = malloc(base_len + strlen(POLL_PATH));
    snprintf(g_send_url, base_len + strlen(SEND_PATH), "%s" SEND_PATH, g_base_url);
    snprintf(g_poll_url, base_len + strlen(POLL_PATH), "%s" POLL_PATH, g_base_url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    if (!curl_handle)
        return -1;
    curl_headers = curl_slist_append(NULL, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, curl_headers);
    curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
    last_hb = time(NULL);
    return 0;
}

char *http_client_poll(size_t *buflen)
{
    // Reuse the global CURL handle to preserve TCP connection (keep-alive)
    curl_easy_reset(curl_handle);

    struct PollCtx ctx = {malloc(MAX_PKT), MAX_PKT, 0};
    curl_easy_setopt(curl_handle, CURLOPT_URL, g_poll_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, recv_cb);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &ctx);
    // Wait up to 500 ms for data
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, 500L);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK || ctx.len == 0)
    {
        free(ctx.buf);
        *buflen = 0;
        return NULL;
    }
    *buflen = ctx.len;
    return (char *)ctx.buf;
}

int http_client_send(const uint8_t *data, size_t len)
{
    curl_easy_setopt(curl_handle, CURLOPT_URL, g_send_url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)len);
    return (curl_easy_perform(curl_handle) == CURLE_OK) ? 0 : -1;
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
    if (curl_handle)
    {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();
    curl_slist_free_all(curl_headers);
    free(g_base_url);
    free(g_send_url);
    free(g_poll_url);
}
