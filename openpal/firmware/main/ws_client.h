#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ws_reply_cb_t)(const char *text, const uint8_t *audio, int audio_len, int sample_rate);

esp_err_t ws_client_start(const char *uri, ws_reply_cb_t on_reply);
esp_err_t ws_client_send_text(const char *json);
esp_err_t ws_client_send_binary(const uint8_t *data, int len);
esp_err_t ws_client_send_audio_end(void);
bool ws_client_is_connected(void);

#ifdef __cplusplus
}
#endif
