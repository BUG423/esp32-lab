#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WS_CLIENT_EVENT_CONNECTED,
    WS_CLIENT_EVENT_DISCONNECTED,
    WS_CLIENT_EVENT_STATUS,
    WS_CLIENT_EVENT_REPLY_START,
    WS_CLIENT_EVENT_AUDIO,
    WS_CLIENT_EVENT_REPLY_END,
    WS_CLIENT_EVENT_ERROR,
    WS_CLIENT_EVENT_LED,
} ws_client_event_type_t;

typedef struct {
    ws_client_event_type_t type;
    const char *text;
    const char *state;
    const uint8_t *data;
    size_t data_len;
    int sample_rate;
    int audio_bytes;
    bool led_on;
} ws_client_event_t;

typedef void (*ws_client_event_cb_t)(const ws_client_event_t *event, void *ctx);

esp_err_t ws_client_start(const char *uri, ws_client_event_cb_t callback, void *ctx);
bool ws_client_is_connected(void);

esp_err_t ws_client_send_text(const char *json);
esp_err_t ws_client_send_binary(const uint8_t *data, size_t len);
esp_err_t ws_client_send_audio_start(int sample_rate);
esp_err_t ws_client_send_audio_end(void);
esp_err_t ws_client_send_interrupt(void);
esp_err_t ws_client_send_playback_done(void);

#ifdef __cplusplus
}
#endif
