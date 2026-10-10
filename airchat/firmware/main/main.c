/**
 * AirChat — ESP-NOW off-grid mesh text messenger.
 *
 * Every node is a standalone ESP32: SoftAP web dashboard + ESP-NOW
 * broadcast chat. No router, no internet, no LoRa hardware.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_manager.h"
#include "espnow_chat.h"
#include "web_server.h"

static const char *TAG = "main";

static void on_chat_change(void)
{
    web_server_broadcast();
}

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, "  AirChat — off-grid messenger");
    ESP_LOGI(TAG, "============================");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "WiFi OK  ssid=%s  ip=%s  ch=%d",
             wifi_manager_get_ssid(), wifi_manager_get_ip(), wifi_manager_get_channel());

    ESP_ERROR_CHECK(espnow_chat_init());
    espnow_chat_set_notify_cb(on_chat_change);

    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "nick='%s' — connect to SoftAP and open http://%s/",
             espnow_chat_get_nick(), wifi_manager_get_ip());
}
