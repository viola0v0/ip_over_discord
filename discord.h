// discord.h - Header file for Discord API interactions
#ifndef DISCORD_H
#define DISCORD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure representing a Discord message with ID and content.
 */
typedef struct {
    char *id;       /**< Message ID (heap-allocated) */
    char *content;  /**< Message content (heap-allocated) */
} discord_message_t;

/**
 * @brief Initialize the Discord API client.
 * @param token      Bot token string (null-terminated).
 * @param channel_id Discord channel ID string.
 * @return 0 on success, non-zero on error.
 */
int discord_init(const char *token, const char *channel_id);

/**
 * @brief Send a Base64-encoded payload as a Discord message.
 * @param message Base64 string.
 * @return 0 on success, non-zero on error.
 */
int discord_send(const char *message);

/**
 * @brief Poll Discord and receive recent messages.
 * @param messages Pointer to array; function will allocate.
 * @return Number of messages received, or 0 on error.
 */
int discord_receive(discord_message_t **messages);

/**
 * @brief Delete a Discord message by ID.
 * @param message_id ID string.
 * @return 0 on success, non-zero on error.
 */
int discord_delete(const char *message_id);

/**
 * @brief Decode a Base64 string into raw bytes.
 * @param data          Base64 string.
 * @param output_length Pointer to output length.
 * @return Heap buffer of bytes, or NULL on error.
 */
unsigned char *discord_base64_decode(const char *data, size_t *output_length);

/**
 * @brief Clean up and release Discord API resources.
 */
void discord_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // DISCORD_H
