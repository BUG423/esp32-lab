#include <stdio.h>
#include <math.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "audio_in.h"
#include "fft.h"
#include "led_strip.h"
#include "web_server.h"

static const char *TAG = "bassline";

#define FFT_N          CONFIG_BASSLINE_FFT_SIZE
#define LOG_BINS       CONFIG_BASSLINE_LOG_BINS
#define LED_UPDATE_MS  33

static float s_window[FFT_N];
static float s_mags[FFT_N / 2];
static float s_bins[LOG_BINS];
static float s_norm[LOG_BINS];

/* ------------------------------------------------------------------ */
/* Colour helpers                                                       */
/* ------------------------------------------------------------------ */

static void s_color_theme(bassline_theme_t theme, float t, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }

    switch (theme) {
    case BASSLINE_THEME_FIRE: {
        /* black -> red -> orange -> yellow -> white */
        float x = t;
        *r = (uint8_t)(255.0f * fminf(1.0f, x * 1.8f));
        *g = (uint8_t)(255.0f * fmaxf(0.0f, fminf(1.0f, (x - 0.35f) * 1.8f)));
        *b = (uint8_t)(255.0f * fmaxf(0.0f, (x - 0.75f) * 3.0f));
        break;
    }
    case BASSLINE_THEME_ICE: {
        /* black -> deep blue -> cyan -> white */
        *r = (uint8_t)(255.0f * fmaxf(0.0f, (t - 0.7f) * 3.0f));
        *g = (uint8_t)(255.0f * fminf(1.0f, t * 1.4f));
        *b = (uint8_t)(255.0f * fminf(1.0f, t * 1.2f + 0.15f));
        break;
    }
    case BASSLINE_THEME_RAINBOW:
    default: {
        /* blue -> cyan -> green -> yellow -> red */
        float x = t * 4.0f;
        if (x < 1.0f) {
            *r = 0;
            *g = (uint8_t)(255.0f * x);
            *b = 255;
        } else if (x < 2.0f) {
            *r = 0;
            *g = 255;
            *b = (uint8_t)(255.0f * (2.0f - x));
        } else if (x < 3.0f) {
            *r = (uint8_t)(255.0f * (x - 2.0f));
            *g = 255;
            *b = 0;
        } else {
            *r = 255;
            *g = (uint8_t)(255.0f * fmaxf(0.0f, 1.0f - (x - 3.0f) * 1.4f));
            *b = 0;
        }
        break;
    }
    }
}

/* ------------------------------------------------------------------ */
/* LED rendering                                                        */
/* ------------------------------------------------------------------ */

static void s_render_spectrum(const bassline_state_t *st)
{
    int n = leds_count();
    int bins = st->n_bins;
    if (bins < 1) {
        return;
    }

    for (int i = 0; i < n; i++) {
        /* map LED index into log-bin space (mirrored around centre) */
        float pos = (float)i / (float)(n - 1 > 0 ? n - 1 : 1);
        float mirrored = (pos < 0.5f) ? (pos * 2.0f) : (2.0f - pos * 2.0f);
        int b = (int)(mirrored * (bins - 1));
        if (b < 0) {
            b = 0;
        }
        if (b >= bins) {
            b = bins - 1;
        }

        float v = st->bins[b];
        /* vertical fill: LED height vs level */
        float height = pos; /* reuse as fill indicator in strip mode */
        (void)height;
        uint8_t r, g, bl;
        s_color_theme(st->theme, v, &r, &g, &bl);

        /* dim LEDs above the current level for a "meter" look on single column strips */
        float thresh = v;
        float vert = (pos < 0.5f) ? (pos * 2.0f) : (2.0f - pos * 2.0f);
        if (vert > thresh + 0.02f) {
            /* faint peak-hold cap */
            float pk = st->peaks[b];
            if (vert <= pk + 0.03f && vert >= pk - 0.03f) {
                r = (uint8_t)(r / 3 + 40);
                g = (uint8_t)(g / 3 + 40);
                bl = (uint8_t)(bl / 3 + 40);
            } else {
                r = (uint8_t)(r * 0.08f);
                g = (uint8_t)(g * 0.08f);
                bl = (uint8_t)(bl * 0.08f);
            }
        }
        leds_set(i, r, g, bl);
    }
}

static void s_render_vu(const bassline_state_t *st)
{
    int n = leds_count();
    int lit = (int)(st->vu * (float)n);
    if (lit > n) {
        lit = n;
    }
    int peak_i = (int)(st->vu_peak * (float)n);
    if (peak_i > n) {
        peak_i = n;
    }

    for (int i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;
        uint8_t r, g, b;
        s_color_theme(st->theme, t, &r, &g, &b);
        if (i >= lit && i != peak_i - 1) {
            r = (uint8_t)(r * 0.06f);
            g = (uint8_t)(g * 0.06f);
            b = (uint8_t)(b * 0.06f);
        }
        leds_set(i, r, g, b);
    }
}

static void s_render_breathing(const bassline_state_t *st)
{
    static float s_phase;
    s_phase += 0.03f + st->vu * 0.08f;
    if (s_phase > 1000.0f) {
        s_phase = 0.0f;
    }
    float level = 0.15f + 0.85f * (0.5f + 0.5f * sinf(s_phase));
    level *= (0.3f + 0.7f * st->vu + 0.3f * st->gain * 0.5f);
    if (level > 1.0f) {
        level = 1.0f;
    }

    int n = leds_count();
    for (int i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i / (float)(n - 1) : 0.0f;
        uint8_t r, g, b;
        s_color_theme(st->theme, 0.35f + 0.65f * level, &r, &g, &b);
        leds_set(i, (uint8_t)(r * level), (uint8_t)(g * level), (uint8_t)(b * level));
        (void)t;
    }
}

static void s_render_sparkle(const bassline_state_t *st)
{
    static uint8_t s_age[300];
    int n = leds_count();
    if (n > 300) {
        n = 300;
    }

    /* randomly ignite a few LEDs, brightness driven by loudness */
    int sparks = 1 + (int)(st->vu * 6.0f * st->gain);
    for (int s = 0; s < sparks; s++) {
        int idx = (int)(esp_timer_get_time() + s * 97) % (n > 0 ? n : 1);
        if (idx >= 0 && idx < n) {
            s_age[idx] = 30 + (uint8_t)(st->vu * 80.0f);
        }
    }

    for (int i = 0; i < n; i++) {
        if (s_age[i] > 0) {
            s_age[i]--;
            float t = (float)s_age[i] / 100.0f;
            if (t > 1.0f) {
                t = 1.0f;
            }
            uint8_t r, g, b;
            s_color_theme(st->theme, 0.4f + 0.6f * t, &r, &g, &b);
            leds_set(i, r, g, b);
        } else {
            leds_set(i, 0, 0, 0);
        }
    }
}

static void s_render_leds(const bassline_state_t *st)
{
    switch (st->mode) {
    case BASSLINE_MODE_VU:
        s_render_vu(st);
        break;
    case BASSLINE_MODE_BREATHING:
        s_render_breathing(st);
        break;
    case BASSLINE_MODE_SPARKLE:
        s_render_sparkle(st);
        break;
    case BASSLINE_MODE_SPECTRUM:
    default:
        s_render_spectrum(st);
        break;
    }
    leds_show();
}

/* ------------------------------------------------------------------ */
/* WiFi                                                                 */
/* ------------------------------------------------------------------ */

static void s_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA got IP " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

static esp_err_t s_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, s_wifi_event, NULL));

    wifi_config_t ap_cfg = { 0 };
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "bassline");
    snprintf((char *)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), "bassline123");
    ap_cfg.ap.ssid_len = strlen("bassline");
    ap_cfg.ap.channel = 6;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

    wifi_config_t sta_cfg = { 0 };
    strncpy((char *)sta_cfg.sta.ssid, CONFIG_BASSLINE_WIFI_SSID, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, CONFIG_BASSLINE_WIFI_PASS, sizeof(sta_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi up — SoftAP SSID \"bassline\" / pass \"bassline123\"");
    if (strlen(CONFIG_BASSLINE_WIFI_SSID) > 0) {
        ESP_LOGI(TAG, "STA trying \"%s\"", CONFIG_BASSLINE_WIFI_SSID);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Tasks                                                                */
/* ------------------------------------------------------------------ */

static void s_audio_task(void *arg)
{
    ESP_LOGI(TAG, "audio task up (FFT=%d, bins=%d, rate=%d Hz)",
             FFT_N, LOG_BINS, audio_in_sample_rate());

    float vu_smooth = 0.0f;

    while (1) {
        if (audio_in_read(s_window, FFT_N) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        fft_compute(s_window, s_mags);
        fft_log_bins(s_mags, FFT_N / 2, (float)audio_in_sample_rate(),
                     s_bins, LOG_BINS, 55.0f, 8000.0f);

        bassline_state_t st;
        web_server_get_state(&st);

        /* normalise: log compression + gain */
        float energy = 0.0f;
        float peak = 1e-6f;
        for (int i = 0; i < LOG_BINS; i++) {
            if (s_bins[i] > peak) {
                peak = s_bins[i];
            }
        }
        for (int i = 0; i < LOG_BINS; i++) {
            float v = s_bins[i] / (peak + 1e-6f);
            /* gentle log curve keeps quiet passages visible */
            v = log10f(1.0f + v * 20.0f) / log10f(21.0f);
            v *= st.gain;
            if (v > 1.0f) {
                v = 1.0f;
            }
            s_norm[i] = v;
            energy += s_bins[i];
        }

        float vu = energy / ((float)LOG_BINS * (peak + 1e-6f));
        vu = log10f(1.0f + vu * 15.0f) / log10f(16.0f);
        vu *= st.gain;
        if (vu > 1.0f) {
            vu = 1.0f;
        }
        vu_smooth = vu_smooth * 0.7f + vu * 0.3f;

        web_server_update_bins(s_norm, LOG_BINS, vu_smooth);
    }
}

static void s_visual_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(LED_UPDATE_MS);
    TickType_t last = xTaskGetTickCount();

    while (1) {
        bassline_state_t st;
        web_server_get_state(&st);
        s_render_leds(&st);
        web_server_broadcast();

        vTaskDelayUntil(&last, period);
    }
}

/* ------------------------------------------------------------------ */
/* Entry                                                                */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "bassline starting — audio-reactive spectrum visualiser");

    fft_init(FFT_N);
    ESP_ERROR_CHECK(leds_init());
    ESP_ERROR_CHECK(audio_in_init());
    ESP_ERROR_CHECK(s_wifi_init());
    ESP_ERROR_CHECK(web_server_start());

    xTaskCreatePinnedToCore(s_audio_task, "audio", 8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(s_visual_task, "visual", 8192, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "dashboard: http://192.168.4.1/  (SoftAP) or the STA IP");
}
