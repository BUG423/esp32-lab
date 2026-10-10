#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "optical.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Framed packet: preamble(2) | len(1) | payload | crc8(1) */
#define GL_MAX_PAYLOAD 200

esp_err_t protocol_init(void);

/** Send a text message over the optical link (auto framed). */
int protocol_send_text(const char *text);

/** Send raw payload. */
int protocol_send_payload(const uint8_t *data, int len);

/** Called by RX task — handles framing + dispatch. */
void protocol_on_bytes(const uint8_t *data, int len);

/** Register a text handler (called with decoded UTF-8). */
typedef void (*proto_text_cb_t)(const char *text, int rssi);
void protocol_set_text_handler(proto_text_cb_t cb);

/** Link stats */
typedef struct {
    uint32_t tx_frames;
    uint32_t rx_frames;
    uint32_t rx_errors;
    uint32_t last_rssi;
    char last_rx_text[GL_MAX_PAYLOAD];
} proto_stats_t;

void protocol_get_stats(proto_stats_t *out);

#ifdef __cplusplus
}
#endif
