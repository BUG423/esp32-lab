/**
 * I2S audio in (INMP441) + out (MAX98357A / PCM5102)
 *
 * Pin defaults target a common ESP32 DevKit + I2S DAC/amp wiring.
 * Change via menuconfig or edit here.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "sdkconfig.h"

#include "audio.h"

static const char *TAG = "audio";

/* ---- pin map (ESP32 DevKit typical) ---- */
#ifndef OPENPAL_I2S_BCLK
#define OPENPAL_I2S_BCLK   26
#endif
#ifndef OPENPAL_I2S_WS
#define OPENPAL_I2S_WS     25
#endif
#ifndef OPENPAL_I2S_DOUT
#define OPENPAL_I2S_DOUT   22
#endif
#ifndef OPENPAL_I2S_DIN
#define OPENPAL_I2S_DIN    21
#endif

static i2s_chan_handle_t s_tx = NULL;
static i2s_chan_handle_t s_rx = NULL;
static bool s_capturing = false;
static bool s_playing = false;

#define CAP_RING_SIZE (32 * 1024)
static RingbufHandle_t s_cap_ring = NULL;
static uint8_t *s_cap_acc = NULL;
static int s_cap_acc_len = 0;

esp_err_t audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_OPENPAL_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = OPENPAL_I2S_BCLK,
            .ws   = OPENPAL_I2S_WS,
            .dout = OPENPAL_I2S_DOUT,
            .din  = OPENPAL_I2S_DIN,
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    s_cap_ring = xRingbufferCreate(CAP_RING_SIZE, RINGBUF_TYPE_BYTEBUF);
    s_cap_acc = malloc(48 * 1024);
    if (!s_cap_ring || !s_cap_acc) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "I2S up @ %d Hz  BCLK=%d WS=%d DOUT=%d DIN=%d",
             CONFIG_OPENPAL_SAMPLE_RATE, OPENPAL_I2S_BCLK, OPENPAL_I2S_WS,
             OPENPAL_I2S_DOUT, OPENPAL_I2S_DIN);
    return ESP_OK;
}

/* background reader: continuously drain RX into ring while capturing */
static void capture_task(void *arg)
{
    uint8_t buf[1024];
    size_t bytes_read = 0;
    while (1) {
        if (!s_capturing) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (i2s_channel_read(s_rx, buf, sizeof(buf), &bytes_read, pdMS_TO_TICKS(100)) == ESP_OK) {
            if (bytes_read) {
                xRingbufferSend(s_cap_ring, buf, bytes_read, 0);
            }
        }
    }
}

esp_err_t audio_capture_start(void)
{
    s_cap_acc_len = 0;
    /* drain ring */
    size_t n;
    while ((n = xRingbufferGetCurFreeSize(s_cap_ring)) < CAP_RING_SIZE) {
        void *p = xRingbufferReceive(s_cap_ring, &n, 0);
        if (!p) break;
        vRingbufferReturnItem(s_cap_ring, p);
    }
    s_capturing = true;
    return ESP_OK;
}

int audio_capture_stop(uint8_t *out, int max_len)
{
    s_capturing = false;
    vTaskDelay(pdMS_TO_TICKS(30)); /* let reader settle */

    int total = 0;
    while (total < max_len) {
        size_t n = 0;
        void *p = xRingbufferReceive(s_cap_ring, &n, 0);
        if (!p) break;
        int take = (int)n;
        if (total + take > max_len) take = max_len - total;
        memcpy(out + total, p, take);
        vRingbufferReturnItem(s_cap_ring, p);
        total += take;
    }
    ESP_LOGI(TAG, "captured %d bytes", total);
    return total;
}

esp_err_t audio_play_pcm(const uint8_t *pcm, int len, int sample_rate)
{
    if (!pcm || len <= 0) return ESP_OK;
    s_playing = true;

    /* if rate differs we just play as-is (good enough for v1) */
    size_t written = 0;
    int off = 0;
    while (off < len) {
        int chunk = len - off;
        if (chunk > 1024) chunk = 1024;
        if (i2s_channel_write(s_tx, pcm + off, chunk, &written, pdMS_TO_TICKS(1000)) != ESP_OK) {
            break;
        }
        off += (int)written;
    }
    s_playing = false;
    return ESP_OK;
}

bool audio_is_playing(void)
{
    return s_playing;
}

/* create reader task at init */
static void __attribute__((constructor)) _audio_ctor(void) { /* placeholder */ }

esp_err_t audio_start_tasks(void)
{
    xTaskCreate(capture_task, "audio_cap", 4096, NULL, 8, NULL);
    return ESP_OK;
}
