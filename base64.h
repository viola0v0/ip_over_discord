// base64.h - Header file for Base64 encoding/decoding
#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode a buffer into Base64.
 * @param data          Input bytes to encode.
 * @param input_length  Number of bytes in the input buffer.
 * @param output_length Pointer to size_t where length of encoded string (excluding null) is stored.
 * @return              Null-terminated Base64 string (malloc’d). Caller must free.
 */
char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length);

/**
 * Decode a Base64-encoded string.
 * @param data          Null-terminated Base64 input string.
 * @param output_length Pointer to size_t where number of decoded bytes will be stored.
 * @return              Buffer of decoded bytes (malloc’d), or NULL on failure. Caller must free.
 */
unsigned char *base64_decode(const char *data, size_t *output_length);

void base64_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // BASE64_H
