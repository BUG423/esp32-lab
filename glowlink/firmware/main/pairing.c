/**
 * Optical pairing — exchange a shared key as a blink sequence, then
 * protect messages with a demo-grade keystream.
 *
 * Control payloads (first byte is the type):
 *   0xB0 PAIR_OFFER  | 8-byte key
 *   0xB1 PAIR_ACK    | 8-byte key (echo)
 *   0xB2 ENC_MSG     | keystream-XOR ciphertext
 *
 * Anything else is plaintext (backward compatible with unpaired nodes).
 *
 * The "encryption" is XOR with a repeating 8-byte key plus a one-byte
 * running checksum. This is demo-grade — the point is the *optical key
 * exchange*, not industrial cryptography.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "pairing.h"
#include "protocol.h"

static const char *TAG = "pair";

#define FRAME_PAIR_OFFER 0xB0
#define FRAME_PAIR_ACK   0xB1
#define FRAME_ENC_MSG    0xB2

#define NVS_NS   "glowlink"
#define NVS_KEY  "pair_key"

static pair_state_t s_state = PAIR_IDLE;
static uint8_t s_key[PAIR_KEY_LEN];
static bool s_have_key = false;

static esp_err_t key_save(void)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (e != ESP_OK) return e;
    e = nvs_set_blob(h, NVS_KEY, s_key, PAIR_KEY_LEN);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return e;
}

static void key_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = PAIR_KEY_LEN;
    if (nvs_get_blob(h, NVS_KEY, s_key, &len) == ESP_OK && len == PAIR_KEY_LEN) {
        s_have_key = true;
        s_state = PAIR_PAIRED;
        ESP_LOGI(TAG, "restored paired key from NVS");
    }
    nvs_close(h);
}

esp_err_t pairing_init(void)
{
    s_state = PAIR_IDLE;
    s_have_key = false;
    memset(s_key, 0, sizeof(s_key));
    key_load();
    return ESP_OK;
}

pair_state_t pairing_get_state(void)
{
    return s_state;
}

bool pairing_is_paired(void)
{
    return s_state == PAIR_PAIRED && s_have_key;
}

bool pairing_get_key(uint8_t key_out[PAIR_KEY_LEN])
{
    if (!s_have_key || !key_out) return false;
    memcpy(key_out, s_key, PAIR_KEY_LEN);
    return true;
}

esp_err_t pairing_start(void)
{
    esp_fill_random(s_key, PAIR_KEY_LEN);
    s_have_key = true;
    s_state = PAIR_WAITING;

    uint8_t payload[1 + PAIR_KEY_LEN];
    payload[0] = FRAME_PAIR_OFFER;
    memcpy(payload + 1, s_key, PAIR_KEY_LEN);

    int n = protocol_send_payload(payload, (int)sizeof(payload));
    ESP_LOGI(TAG, "pairing offer sent (%d bytes) — point the LEDs together", n);
    return (n > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t pairing_unpair(void)
{
    memset(s_key, 0, sizeof(s_key));
    s_have_key = false;
    s_state = PAIR_IDLE;

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "unpaired");
    return ESP_OK;
}

static void keystream_xor(const uint8_t *in, uint8_t *out, int len)
{
    for (int i = 0; i < len; i++) {
        out[i] = in[i] ^ s_key[i % PAIR_KEY_LEN];
    }
}

static uint8_t checksum8(const uint8_t *d, int n)
{
    uint8_t c = 0;
    for (int i = 0; i < n; i++) c = (uint8_t)(c + d[i]);
    return c;
}

int pairing_protect(const char *plain, uint8_t *out, int out_cap)
{
    if (!plain || !out) return -1;
    int n = (int)strlen(plain);
    if (n < 0) return -1;

    if (!pairing_is_paired()) {
        if (n + 1 > out_cap) return -1;
        memcpy(out, plain, n);
        return n;
    }

    /* type | ciphertext[n] | checksum */
    if (n + 3 > out_cap) return -1;
    out[0] = FRAME_ENC_MSG;
    keystream_xor((const uint8_t *)plain, out + 1, n);
    out[1 + n] = checksum8(out + 1, n);
    return n + 2;
}

int pairing_on_payload(const uint8_t *data, int len, char *plain_out, int plain_cap)
{
    if (!data || len <= 0) return -1;

    switch (data[0]) {
    case FRAME_PAIR_OFFER: {
        if (len != 1 + PAIR_KEY_LEN) {
            ESP_LOGW(TAG, "bad offer len=%d", len);
            return -1;
        }
        memcpy(s_key, data + 1, PAIR_KEY_LEN);
        s_have_key = true;
        s_state = PAIR_PAIRED;
        key_save();

        uint8_t ack[1 + PAIR_KEY_LEN];
        ack[0] = FRAME_PAIR_ACK;
        memcpy(ack + 1, s_key, PAIR_KEY_LEN);
        protocol_send_payload(ack, (int)sizeof(ack));

        ESP_LOGI(TAG, "pairing complete (accepted offer)");
        return 0;
    }
    case FRAME_PAIR_ACK: {
        if (len != 1 + PAIR_KEY_LEN || !s_have_key) return -1;
        if (memcmp(s_key, data + 1, PAIR_KEY_LEN) != 0) {
            ESP_LOGW(TAG, "ack key mismatch");
            return -1;
        }
        s_state = PAIR_PAIRED;
        key_save();
        ESP_LOGI(TAG, "pairing complete (ack received)");
        return 0;
    }
    case FRAME_ENC_MSG: {
        if (!pairing_is_paired() || len < 3) return -1;
        int n = len - 2; /* drop type + checksum */
        if (n + 1 > plain_cap) return -1;
        uint8_t expect = data[1 + n];
        if (checksum8(data + 1, n) != expect) {
            ESP_LOGW(TAG, "enc checksum mismatch");
            return -1;
        }
        keystream_xor(data + 1, (uint8_t *)plain_out, n);
        plain_out[n] = 0;
        return n;
    }
    default:
        return 0; /* plaintext pass-through */
    }
}
