#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MACRO_NAME_LEN  24
#define MACRO_KEYS_MAX  8
#define MACRO_LAYERS    3
#define MACRO_SLOTS     4

/** Consumer-page usages we support as one-shot media keys. */
typedef enum {
    MEDIA_NONE        = 0,
    MEDIA_PLAY_PAUSE  = 0x00CD,
    MEDIA_SCAN_NEXT   = 0x00B5,
    MEDIA_SCAN_PREV   = 0x00B6,
    MEDIA_MUTE        = 0x00E2,
    MEDIA_VOL_UP      = 0x00E9,
    MEDIA_VOL_DOWN    = 0x00EA,
} media_usage_t;

typedef struct {
    char name[MACRO_NAME_LEN];
    uint8_t modifier;                       /**< HID modifier bitmap */
    uint8_t keys[MACRO_KEYS_MAX];           /**< keycodes */
    int nkeys;
    char text[64];                          /**< optional ASCII text */
    uint16_t consumer;                      /**< media_usage_t, 0 = none */
} macro_t;

esp_err_t macros_init(void);

/** Current layer (0..MACRO_LAYERS-1). */
int  macros_get_layer(void);
void macros_set_layer(int layer);
/** Next layer, wrapping. Returns the new layer. */
int  macros_cycle_layer(void);

esp_err_t macros_set(int idx, const macro_t *m);
void macros_get(int idx, macro_t *out);
int macros_count(void);
esp_err_t macros_run(int idx);

#ifdef __cplusplus
}
#endif