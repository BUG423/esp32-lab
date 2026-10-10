#pragma once

#include "esp_err.h"
#include "csi_processor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Home Assistant MQTT discovery + state publishing.
 *
 * Publishes:
 *   - binary_sensor  occupancy  (presence)
 *   - sensor         motion energy
 *   - sensor         presence score
 *   - sensor         breathing rate
 *
 * Discovery configs go to homeassistant/<component>/<id>/config,
 * state to <prefix>/state (JSON), availability to <prefix>/status.
 */
esp_err_t ha_mqtt_start(void);

/** Push the latest CSI snapshot to MQTT (called from the publish task). */
void ha_mqtt_publish_frame(const csi_frame_t *f);

/** Is the MQTT client connected? */
bool ha_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif
