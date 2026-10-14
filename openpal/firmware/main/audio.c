/**
 * Streaming I2S audio for a classic ESP32.
 *
 * INMP441 RX and MAX98357A TX use separate controllers and clocks. This is
 * intentional: the microphone sends 32-bit slots at 16 kHz while server TTS
 * is normally PCM16 at 24 kHz. Sharing one controller silently corrupted one
 * side of the original design.
 */

#include <limits.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "audio.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static StreamBufferHandle_t s_play_stream;
static SemaphoreHandle_t s_tx_mutex;
static TaskHandle_t s_play_task;

static volatile bool s_capturing;
static volatile bool s_playing;
static volatile bool s_finish_requested;
static volatile bool s_abort_requested;

static audio_capture_sink_t s_capture_sink;
static void *s_capture_ctx;
static audio_playback_done_cb_t s_playback_done_cb;
static void *s_playback_done_ctx;

static int16_t clamp_pcm16(int32_t sample)
{
    if (sample > INT16_MAX) return INT16_MAX;
    if (sample < INT16_MIN) return INT16_MIN;
    return (int16_t)sample;
}

static void drain_play_stream(void)
{
    uint8_t discard[256];
    while (xStreamBufferReceive(s_play_stream, discard, sizeof(discard), 0) > 0) {
    }
}

static void capture_task(void *arg)
{
    (void)arg;
    int32_t raw[256];
    int16_t pcm[256];

    while (true) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(
            s_rx, raw, sizeof(raw), &bytes_read, pdMS_TO_TICKS(200));
        if (err != ESP_OK || bytes_read == 0) {
            continue;
        }

        if (!s_capturing || s_capture_sink == NULL) {
            continue; /* Continuously drain DMA so a new utterance is fresh. */
        }

        size_t samples = bytes_read / sizeof(raw[0]);
        for (size_t i = 0; i < samples; ++i) {
            pcm[i] = clamp_pcm16(raw[i] >> CONFIG_OPENPAL_MIC_SHIFT);
        }
        esp_err_t sink_err = s_capture_sink(
            (const uint8_t *)pcm, samples * sizeof(pcm[0]), s_capture_ctx);
        if (sink_err != ESP_OK) {
            ESP_LOGW(TAG, "capture sink: %s", esp_err_to_name(sink_err));
        }
    }
}

static void playback_task(void *arg)
{
    (void)arg;
    uint8_t chunk[1024];

    while (true) {
        if (!s_playing) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        if (s_abort_requested) {
            /* Mark stopped before draining. A sender blocked on a full stream
             * buffer then wakes, sees !s_playing, and cannot enqueue again. */
            s_playing = false;
            s_finish_requested = false;
            drain_play_stream();
            vTaskDelay(1);
            drain_play_stream();
            s_abort_requested = false;
            continue;
        }

        size_t got = xStreamBufferReceive(
            s_play_stream, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        if (got > 0) {
            size_t offset = 0;
            while (offset < got && !s_abort_requested) {
                size_t written = 0;
                if (xSemaphoreTake(s_tx_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
                    break;
                }
                esp_err_t err = i2s_channel_write(
                    s_tx, chunk + offset, got - offset, &written, pdMS_TO_TICKS(200));
                xSemaphoreGive(s_tx_mutex);
                if (err != ESP_OK || written == 0) {
                    ESP_LOGW(TAG, "speaker write: %s", esp_err_to_name(err));
                    break;
                }
                offset += written;
            }
            continue;
        }

        if (s_finish_requested && xStreamBufferBytesAvailable(s_play_stream) == 0) {
            s_finish_requested = false;
            s_playing = false;
            if (s_playback_done_cb) {
                s_playback_done_cb(s_playback_done_ctx);
            }
        }
    }
}

esp_err_t audio_init(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&tx_chan_cfg, &s_tx, NULL), TAG, "create TX");

    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_OPENPAL_OUTPUT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_OPENPAL_SPK_BCLK_GPIO,
            .ws = CONFIG_OPENPAL_SPK_WS_GPIO,
            .dout = CONFIG_OPENPAL_SPK_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &tx_cfg), TAG, "init TX");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "enable TX");

    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&rx_chan_cfg, NULL, &s_rx), TAG, "create RX");

    i2s_std_config_t rx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_OPENPAL_INPUT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_OPENPAL_MIC_BCLK_GPIO,
            .ws = CONFIG_OPENPAL_MIC_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = CONFIG_OPENPAL_MIC_DIN_GPIO,
        },
    };
#if CONFIG_OPENPAL_MIC_CHANNEL_RIGHT
    rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
#else
    rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#endif
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &rx_cfg), TAG, "init RX");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "enable RX");

    s_play_stream = xStreamBufferCreate(CONFIG_OPENPAL_PLAYBACK_BUFFER_BYTES, 1);
    s_tx_mutex = xSemaphoreCreateMutex();
    if (!s_play_stream || !s_tx_mutex) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(capture_task, "audio_cap", 4096, NULL, 8, NULL) != pdPASS ||
        xTaskCreate(playback_task, "audio_play", 4096, NULL, 8, &s_play_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "mic I2S1 %d Hz: BCLK=%d WS=%d DIN=%d shift=%d",
             CONFIG_OPENPAL_INPUT_SAMPLE_RATE,
             CONFIG_OPENPAL_MIC_BCLK_GPIO, CONFIG_OPENPAL_MIC_WS_GPIO,
             CONFIG_OPENPAL_MIC_DIN_GPIO, CONFIG_OPENPAL_MIC_SHIFT);
    ESP_LOGI(TAG, "speaker I2S0: BCLK=%d WS=%d DOUT=%d",
             CONFIG_OPENPAL_SPK_BCLK_GPIO, CONFIG_OPENPAL_SPK_WS_GPIO,
             CONFIG_OPENPAL_SPK_DOUT_GPIO);
    return ESP_OK;
}

esp_err_t audio_capture_start(audio_capture_sink_t sink, void *ctx)
{
    if (!sink) return ESP_ERR_INVALID_ARG;
    if (s_capturing) return ESP_ERR_INVALID_STATE;
    s_capture_sink = sink;
    s_capture_ctx = ctx;
    s_capturing = true;
    return ESP_OK;
}

void audio_capture_stop(void)
{
    s_capturing = false;
    /* Let an in-flight sink call complete before audio_end is sent. */
    vTaskDelay(pdMS_TO_TICKS(40));
    s_capture_sink = NULL;
    s_capture_ctx = NULL;
}

bool audio_is_capturing(void)
{
    return s_capturing;
}

void audio_playback_abort(void)
{
    if (!s_playing) {
        if (s_abort_requested) return;
        if (s_play_stream) xStreamBufferReset(s_play_stream);
        return;
    }
    s_abort_requested = true;
    if (s_play_task) xTaskNotifyGive(s_play_task);
}

esp_err_t audio_playback_start(int sample_rate)
{
    if (sample_rate < 8000 || sample_rate > 48000) return ESP_ERR_INVALID_ARG;

    audio_playback_abort();
    for (int i = 0; i < 30 && (s_playing || s_abort_requested); ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (s_playing || s_abort_requested) return ESP_ERR_TIMEOUT;
    xStreamBufferReset(s_play_stream);

    if (xSemaphoreTake(s_tx_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = i2s_channel_disable(s_tx);
    if (err == ESP_OK) {
        i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        err = i2s_channel_reconfig_std_clock(s_tx, &clk);
    }
    if (err == ESP_OK) err = i2s_channel_enable(s_tx);
    xSemaphoreGive(s_tx_mutex);
    if (err != ESP_OK) return err;

    s_abort_requested = false;
    s_finish_requested = false;
    s_playing = true;
    xTaskNotifyGive(s_play_task);
    ESP_LOGI(TAG, "playback start @ %d Hz", sample_rate);
    return ESP_OK;
}

esp_err_t audio_playback_write(const uint8_t *pcm, size_t len, TickType_t timeout)
{
    if (!pcm || len == 0) return ESP_OK;
    if (!s_playing || s_abort_requested) return ESP_ERR_INVALID_STATE;

    size_t sent = 0;
    TickType_t started = xTaskGetTickCount();
    while (sent < len && s_playing && !s_abort_requested) {
        TickType_t elapsed = xTaskGetTickCount() - started;
        TickType_t wait = elapsed < timeout ? timeout - elapsed : 0;
        size_t n = xStreamBufferSend(s_play_stream, pcm + sent, len - sent, wait);
        if (n == 0) return ESP_ERR_TIMEOUT;
        sent += n;
    }
    return sent == len ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void audio_playback_finish(void)
{
    if (s_playing && !s_abort_requested) {
        s_finish_requested = true;
        xTaskNotifyGive(s_play_task);
    }
}

bool audio_is_playing(void)
{
    return s_playing;
}

void audio_set_playback_done_callback(audio_playback_done_cb_t cb, void *ctx)
{
    s_playback_done_cb = cb;
    s_playback_done_ctx = ctx;
}
