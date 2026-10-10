/**
 * bytepet - application entry point
 *
 * 电子宠物复活了 — a Tamagotchi-style virtual pet on ESP32.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "pet.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "button.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== bytepet 电子宠物 ===");

    /* NVS first: pet state lives there */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(pet_init());
    ESP_ERROR_CHECK(button_init());

    ESP_ERROR_CHECK(wifi_manager_start());

    char ip[16];
    wifi_manager_get_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "dashboard: http://%s/", ip);

    ESP_ERROR_CHECK(web_server_start());

    /* app_main returns; everything else runs in FreeRTOS tasks */
}
