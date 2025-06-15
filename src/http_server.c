// src/http_server.c
#include "http_server.h"
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#define MAX_URL_LEN 64

struct PacketNode
{
    struct PacketNode *next;
    size_t len;
    char *data;
};
static struct PacketNode *queue_head = NULL, *queue_tail = NULL;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct MHD_Daemon *server_daemon = NULL;

struct ConnectionInfo
{
    char *buf;
    size_t size;
};

static void enqueue_packet(const char *data, size_t len)
{
    struct PacketNode *node = malloc(sizeof(*node));
    node->data = malloc(len);
    memcpy(node->data, data, len);
    node->len = len;
    node->next = NULL;
    pthread_mutex_lock(&queue_mutex);
    if (!queue_tail)
        queue_head = node;
    else
        queue_tail->next = node;
    queue_tail = node;
    pthread_mutex_unlock(&queue_mutex);
}

static char *dequeue_packet(size_t *out_len)
{
    pthread_mutex_lock(&queue_mutex);
    if (!queue_head)
    {
        pthread_mutex_unlock(&queue_mutex);
        *out_len = 0;
        return NULL;
    }
    struct PacketNode *n = queue_head;
    queue_head = n->next;
    if (!queue_head)
        queue_tail = NULL;
    pthread_mutex_unlock(&queue_mutex);

    char *d = n->data;
    *out_len = n->len;
    free(n);
    return d;
}

static enum MHD_Result
request_handler(void *cls,
                struct MHD_Connection *connection,
                const char *url,
                const char *method,
                const char *version,
                const char *upload_data,
                size_t *upload_data_size,
                void **con_cls)
{
    if (0 == strcmp(method, "POST") && 0 == strcmp(url, "/send"))
    {
        struct ConnectionInfo *ci = *con_cls;
        if (!ci)
        {
            ci = calloc(1, sizeof(*ci));
            *con_cls = ci;
            return MHD_YES;
        }
        if (*upload_data_size)
        {
            ci->buf = realloc(ci->buf, ci->size + *upload_data_size);
            memcpy(ci->buf + ci->size, upload_data, *upload_data_size);
            ci->size += *upload_data_size;
            *upload_data_size = 0;
            return MHD_YES;
        }
        enqueue_packet(ci->buf, ci->size);
        printf("[server] queued %zu bytes from /send\n", ci->size);
        free(ci->buf);
        free(ci);
        *con_cls = NULL;

        struct MHD_Response *resp = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        int ok = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        return ok ? MHD_YES : MHD_NO;
    }

    if (0 == strcmp(method, "GET") && 0 == strcmp(url, "/poll"))
    {
        size_t pkt_len;
        char *pkt = dequeue_packet(&pkt_len);
        if (!pkt)
        {
            struct MHD_Response *resp = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ok = MHD_queue_response(connection, MHD_HTTP_NO_CONTENT, resp);
            MHD_destroy_response(resp);
            return ok ? MHD_YES : MHD_NO;
        }
        printf("[server] /poll returning %zu bytes\n", pkt_len);
        struct MHD_Response *resp = MHD_create_response_from_buffer(pkt_len, pkt, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(resp, "Content-Type", "application/octet-stream");
        int ok = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        return ok ? MHD_YES : MHD_NO;
    }

    const char *notf = "Not Found";
    struct MHD_Response *resp = MHD_create_response_from_buffer(strlen(notf), (void *)notf, MHD_RESPMEM_PERSISTENT);
    int ok = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
    MHD_destroy_response(resp);
    return ok ? MHD_YES : MHD_NO;
}

int http_server_start(const char *port_str)
{
    int port = atoi(port_str);
    if (port <= 0)
        return -1;
    server_daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY | MHD_USE_THREAD_PER_CONNECTION,
        port,
        NULL, NULL,
        &request_handler, NULL,
        MHD_OPTION_END);
    if (server_daemon)
        printf("[HTTP-SRV] listening on port %s\n", port_str);
    else
        fprintf(stderr, "[HTTP-SRV] FAILED to start on %s\n", port_str);
    fflush(stdout);
    return server_daemon ? 0 : -1;
}

void http_server_stop(void)
{
    if (server_daemon)
    {
        MHD_stop_daemon(server_daemon);
        server_daemon = NULL;
    }
}
