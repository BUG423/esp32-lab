/**
 * ESP-NOW broadcast chat core.
 *
 * Wire format (packed, sent as one ESP-NOW frame):
 *   magic u16 | ver u8 | type u8 | msg_id u32 | nick[32] | text[200]
 *
 * All nodes broadcast to FF:FF:FF:FF:FF:FF. Duplicates are dropped by
 * message id. Flood control: nickname <= 32, text <= 200, rate limit.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHAT_NICK_MAX     32
#define CHAT_TEXT_MAX     200
#define CHAT_MSG_MAX      48
#define CHAT_PEER_MAX     16
#define CHAT_MSG_MAGIC    0xAC48
#define CHAT_MSG_VERSION  1
#define CHAT_MSG_TYPE_TEXT 1

/** One chat message as shown in the UI. */
typedef struct {
    uint32_t id;
    char     nick[CHAT_NICK_MAX + 1];
    char     text[CHAT_TEXT_MAX + 1];
    int8_t   rssi;
    uint8_t  mac[6];
    bool     self;
    int64_t  ts_ms;          /* esp_timer time since boot */
} chat_msg_t;

/** A peer heard on the air. */
typedef struct {
    uint8_t  mac[6];
    char     nick[CHAT_NICK_MAX + 1];
    int8_t   rssi;
    uint32_t msg_count;
    int64_t  last_seen_ms;
} chat_peer_t;

typedef void (*chat_notify_fn)(void);

/** Init WiFi-independent ESP-NOW stack (call after wifi_manager_start). */
esp_err_t espnow_chat_init(void);

/** Broadcast a text message (subject to rate limit). */
esp_err_t espnow_chat_send(const char *text);

/** Register a callback fired from the ESP-NOW RX task on any change. */
void espnow_chat_set_notify_cb(chat_notify_fn cb);

/** Copy up to max most-recent messages into out; returns count. */
int espnow_chat_get_messages(chat_msg_t *out, int max);

/** Copy up to max peers into out; returns count. */
int espnow_chat_get_peers(chat_peer_t *out, int max);

/** Lifetime counters. */
void espnow_chat_get_stats(uint32_t *tx, uint32_t *rx, uint32_t *dup, uint32_t *drops);

/** This node's nickname. */
const char *espnow_chat_get_nick(void);

#ifdef __cplusplus
}
#endif
