#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_init(void);

/** Start capturing mic PCM into the internal ring (called on PTT down). */
esp_err_t audio_capture_start(void);

/** Stop capturing. Returns number of bytes captured in this utterance. */
int audio_capture_stop(uint8_t *out, int max_len);

/** Play PCM16 mono buffer (blocking-ish, runs on a worker). */
esp_err_t audio_play_pcm(const uint8_t *pcm, int len, int sample_rate);

/** True while speaker is busy. */
bool audio_is_playing(void);

#ifdef __cplusplus
}
#endif
