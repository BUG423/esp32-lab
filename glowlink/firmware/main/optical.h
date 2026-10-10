#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Raw optical bit sampling result for the oscilloscope view. */
typedef struct {
    int16_t level[128];     ///< ADC samples (raw 0..4095 centered)
    int     count;
    float   noise_floor;    ///< estimated background light
    float   signal;         ///< estimated signal amplitude
    float   snr_db;         ///< signal / noise in dB
    int     rssi_like;      ///< 0..100 link quality
    uint32_t rx_errors;     ///< CRC / framing errors
} optical_stats_t;

esp_err_t optical_init(int tx_gpio, int rx_adc_gpio, int baud);

/**
 * @brief Transmit one byte with Manchester encoding. Blocking, timing-critical.
 */
esp_err_t optical_send_byte(uint8_t b);

/**
 * @brief Transmit a buffer. Returns bytes sent.
 */
int optical_send(const uint8_t *data, int len);

/**
 * @brief Try to receive one byte within timeout_us.
 *
 * @return 1 on success, 0 on timeout, -1 on framing error
 */
int optical_recv_byte(uint8_t *out, int32_t timeout_us);

/**
 * @brief Start a background RX task that delivers framed packets via callback.
 */
typedef void (*optical_frame_cb_t)(const uint8_t *data, int len);
esp_err_t optical_start_rx_task(optical_frame_cb_t cb);

/** Force TX / RX mode (used by protocol). */
void optical_set_mode_tx(void);
void optical_set_mode_rx(void);

/** Fill stats for the web scope. */
void optical_get_stats(optical_stats_t *out);

/** Calibrate the noise floor with the current ambient light. */
void optical_calibrate(void);

#ifdef __cplusplus
}
#endif
