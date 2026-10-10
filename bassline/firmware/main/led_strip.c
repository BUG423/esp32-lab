#include "led_strip.h"

#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "leds";

/* WS2812 timing at 10 MHz RMT resolution (0.1 us per tick).
 * 0-bit: 0.4 us high + 0.8 us low;  1-bit: 0.8 us high + 0.4 us low. */
#define WS_T0H_TICKS  4
#define WS_T0L_TICKS  8
#define WS_T1H_TICKS  8
#define WS_T1L_TICKS  4

#define LEDS_MAX 300

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_copy;
static uint8_t s_pixels[LEDS_MAX][3];
static rmt_symbol_word_t s_symbols[LEDS_MAX * 24];
static int s_count;
static bool s_ready;

int leds_count(void)
{
    return s_count;
}

esp_err_t leds_init(void)
{
    s_count = CONFIG_BASSLINE_LED_COUNT;
    if (s_count < 1) {
        s_count = 1;
    }
    if (s_count > LEDS_MAX) {
        s_count = LEDS_MAX;
    }

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = CONFIG_BASSLINE_LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &s_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    rmt_copy_encoder_config_t enc_cfg = {};
    err = rmt_new_copy_encoder(&enc_cfg, &s_copy);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_copy_encoder_new failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_enable(s_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "WS2812 ready: %d LEDs on GPIO %d (RMT)", s_count, CONFIG_BASSLINE_LED_GPIO);

    leds_clear();
    leds_show();
    return ESP_OK;
}

void leds_set(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_ready || idx < 0 || idx >= s_count) {
        return;
    }
    /* WS2812 wire order is GRB */
    s_pixels[idx][0] = g;
    s_pixels[idx][1] = r;
    s_pixels[idx][2] = b;
}

static void s_build_symbols(void)
{
    rmt_symbol_word_t *w = s_symbols;
    for (int i = 0; i < s_count; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            bool one = (s_pixels[i][0] >> bit) & 1;
            w->level0 = 1;
            w->level1 = 0;
            if (one) {
                w->duration0 = WS_T1H_TICKS;
                w->duration1 = WS_T1L_TICKS;
            } else {
                w->duration0 = WS_T0H_TICKS;
                w->duration1 = WS_T0L_TICKS;
            }
            w++;
        }
        for (int bit = 7; bit >= 0; bit--) {
            bool one = (s_pixels[i][1] >> bit) & 1;
            w->level0 = 1;
            w->level1 = 0;
            if (one) {
                w->duration0 = WS_T1H_TICKS;
                w->duration1 = WS_T1L_TICKS;
            } else {
                w->duration0 = WS_T0H_TICKS;
                w->duration1 = WS_T0L_TICKS;
            }
            w++;
        }
        for (int bit = 7; bit >= 0; bit--) {
            bool one = (s_pixels[i][2] >> bit) & 1;
            w->level0 = 1;
            w->level1 = 0;
            if (one) {
                w->duration0 = WS_T1H_TICKS;
                w->duration1 = WS_T1L_TICKS;
            } else {
                w->duration0 = WS_T0H_TICKS;
                w->duration1 = WS_T0L_TICKS;
            }
            w++;
        }
    }
}

void leds_show(void)
{
    if (!s_ready) {
        return;
    }
    s_build_symbols();

    rmt_transmit_config_t tx_conf = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(s_chan, s_copy, s_symbols,
                                 sizeof(rmt_symbol_word_t) * 24 * s_count, &tx_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
        return;
    }
    rmt_tx_wait_all_done(s_chan, pdMS_TO_TICKS(50));

    /* reset code: line low > 50 us */
    esp_rom_delay_us(80);
}

void leds_clear(void)
{
    memset(s_pixels, 0, sizeof(s_pixels));
}
