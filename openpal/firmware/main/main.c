/** OpenPal ESP32 firmware — streamed push-to-talk voice companion. */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "audio.h"
#include "wifi_manager.h"
#include "ws_client.h"

static const char *TAG = "main";

#define PTT_GPIO CONFIG_OPENPAL_PTT_GPIO
#define LED_GPIO CONFIG_OPENPAL_LED_GPIO

static volatile bool s_recording;
static volatile bool s_reply_has_audio;

static void led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(LED_GPIO, 0);
}

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

static esp_err_t capture_sink(const uint8_t *pcm, size_t len, void *ctx)
{
    (void)ctx;
    if (!s_recording || !ws_client_is_connected()) return ESP_ERR_INVALID_STATE;
    return ws_client_send_binary(pcm, len);
}

static void playback_done(void *ctx)
{
    (void)ctx;
    s_reply_has_audio = false;
    led_set(false);
    esp_err_t err = ws_client_send_playback_done();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "playback_done: %s", esp_err_to_name(err));
    }
}

static void on_ws_event(const ws_client_event_t *event, void *ctx)
{
    (void)ctx;
    switch (event->type) {
    case WS_CLIENT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "voice server connected");
        led_set(false);
        break;

    case WS_CLIENT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "voice server disconnected");
        s_recording = false;
        audio_capture_stop();
        audio_playback_abort();
        led_set(false);
        break;

    case WS_CLIENT_EVENT_STATUS:
        ESP_LOGI(TAG, "state: %s", event->state ? event->state : "");
        if (event->state && strcmp(event->state, "thinking") == 0) led_set(true);
        if (event->state && strcmp(event->state, "listening") == 0 &&
            !s_recording && !audio_is_playing()) led_set(false);
        break;

    case WS_CLIENT_EVENT_REPLY_START:
        if (event->text && event->text[0]) ESP_LOGI(TAG, "PAL: %s", event->text);
        s_reply_has_audio = event->audio_bytes > 0;
        if (s_reply_has_audio) {
            led_set(true);
            esp_err_t err = audio_playback_start(event->sample_rate);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "playback start: %s", esp_err_to_name(err));
                s_reply_has_audio = false;
            }
        }
        break;

    case WS_CLIENT_EVENT_AUDIO:
        if (s_reply_has_audio) {
            esp_err_t err = audio_playback_write(
                event->data, event->data_len, pdMS_TO_TICKS(2000));
            if (err != ESP_OK) ESP_LOGW(TAG, "playback buffer: %s", esp_err_to_name(err));
        }
        break;

    case WS_CLIENT_EVENT_REPLY_END:
        if (s_reply_has_audio && audio_is_playing()) {
            audio_playback_finish();
        } else {
            playback_done(NULL);
        }
        break;

    case WS_CLIENT_EVENT_ERROR:
        ESP_LOGW(TAG, "server: %s", event->text ? event->text : "error");
        led_set(false);
        break;

    case WS_CLIENT_EVENT_LED:
        led_set(event->led_on);
        break;
    }
}

static void stop_recording(bool send_end)
{
    if (!s_recording) return;
    s_recording = false;
    audio_capture_stop();
    led_set(false);
    if (send_end && ws_client_is_connected()) {
        esp_err_t err = ws_client_send_audio_end();
        if (err != ESP_OK) ESP_LOGW(TAG, "audio_end: %s", esp_err_to_name(err));
    }
}

static void start_recording(void)
{
    if (!ws_client_is_connected()) {
        ESP_LOGW(TAG, "PTT ignored: server is offline");
        return;
    }

    /* Barge-in cancels server work and local playback before the new stream. */
    audio_playback_abort();
    s_reply_has_audio = false;
    ws_client_send_interrupt();
    if (ws_client_send_audio_start(CONFIG_OPENPAL_INPUT_SAMPLE_RATE) != ESP_OK) {
        ESP_LOGW(TAG, "failed to start audio stream");
        return;
    }
    s_recording = true;
    if (audio_capture_start(capture_sink, NULL) != ESP_OK) {
        s_recording = false;
        ws_client_send_audio_end();
        ESP_LOGW(TAG, "microphone is already active");
        return;
    }
    led_set(true);
    ESP_LOGI(TAG, "PTT down — streaming microphone");
}

static void ptt_task(void *arg)
{
    (void)arg;
    bool last_pressed = false;
    int64_t started_us = 0;

    while (true) {
        bool pressed = gpio_get_level(PTT_GPIO) == 0;
        if (pressed && !last_pressed) {
            start_recording();
            if (s_recording) started_us = esp_timer_get_time();
        } else if (!pressed && last_pressed) {
            stop_recording(true);
        }

        if (s_recording &&
            esp_timer_get_time() - started_us >=
                (int64_t)CONFIG_OPENPAL_MAX_UTTERANCE_SECONDS * 1000000LL - 200000LL) {
            ESP_LOGW(TAG, "maximum utterance duration reached");
            stop_recording(true);
        }

        last_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "OpenPal firmware protocol v2");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << PTT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "WiFi OK, IP=%s", wifi_manager_get_ip());

    ESP_ERROR_CHECK(audio_init());
    audio_set_playback_done_callback(playback_done, NULL);

    ESP_LOGI(TAG, "connecting to %s", CONFIG_OPENPAL_SERVER_URI);
    ESP_ERROR_CHECK(ws_client_start(CONFIG_OPENPAL_SERVER_URI, on_ws_event, NULL));

    if (xTaskCreate(ptt_task, "ptt", 4096, NULL, 6, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create PTT task");
        return;
    }
    ESP_LOGI(TAG, "Ready. Hold GPIO%d to talk.", PTT_GPIO);
}
