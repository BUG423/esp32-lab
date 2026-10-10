#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Number of CSI subcarriers we track (ESP32 HT20 => 64). */
#define CSI_MAX_SUBCARRIERS 64

/** Sliding window length for variance (frames). */
#define CSI_WINDOW_LEN 32

/** How often we publish a processed frame to the web UI (ms). */
#define CSI_PUBLISH_PERIOD_MS 100

typedef struct {
    float subcarrier_amp[CSI_MAX_SUBCARRIERS];  ///< mean amplitude per subcarrier
    float subcarrier_var[CSI_MAX_SUBCARRIERS];  ///< temporal variance per subcarrier
    float motion_energy;                        ///< aggregated motion energy
    float baseline;                             ///< adaptive empty-room baseline
    float presence_score;                       ///< 0..1, higher = more confident presence
    bool  present;                              ///< hard presence decision
    float breath_bpm;                           ///< estimated breathing rate (0 if unknown)
    int   sample_count;                         ///< frames processed since boot
    int64_t ts_us;                              ///< timestamp of this frame
} csi_frame_t;

/**
 * @brief Initialise the CSI signal processor.
 */
esp_err_t csi_processor_init(void);

/**
 * @brief Feed one raw CSI buffer (int8 I/Q interleaved) into the processor.
 *
 * @param buf       pointer to CSI bytes
 * @param len       number of bytes
 * @param rssi      RSSI of the packet (dBm)
 * @param ts_us     arrival timestamp (microseconds)
 */
void csi_processor_feed(const int8_t *buf, uint32_t len, int8_t rssi, int64_t ts_us);

/**
 * @brief Get a snapshot of the latest processed frame (thread-safe).
 */
void csi_processor_get_frame(csi_frame_t *out);

/**
 * @brief Force re-calibration of the empty-room baseline.
 */
void csi_processor_recalibrate(void);

#ifdef __cplusplus
}
#endif
