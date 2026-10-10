#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BASSLINE_MAX_BINS 64

typedef enum {
    BASSLINE_MODE_SPECTRUM = 0,
    BASSLINE_MODE_VU,
    BASSLINE_MODE_BREATHING,
    BASSLINE_MODE_SPARKLE,
    BASSLINE_MODE_MAX
} bassline_mode_t;

typedef enum {
    BASSLINE_THEME_RAINBOW = 0,
    BASSLINE_THEME_FIRE,
    BASSLINE_THEME_ICE,
    BASSLINE_THEME_MAX
} bassline_theme_t;

/** Shared visualiser state (guarded by an internal mutex). */
typedef struct {
    float bins[BASSLINE_MAX_BINS];   /* 0..1 normalised magnitudes */
    float peaks[BASSLINE_MAX_BINS];  /* peak-hold, decays over time */
    float vu;                        /* 0..1 overall loudness */
    float vu_peak;
    int n_bins;
    bassline_mode_t mode;
    bassline_theme_t theme;
    float gain;                      /* 0.1 .. 4.0 */
    float decay;                     /* 0.50 .. 0.98 falloff factor */
} bassline_state_t;

/** Start HTTP + WebSocket server. */
esp_err_t web_server_start(void);

/** Copy current state (thread-safe). */
void web_server_get_state(bassline_state_t *out);

/** Update analysis outputs (thread-safe). Called from the audio task. */
void web_server_update_bins(const float *bins, int n_bins, float vu);

/** Broadcast one spectrum frame to all WebSocket clients (~30 fps). */
void web_server_broadcast(void);

#ifdef __cplusplus
}
#endif
