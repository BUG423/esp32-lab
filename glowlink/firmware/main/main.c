/**
 * GlowLink — LED optical chat.
 *
 * Two nodes, each with one LED:
 *   - LED is driven as a light transmitter (TX)
 *   - LED is read via ADC as a photodiode (RX)
 *
 * This node runs a web dashboard showing link quality + a message console.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_manager.h"
#include "optical.h"
#include "protocol.h"

static const char *TAG = "main";

extern esp_err_t web_server_start(void);
extern void web_log_msg(const char *dir, const char *text);

static void on_text(const char *text, int rssi)
{
    ESP_LOGI(TAG, "RX [%d dB-ish]: %s", rssi, text);
    web_log_msg("RX", text);
}

static void beacon_task(void *arg)
{
    int n = 0;
    char msg[64];
    while (1) {
        snprintf(msg, sizeof(msg), "GLOW#%d from node", n++);
        ESP_LOGI(TAG, "beacon: %s", msg);
        web_log_msg("TX", msg);
        protocol_send_text(msg);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, "  GlowLink — LED optical chat");
    ESP_LOGI(TAG, "============================");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "WiFi OK  ip=%s", wifi_manager_get_ip());

    ESP_ERROR_CHECK(optical_init(CONFIG_GLOWLINK_TX_GPIO,
                                 CONFIG_GLOWLINK_RX_GPIO,
                                 CONFIG_GLOWLINK_BAUD));

    ESP_ERROR_CHECK(protocol_init());
    protocol_set_text_handler(on_text);
    optical_start_rx_task(protocol_on_bytes);

    ESP_ERROR_CHECK(web_server_start());

#if CONFIG_GLOWLINK_IS_SENDER
    xTaskCreate(beacon_task, "beacon", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "role: SENDER (beacon every 5s)");
#else
    ESP_LOGI(TAG, "role: RECEIVER (listening)");
#endif

    ESP_LOGI(TAG, "Point two LEDs at each other. Open the dashboard.");
}
