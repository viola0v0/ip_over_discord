// base64.c - Base64 encoding/decoding implementation
// base64.h / base64.c
// 职责：通用的 Base64 编解码

// base64_encode()：把二进制包转成 ASCII 字符串

// base64_decode()：把收到的 ASCII 字符串还原成二进制

// 注意点

// 两端（client/server）都要包含同一套实现，避免重复定义冲突。

// mod_table 与 padding 逻辑要一致。
#include "base64.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

// Base64 encoding/decoding tables
static const char b64_encoding_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static unsigned char *b64_decoding_table = NULL;
static const int mod_table[] = {0, 2, 1};
static pthread_mutex_t b64_mutex = PTHREAD_MUTEX_INITIALIZER;

// Build decoding table on first use
static void build_decoding_table() {
    b64_decoding_table = malloc(256);
    if (!b64_decoding_table) return;
    for (int i = 0; i < 64; i++) {
        b64_decoding_table[(unsigned char)b64_encoding_table[i]] = i;
    }
}

// Base64 encode
char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {
    *output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(*output_length + 1);
    if (!encoded_data) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = b64_encoding_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = b64_encoding_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = b64_encoding_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = b64_encoding_table[triple & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++) {
        encoded_data[*output_length - 1 - i] = '=';
    }
    encoded_data[*output_length] = '\0';
    return encoded_data;
}


// Base64 decode
unsigned char *base64_decode(const char *data, size_t *output_length) {
    size_t input_length = strlen(data);
    if (input_length % 4 != 0) return NULL;

    // 初始化解码表（只做一次）
    pthread_mutex_lock(&b64_mutex);
    if (!b64_decoding_table) build_decoding_table();
    pthread_mutex_unlock(&b64_mutex);

    *output_length = input_length / 4 * 3;
    if (input_length >= 1 && data[input_length - 1] == '=') (*output_length)--;
    if (input_length >= 2 && data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = malloc(*output_length + 1);
    if (!decoded_data) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : b64_decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : b64_decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : b64_decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : b64_decoding_table[(unsigned char)data[i++]];
        uint32_t triple   = (sextet_a << 18) | (sextet_b << 12)
                          | (sextet_c << 6)  | sextet_d;

        if (j < *output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 8)  & 0xFF;
        if (j < *output_length) decoded_data[j++] = triple & 0xFF;
    }

    decoded_data[*output_length] = '\0';
    return decoded_data;
}

// Optional: Cleanup decoding table (call before program exit)
void base64_cleanup(void) {
    pthread_mutex_lock(&b64_mutex);
    if (b64_decoding_table) {
        free(b64_decoding_table);
        b64_decoding_table = NULL;
    }
    pthread_mutex_unlock(&b64_mutex);
}