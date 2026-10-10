/**
 * Optical PHY — drive an LED as transmitter, read it as photodiode.
 *
 * Wiring (per node):
 *
 *   TX:  GPIO ──[220Ω]──|>|── GND        (LED anode via resistor)
 *   RX:  ADC GPIO ───────|>|── GND       (same LED, or a second LED)
 *
 * The LED in reverse (or photovoltaic) mode produces a small voltage
 * proportional to incident light. We sample it with the ADC and compare
 * against an adaptive threshold.
 *
 * Encoding: Manchester (self-clocking, DC-balanced).
 *   bit '1' = HIGH half, LOW half
 *   bit '0' = LOW  half, HIGH half
 */

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "optical.h"
#include "protocol.h"

static const char *TAG = "optical";

static int s_tx_gpio = -1;
static int s_rx_gpio = -1;
static int s_baud = 250;

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_channel_t s_adc_ch;
static adc_unit_t s_adc_unit;

static volatile bool s_tx_mode = false;
static optical_stats_t s_stats;
static optical_frame_cb_t s_frame_cb = NULL;

/* ring of raw samples for the web scope */
#define SCOPE_LEN 128
static int16_t s_scope[SCOPE_LEN];
static int s_scope_idx = 0;

static inline int64_t half_period_us(void)
{
    /* Manchester: 2 halves per bit */
    return 1000000LL / (s_baud * 2);
}

/* ------------------------------------------------------------------ */
/* ADC helpers                                                         */
/* ------------------------------------------------------------------ */

static int read_adc(void)
{
    int raw = 0;
    if (s_adc) {
        adc_oneshot_read(s_adc, s_adc_ch, &raw);
    }
    return raw;
}

static void scope_push(int v)
{
    s_scope[s_scope_idx] = (int16_t)v;
    s_scope_idx = (s_scope_idx + 1) % SCOPE_LEN;
}

/* ------------------------------------------------------------------ */
/* Mode switching                                                      */
/* ------------------------------------------------------------------ */

void optical_set_mode_tx(void)
{
    if (s_tx_gpio < 0) return;
    gpio_set_direction(s_tx_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_tx_gpio, 0);
    s_tx_mode = true;
}

void optical_set_mode_rx(void)
{
    if (s_tx_gpio >= 0) {
        /* release the line so the LED can act as sensor */
        gpio_set_level(s_tx_gpio, 0);
    }
    s_tx_mode = false;
}

/* ------------------------------------------------------------------ */
/* TX                                                                  */
/* ------------------------------------------------------------------ */

static void tx_half(int level, int64_t t_us)
{
    gpio_set_level(s_tx_gpio, level);
    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < t_us) {
        /* busy wait for precise timing */
    }
}

static void tx_bit(int bit, int64_t t_us)
{
    if (bit) {
        tx_half(1, t_us);
        tx_half(0, t_us);
    } else {
        tx_half(0, t_us);
        tx_half(1, t_us);
    }
}

esp_err_t optical_send_byte(uint8_t b)
{
    if (s_tx_gpio < 0) return ESP_ERR_INVALID_STATE;
    int64_t t = half_period_us();
    optical_set_mode_tx();

    /* 8 data bits, MSB first */
    for (int i = 7; i >= 0; i--) {
        tx_bit((b >> i) & 1, t);
    }
    /* idle low */
    gpio_set_level(s_tx_gpio, 0);
    return ESP_OK;
}

int optical_send(const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++) {
        if (optical_send_byte(data[i]) != ESP_OK) {
            return i;
        }
    }
    return len;
}

/* ------------------------------------------------------------------ */
/* RX                                                                  */
/* ------------------------------------------------------------------ */

/**
 * Sample the ADC quickly and estimate a bit using Manchester:
 * compare first half vs second half of the bit period.
 */
static int rx_bit(int64_t t_us, int32_t timeout_us)
{
    /* wait for edge / preamble — sample until level changes or timeout */
    int64_t t0 = esp_timer_get_time();
    int prev = read_adc();
    scope_push(prev);

    /* simple threshold from running stats */
    static float thresh = 800.f;
    int waited = 0;
    while (esp_timer_get_time() - t0 < timeout_us) {
        int cur = read_adc();
        scope_push(cur);
        /* update adaptive threshold slowly */
        thresh += 0.05f * (cur - thresh);
        int diff = cur - (int)thresh;
        if (diff > 80 || diff < -80) {
            /* edge detected — align to half-bit */
            break;
        }
        prev = cur;
        waited++;
        if (waited > 2000) {
            /* don't spin forever */
            t0 = esp_timer_get_time();
            waited = 0;
        }
    }

    /* measure first half */
    int64_t start = esp_timer_get_time();
    int sum1 = 0, n1 = 0;
    while (esp_timer_get_time() - start < t_us) {
        sum1 += read_adc();
        n1++;
    }
    /* measure second half */
    start = esp_timer_get_time();
    int sum2 = 0, n2 = 0;
    while (esp_timer_get_time() - start < t_us) {
        sum2 += read_adc();
        n2++;
    }
    if (n1 == 0 || n2 == 0) return -1;

    int avg1 = sum1 / n1;
    int avg2 = sum2 / n2;
    scope_push(avg1);
    scope_push(avg2);

    /* Manchester: 1 = high then low => avg1 > avg2 */
    return (avg1 > avg2) ? 1 : 0;
}

int optical_recv_byte(uint8_t *out, int32_t timeout_us)
{
    if (!out) return -1;
    int64_t t = half_period_us();
    optical_set_mode_rx();

    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        int bit = rx_bit(t, (i == 0) ? timeout_us : t * 4);
        if (bit < 0) {
            return (i == 0) ? 0 : -1;  /* timeout on first bit = 0 */
        }
        b = (uint8_t)((b << 1) | (bit & 1));
    }
    *out = b;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Stats                                                               */
/* ------------------------------------------------------------------ */

void optical_calibrate(void)
{
    optical_set_mode_rx();
    int64_t t0 = esp_timer_get_time();
    double sum = 0;
    int n = 0;
    while (esp_timer_get_time() - t0 < 200000) { /* 200 ms */
        sum += read_adc();
        n++;
    }
    s_stats.noise_floor = n ? (float)(sum / n) : 0.f;
    ESP_LOGI(TAG, "calibrated noise floor = %.1f", s_stats.noise_floor);
}

void optical_get_stats(optical_stats_t *out)
{
    if (!out) return;
    optical_set_mode_rx();

    /* quick capture */
    int mn = 4095, mx = 0;
    double sum = 0, sum2 = 0;
    int n = 0;
    for (int i = 0; i < 256; i++) {
        int v = read_adc();
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        sum2 += (double)v * v;
        n++;
    }
    double mean = n ? sum / n : 0;
    double var = n ? (sum2 / n - mean * mean) : 0;
    if (var < 0) var = 0;
    float std = (float)sqrtf((float)var);

    memcpy(out->level, s_scope, sizeof(s_scope));
    out->count = SCOPE_LEN;
    out->noise_floor = s_stats.noise_floor;
    out->signal = (float)(mx - mn);
    out->snr_db = std > 0.01f ? 20.f * log10f((out->signal + 1.f) / (std + 1.f)) : 0.f;
    int q = (int)(out->signal * 0.25f);
    out->rssi_like = q > 100 ? 100 : (q < 0 ? 0 : q);
}

/* ------------------------------------------------------------------ */
/* RX task with preamble hunt                                          */
/* ------------------------------------------------------------------ */

#define PREAMBLE_BYTE 0xAA
#define PREAMBLE_N    3

static void rx_task(void *arg)
{
    uint8_t window[PREAMBLE_N] = {0};
    while (1) {
        uint8_t b = 0;
        int r = optical_recv_byte(&b, 2000 /* us */);
        if (r == 1) {
            /* slide window looking for preamble */
            memmove(window, window + 1, PREAMBLE_N - 1);
            window[PREAMBLE_N - 1] = b;

            int hit = 1;
            for (int i = 0; i < PREAMBLE_N; i++) {
                if (window[i] != PREAMBLE_BYTE) { hit = 0; break; }
            }
            if (!hit) continue;

            /* read frame: len + payload + crc8 */
            uint8_t len = 0;
            if (optical_recv_byte(&len, 50000) != 1) continue;
            if (len == 0 || len > GL_MAX_PAYLOAD) continue;

            uint8_t payload[GL_MAX_PAYLOAD + 1];
            int got = 0;
            for (int i = 0; i < len; i++) {
                uint8_t d = 0;
                if (optical_recv_byte(&d, 50000) != 1) break;
                payload[got++] = d;
            }
            if (got != len) continue;

            uint8_t crc = 0;
            if (optical_recv_byte(&crc, 50000) != 1) continue;

            uint8_t calc = 0;
            for (int i = 0; i < len; i++) calc ^= payload[i];  /* simple XOR-CRC8 */
            if (calc != crc) {
                s_stats.rx_errors++;
                continue;
            }
            payload[len] = 0;
            if (s_frame_cb) {
                s_frame_cb(payload, len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t optical_start_rx_task(optical_frame_cb_t cb)
{
    s_frame_cb = cb;
    xTaskCreate(rx_task, "opt_rx", 4096, NULL, 10, NULL);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

esp_err_t optical_init(int tx_gpio, int rx_adc_gpio, int baud)
{
    s_tx_gpio = tx_gpio;
    s_rx_gpio = rx_adc_gpio;
    s_baud = baud > 0 ? baud : 250;

    /* TX pin */
    if (s_tx_gpio >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << s_tx_gpio,
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&io);
        gpio_set_level(s_tx_gpio, 0);
    }

    /* RX ADC */
    if (s_rx_gpio >= 0) {
        ESP_ERROR_CHECK(adc_oneshot_io_to_channel(s_rx_gpio, &s_adc_unit, &s_adc_ch));
        adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = s_adc_unit};
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &s_adc));
        adc_oneshot_chan_cfg_t ccfg = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_adc_ch, &ccfg));
    }

    memset(&s_stats, 0, sizeof(s_stats));
    optical_calibrate();

    ESP_LOGI(TAG, "optical up  TX=GPIO%d  RX=GPIO%d  baud=%d",
             s_tx_gpio, s_rx_gpio, s_baud);
    return ESP_OK;
}
