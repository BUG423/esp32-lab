/**
 * Packet framing over the optical PHY.
 *
 * Frame: 0xAA 0xAA 0xAA | LEN | PAYLOAD[LEN] | XOR-CRC8
 */

#include <string.h>
#include "esp_log.h"
#include "sdkconfig.h"

#include "protocol.h"
#include "pairing.h"

static const char *TAG = "proto";

static proto_text_cb_t s_text_cb = NULL;
static proto_stats_t s_stats;

void protocol_set_text_handler(proto_text_cb_t cb)
{
    s_text_cb = cb;
}

esp_err_t protocol_init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    pairing_init();
    return ESP_OK;
}

static uint8_t crc8_xor(const uint8_t *d, int n)
{
    uint8_t c = 0;
    for (int i = 0; i < n; i++) c ^= d[i];
    return c;
}

int protocol_send_payload(const uint8_t *data, int len)
{
    if (!data || len <= 0 || len > GL_MAX_PAYLOAD) return -1;

    uint8_t hdr[3] = {0xAA, 0xAA, 0xAA};
    optical_send(hdr, 3);
    optical_send_byte((uint8_t)len);
    optical_send(data, len);
    optical_send_byte(crc8_xor(data, len));

    s_stats.tx_frames++;
    ESP_LOGI(TAG, "TX frame len=%d", len);
    return len;
}

int protocol_send_text(const char *text)
{
    if (!text) return -1;

    uint8_t buf[GL_MAX_PAYLOAD];
    int n = pairing_protect(text, buf, (int)sizeof(buf));
    if (n < 0) return -1;
    return protocol_send_payload(buf, n);
}

void protocol_on_bytes(const uint8_t *data, int len)
{
    /* called by optical layer after a validated frame */
    s_stats.rx_frames++;

    char plain[GL_MAX_PAYLOAD];
    int pn = pairing_on_payload(data, len, plain, (int)sizeof(plain));
    if (pn < 0) {
        s_stats.rx_errors++;
        return;
    }

    /* normalize to a NUL-terminated local buffer */
    char text[GL_MAX_PAYLOAD];
    int tlen;
    if (pn > 0) {
        tlen = pn;
        memcpy(text, plain, tlen);
        text[tlen] = 0;
    } else if (pn == 0 && len > 0 && data[0] != 0xB0 && data[0] != 0xB1 && data[0] != 0xB2) {
        tlen = len < (int)sizeof(text) - 1 ? len : (int)sizeof(text) - 1;
        memcpy(text, data, tlen);
        text[tlen] = 0;
    } else {
        /* consumed as control frame */
        return;
    }

    if (tlen > 0 && tlen < (int)sizeof(s_stats.last_rx_text)) {
        memcpy(s_stats.last_rx_text, text, tlen);
        s_stats.last_rx_text[tlen] = 0;
    }

    if (s_text_cb) {
        s_text_cb(text, (int)s_stats.last_rssi);
    }
}

void protocol_get_stats(proto_stats_t *out)
{
    if (out) *out = s_stats;
}
