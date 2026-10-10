/**
 * OpenPal firmware — push-to-talk AI voice companion.
 *
 * Flow:
 *   1. Connect WiFi
 *   2. Connect WebSocket to OpenPal server
 *   3. Hold PTT button -> capture I2S mic -> send PCM
 *   4. Release -> audio_end -> server replies with text + TTS PCM
 *   5. Play TTS on I2S speaker, LED shows state
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "audio.h"
#include "ws_client.h"

static const char *TAG = "main";

extern esp_err_t audio_start_tasks(void);

#define PTT_GPIO  CONFIG_OPENPAL_PTT_GPIO
#define LED_GPIO  CONFIG_OPENPAL_LED_GPIO
#define CAP_MAX   (CONFIG_OPENPAL_SAMPLE_RATE * 2 * 15)   /* 15 s of PCM16 mono */

static uint8_t *s_cap_buf = NULL;
static volatile bool s_ptt_down = false;

/* ---- LED ---- */
static void led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(LED_GPIO, 0);
}

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

/* ---- WS reply: play audio ---- */
static void on_reply(const char *text, const uint8_t *audio, int audio_len, int sample_rate)
{
    if (text && text[0]) {
        ESP_LOGI(TAG, "PAL: %s", text);
    }
    if (audio && audio_len > 0) {
        led_set(true);
        audio_play_pcm(audio, audio_len, sample_rate);
        led_set(false);
    }
    /* tell server playback finished */
    ws_client_send_text("{\"type\":\"playback_done\"}");
}

/* ---- PTT task ---- */
static void ptt_task(void *arg)
{
    bool last = false;
    while (1) {
        bool pressed = gpio_get_level(PTT_GPIO) == 0; /* active low */
        if (pressed && !last) {
            /* down */
            s_ptt_down = true;
            led_set(true);
            audio_capture_start();
            ESP_LOGI(TAG, "PTT down — listening");
        } else if (!pressed && last) {
            /* up */
            s_ptt_down = false;
            int n = audio_capture_stop(s_cap_buf, CAP_MAX);
            led_set(false);
            if (n > 0 && ws_client_is_connected()) {
                ESP_LOGI(TAG, "sending %d bytes", n);
                ws_client_send_binary(s_cap_buf, n);
                ws_client_send_audio_end();
            } else {
                ESP_LOGW(TAG, "nothing captured or not connected");
            }
        }
        last = pressed;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==============================");
    ESP_LOGI(TAG, "  OpenPal firmware");
    ESP_LOGI(TAG, "==============================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();

    /* PTT button */
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << PTT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    s_cap_buf = malloc(CAP_MAX);
    if (!s_cap_buf) {
        ESP_LOGE(TAG, "oom");
        return;
    }

    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "WiFi OK  ip=%s", wifi_manager_get_ip());

    ESP_ERROR_CHECK(audio_init());
    ESP_ERROR_CHECK(audio_start_tasks());

    ESP_LOGI(TAG, "connecting to %s", CONFIG_OPENPAL_SERVER_URI);
    ESP_ERROR_CHECK(ws_client_start(CONFIG_OPENPAL_SERVER_URI, on_reply));

    xTaskCreate(ptt_task, "ptt", 4096, NULL, 6, NULL);

    ESP_LOGI(TAG, "Ready. Hold GPIO%d to talk.", PTT_GPIO);
}
