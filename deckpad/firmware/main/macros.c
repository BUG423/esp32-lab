/**
 * Macro storage + execution with layers and media keys.
 * Defaults: common creative/streamer shortcuts on layer 0.
 */

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "macros.h"
#include "bt_hid.h"

static const char *TAG = "macros";

static macro_t s_macros[MACRO_LAYERS][MACRO_SLOTS];
static int s_layer = 0;

static void set_defaults(void)
{
    /* ---- layer 0: streaming / calls ---- */
    strlcpy(s_macros[0][0].name, "Mute Mic", sizeof(s_macros[0][0].name));
    s_macros[0][0].modifier = 0x02 | 0x01; /* shift + ctrl */
    s_macros[0][0].keys[0] = 0x10;         /* 'm' */
    s_macros[0][0].nkeys = 1;

    strlcpy(s_macros[0][1].name, "Action 2", sizeof(s_macros[0][1].name));
    s_macros[0][1].modifier = 0x02 | 0x01;
    s_macros[0][1].keys[0] = 0x1D;         /* 'z' */
    s_macros[0][1].nkeys = 1;

    strlcpy(s_macros[0][2].name, "Hello Text", sizeof(s_macros[0][2].name));
    strlcpy(s_macros[0][2].text, "Hello from DeckPad!\n", sizeof(s_macros[0][2].text));

    strlcpy(s_macros[0][3].name, "Copy", sizeof(s_macros[0][3].name));
    s_macros[0][3].modifier = 0x01;
    s_macros[0][3].keys[0] = 0x06;         /* 'c' */
    s_macros[0][3].nkeys = 1;

    /* ---- layer 1: media ---- */
    strlcpy(s_macros[1][0].name, "Vol Down", sizeof(s_macros[1][0].name));
    s_macros[1][0].consumer = MEDIA_VOL_DOWN;

    strlcpy(s_macros[1][1].name, "Vol Up", sizeof(s_macros[1][1].name));
    s_macros[1][1].consumer = MEDIA_VOL_UP;

    strlcpy(s_macros[1][2].name, "Play/Pause", sizeof(s_macros[1][2].name));
    s_macros[1][2].consumer = MEDIA_PLAY_PAUSE;

    strlcpy(s_macros[1][3].name, "Mute", sizeof(s_macros[1][3].name));
    s_macros[1][3].consumer = MEDIA_MUTE;

    /* ---- layer 2: media transport ---- */
    strlcpy(s_macros[2][0].name, "Prev Track", sizeof(s_macros[2][0].name));
    s_macros[2][0].consumer = MEDIA_SCAN_PREV;

    strlcpy(s_macros[2][1].name, "Next Track", sizeof(s_macros[2][1].name));
    s_macros[2][1].consumer = MEDIA_SCAN_NEXT;

    strlcpy(s_macros[2][2].name, "Play/Pause", sizeof(s_macros[2][2].name));
    s_macros[2][2].consumer = MEDIA_PLAY_PAUSE;

    strlcpy(s_macros[2][3].name, "Mute", sizeof(s_macros[2][3].name));
    s_macros[2][3].consumer = MEDIA_MUTE;
}

static void load_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open("deckpad", NVS_READONLY, &h) != ESP_OK) return;
    for (int L = 0; L < MACRO_LAYERS; L++) {
        for (int i = 0; i < MACRO_SLOTS; i++) {
            char key[8];
            snprintf(key, sizeof(key), "n%d%d", L, i);
            size_t len = sizeof(macro_t);
            nvs_get_blob(h, key, &s_macros[L][i], &len);
        }
    }
    int8_t layer = 0;
    if (nvs_get_i8(h, "layer", &layer) == ESP_OK && layer >= 0 && layer < MACRO_LAYERS) {
        s_layer = layer;
    }
    nvs_close(h);
}

static void save_nvs(int layer, int idx)
{
    nvs_handle_t h;
    if (nvs_open("deckpad", NVS_READWRITE, &h) != ESP_OK) return;
    char key[8];
    snprintf(key, sizeof(key), "n%d%d", layer, idx);
    nvs_set_blob(h, key, &s_macros[layer][idx], sizeof(macro_t));
    nvs_commit(h);
    nvs_close(h);
}

static void save_layer(void)
{
    nvs_handle_t h;
    if (nvs_open("deckpad", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i8(h, "layer", (int8_t)s_layer);
    nvs_commit(h);
    nvs_close(h);
}

esp_err_t macros_init(void)
{
    memset(s_macros, 0, sizeof(s_macros));
    set_defaults();
    load_nvs();
    ESP_LOGI(TAG, "macros ready (%d layers × %d slots), layer=%d",
             MACRO_LAYERS, MACRO_SLOTS, s_layer);
    return ESP_OK;
}

int macros_get_layer(void) { return s_layer; }

void macros_set_layer(int layer)
{
    if (layer < 0 || layer >= MACRO_LAYERS) return;
    s_layer = layer;
    save_layer();
    ESP_LOGI(TAG, "layer -> %d", s_layer);
}

int macros_cycle_layer(void)
{
    s_layer = (s_layer + 1) % MACRO_LAYERS;
    save_layer();
    ESP_LOGI(TAG, "layer -> %d", s_layer);
    return s_layer;
}

int macros_count(void) { return MACRO_SLOTS; }

void macros_get(int idx, macro_t *out)
{
    if (idx < 0 || idx >= MACRO_SLOTS || !out) return;
    *out = s_macros[s_layer][idx];
}

esp_err_t macros_set(int idx, const macro_t *m)
{
    if (idx < 0 || idx >= MACRO_SLOTS || !m) return ESP_ERR_INVALID_ARG;
    s_macros[s_layer][idx] = *m;
    save_nvs(s_layer, idx);
    return ESP_OK;
}

esp_err_t macros_run(int idx)
{
    if (idx < 0 || idx >= MACRO_SLOTS) return ESP_ERR_INVALID_ARG;
    const macro_t *m = &s_macros[s_layer][idx];
    ESP_LOGI(TAG, "run L%d[%d] %s", s_layer, idx, m->name);

    if (!bt_hid_is_connected()) {
        ESP_LOGW(TAG, "BLE not connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (m->consumer) {
        esp_err_t err = bt_hid_send_consumer(m->consumer);
        if (err != ESP_OK) return err;
    }
    if (m->nkeys > 0) {
        uint8_t buf[8] = {0};
        buf[0] = m->modifier;
        for (int i = 0; i < m->nkeys && i < 6; i++) {
            buf[2 + i] = m->keys[i];
        }
        esp_err_t err = bt_hid_send_report(buf);
        if (err != ESP_OK) return err;
    }
    if (m->text[0]) {
        return bt_hid_send_text(m->text);
    }
    return ESP_OK;
}