#include "audio_in.h"

#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#if CONFIG_BASSLINE_MIC_I2S
#include "driver/i2s_std.h"
#else
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#endif

static const char *TAG = "audio_in";

#define AUDIO_SAMPLE_RATE   CONFIG_BASSLINE_SAMPLE_RATE
#define ADC_FRAME_MS        16

#if CONFIG_BASSLINE_MIC_I2S
static i2s_chan_handle_t s_rx;
static int32_t s_i2s_buf[512];
#else
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_cali_ok;
#endif

int audio_in_sample_rate(void)
{
    return AUDIO_SAMPLE_RATE;
}

#if CONFIG_BASSLINE_MIC_I2S

esp_err_t audio_in_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_BASSLINE_I2S_BCLK_GPIO,
            .ws = CONFIG_BASSLINE_I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = CONFIG_BASSLINE_I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_rx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2S mic ready (BCLK=%d WS=%d DIN=%d, %d Hz)",
             CONFIG_BASSLINE_I2S_BCLK_GPIO, CONFIG_BASSLINE_I2S_WS_GPIO,
             CONFIG_BASSLINE_I2S_DIN_GPIO, AUDIO_SAMPLE_RATE);
    return ESP_OK;
}

esp_err_t audio_in_read(float *out, int n)
{
    if (n > (int)(sizeof(s_i2s_buf) / sizeof(s_i2s_buf[0]))) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t need = (size_t)n * sizeof(int32_t);
    size_t got = 0;
    uint8_t *p = (uint8_t *)s_i2s_buf;

    while (got < need) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx, p + got, need - got, &bytes_read, pdMS_TO_TICKS(2000));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s read failed: %s", esp_err_to_name(err));
            return err;
        }
        if (bytes_read == 0) {
            continue;
        }
        got += bytes_read;
    }

    /* INMP441 delivers 24-bit data left-justified in 32-bit slots */
    for (int i = 0; i < n; i++) {
        out[i] = (float)s_i2s_buf[i] / 2147483648.0f;
    }
    return ESP_OK;
}

#else /* ADC microphone */

esp_err_t audio_in_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;
    err = adc_oneshot_io_to_channel(CONFIG_BASSLINE_ADC_GPIO, &unit, &chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d is not a valid ADC pin", CONFIG_BASSLINE_ADC_GPIO);
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc, chan, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali) == ESP_OK) {
        s_cali_ok = true;
    }
#else
    (void)cali_cfg;
#endif

    ESP_LOGI(TAG, "ADC mic ready (GPIO %d, %d Hz, cali=%d)",
             CONFIG_BASSLINE_ADC_GPIO, AUDIO_SAMPLE_RATE, (int)s_cali_ok);
    return ESP_OK;
}

esp_err_t audio_in_read(float *out, int n)
{
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;
    adc_oneshot_io_to_channel(CONFIG_BASSLINE_ADC_GPIO, &unit, &chan);

    /* Approximate pacing: sleep-based at sample rate. Fine for visualisation. */
    const int64_t period_us = 1000000LL / AUDIO_SAMPLE_RATE;
    int64_t next = esp_timer_get_time();

    for (int i = 0; i < n; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_adc, chan, &raw);
        if (err != ESP_OK) {
            return err;
        }

        int mv = 0;
        if (s_cali_ok) {
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_raw_to_voltage(s_cali, raw, &mv);
#endif
        } else {
            mv = (raw * 3300) / 4095;
        }

        /* centre around mid-rail ~1650 mV, normalise to [-1, 1] */
        float v = ((float)mv - 1650.0f) / 1650.0f;
        if (v > 1.0f) {
            v = 1.0f;
        }
        if (v < -1.0f) {
            v = -1.0f;
        }
        out[i] = v;

        next += period_us;
        int64_t now = esp_timer_get_time();
        if (next > now) {
            int64_t wait = next - now;
            if (wait > 1000) {
                vTaskDelay(pdMS_TO_TICKS(wait / 1000));
            }
        } else {
            next = now;
        }
    }
    return ESP_OK;
}

#endif /* mic backend */
