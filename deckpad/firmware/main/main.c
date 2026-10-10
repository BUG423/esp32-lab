/**
 * DeckPad — BLE HID macro keyboard.
 *
 * Buttons send configurable keyboard shortcuts to a paired phone/tablet/laptop.
 * A SoftAP web UI lets you edit the macros at runtime.
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "bt_hid.h"
#include "macros.h"
#include "buttons.h"
#include "wifi_manager.h"

static const char *TAG = "main";

extern esp_err_t web_server_start(void);

void app_main(void)
{
    ESP_LOGI(TAG, "============================");
    ESP_LOGI(TAG, "  DeckPad — BLE macro pad");
    ESP_LOGI(TAG, "============================");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(macros_init());
    ESP_ERROR_CHECK(bt_hid_start(CONFIG_DECKPAD_DEVICE_NAME));
    ESP_ERROR_CHECK(buttons_init());
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "Pair '%s' over BLE. Edit macros at http://192.168.4.1",
             CONFIG_DECKPAD_DEVICE_NAME);
}
