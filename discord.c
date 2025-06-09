// discord.c - Discord API client implementation
// discord.h / discord.c
// 职责：封装 Discord REST API

// discord_init(token, channel_id)：初始化 libcurl、拼装好发送、接收、删除消息的 URL 和 HTTP 头

// discord_send()：把 Base64 字符串当作 JSON content 发消息

// discord_receive()：拉取最近若干条消息，解析成 discord_message_t{id,content} 数组

// discord_delete()：删消息以免重复处理

// discord_base64_decode()：内置一套简单解码（可选，因为已有 base64.c）

// discord_cleanup()：收尾，释放 curl 全局资源

// 注意点

// discord_receive() 如果返回负数或 0，要在业务层做好容错。

// JSON 解析失败时要打印或处理掉垃圾数据。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include "discord.h"
#include "cJSON.h"
#include "base64.h"

#define BASE_URL       "https://discord.com/api/v10"
#define RECEIVE_LIMIT  10
#define URL_MAX_LEN    256

// Internal CURL handle and headers
static CURL *curl_handle = NULL;
static struct curl_slist *headers = NULL;
static char send_url[URL_MAX_LEN];
static char receive_url[URL_MAX_LEN];
static char delete_prefix[URL_MAX_LEN];
static pthread_mutex_t curl_mutex = PTHREAD_MUTEX_INITIALIZER;

// Buffer for HTTP responses
struct response_buffer {
    char *data;
    size_t size;
};

// Write callback for libcurl
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    struct response_buffer *buf = userdata;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

int discord_init(const char *token, const char *channel_id) {
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (!curl_handle) return -1;

    snprintf(send_url, sizeof(send_url), "%s/channels/%s/messages", BASE_URL, channel_id);
    snprintf(receive_url, sizeof(receive_url), "%s/channels/%s/messages?limit=%d", BASE_URL, channel_id, RECEIVE_LIMIT);
    snprintf(delete_prefix, sizeof(delete_prefix), "%s/channels/%s/messages/", BASE_URL, channel_id);

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bot %s", token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);

    return 0;
}

int discord_send(const char *message) {
    if (!curl_handle) return -1;

    pthread_mutex_lock(&curl_mutex);
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "content", message);
    char *body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    curl_easy_setopt(curl_handle, CURLOPT_URL, send_url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, body);
    CURLcode res = curl_easy_perform(curl_handle);
    pthread_mutex_unlock(&curl_mutex);

    free(body);
    if (res != CURLE_OK) {
        fprintf(stderr, "[discord] send error: %s\n", curl_easy_strerror(res));
        return -1;
    }
    curl_easy_setopt(curl_handle, CURLOPT_POST, 0L);
    return 0;
}

int discord_receive(discord_message_t **out_msgs) {
    if (!curl_handle || !out_msgs) return 0;

    struct response_buffer buf = { .data = malloc(1), .size = 0 };

    pthread_mutex_lock(&curl_mutex);
    curl_easy_setopt(curl_handle, CURLOPT_URL, receive_url);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &buf);
    CURLcode res = curl_easy_perform(curl_handle);
    pthread_mutex_unlock(&curl_mutex);

    if (res != CURLE_OK) {
        fprintf(stderr, "[discord] receive error: %s\n", curl_easy_strerror(res));
        free(buf.data);
        return 0;
    }

    cJSON *json = cJSON_Parse(buf.data);
    free(buf.data);
    if (!json) {
        fprintf(stderr, "[discord] JSON parse failed\n");
        return 0;
    }
    if (!cJSON_IsArray(json)) {
        fprintf(stderr, "[discord] unexpected JSON structure\n");
        cJSON_Delete(json);
        return 0;
    }

    int count = cJSON_GetArraySize(json);
    discord_message_t *msgs = malloc(count * sizeof(discord_message_t));
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        cJSON *id   = cJSON_GetObjectItem(item, "id");
        cJSON *cont = cJSON_GetObjectItem(item, "content");
        msgs[i].id      = strdup(id   ? id->valuestring   : "");
        msgs[i].content = strdup(cont ? cont->valuestring : "");
    }
    cJSON_Delete(json);
    *out_msgs = msgs;
    return count;
}

int discord_delete(const char *message_id) {
    if (!curl_handle || !message_id) return -1;

    char url[URL_MAX_LEN];
    snprintf(url, sizeof(url), "%s%s", delete_prefix, message_id);

    pthread_mutex_lock(&curl_mutex);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
    CURLcode res = curl_easy_perform(curl_handle);
    pthread_mutex_unlock(&curl_mutex);

    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    if (res != CURLE_OK) {
        fprintf(stderr, "[discord] delete error: %s\n", curl_easy_strerror(res));
        return -1;
    }
    return 0;
}

unsigned char *discord_base64_decode(const char *data, size_t *out_len) {
    return base64_decode(data, out_len);
}

void discord_cleanup() {
    if (headers) curl_slist_free_all(headers);
    if (curl_handle) curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
}
