#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*audio_capture_sink_t)(const uint8_t *pcm, size_t len, void *ctx);
typedef void (*audio_playback_done_cb_t)(void *ctx);

/** Initialise independent I2S1 microphone RX and I2S0 speaker TX paths. */
esp_err_t audio_init(void);

/** Stream microphone PCM16 chunks to sink until audio_capture_stop(). */
esp_err_t audio_capture_start(audio_capture_sink_t sink, void *ctx);
void audio_capture_stop(void);
bool audio_is_capturing(void);

/** Start a new streamed PCM16 reply, reconfiguring the speaker clock. */
esp_err_t audio_playback_start(int sample_rate);
esp_err_t audio_playback_write(const uint8_t *pcm, size_t len, TickType_t timeout);
void audio_playback_finish(void);
void audio_playback_abort(void);
bool audio_is_playing(void);
void audio_set_playback_done_callback(audio_playback_done_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
