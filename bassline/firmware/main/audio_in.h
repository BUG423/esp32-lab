#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the selected microphone backend (I2S or ADC, via Kconfig). */
esp_err_t audio_in_init(void);

/**
 * Read exactly n samples as floats roughly in [-1, 1].
 * Blocks until the window is filled.
 */
esp_err_t audio_in_read(float *out, int n);

/** Configured capture sample rate in Hz. */
int audio_in_sample_rate(void);

#ifdef __cplusplus
}
#endif
