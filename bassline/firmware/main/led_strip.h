#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the WS2812 strip via RMT (espressif/led_strip). */
esp_err_t leds_init(void);

/** Number of LEDs on the strip. */
int leds_count(void);

/** Set one pixel (0-based), values 0..255. */
void leds_set(int idx, uint8_t r, uint8_t g, uint8_t b);

/** Push the pixel buffer to the strip. */
void leds_show(void);

/** Blank the strip. */
void leds_clear(void);

#ifdef __cplusplus
}
#endif
