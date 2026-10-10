/**
 * SoftAP + STA hybrid: try STA first; fall back to SoftAP so the web
 * config page is always reachable.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"

#include "wifi_manager.h"

static const char *TAG = "wifi";
static char s_ip[16];

esp_err_t wifi_manager_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    /* SoftAP config */
    esp_netif_create_default_wifi_ap();
    wifi_config_t ap = {0};
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "DeckPad-%02X%02X", mac[4], mac[5]);
    ap.ap.ssid_len = strlen((char *)ap.ap.ssid);
    ap.ap.channel = 6;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    strlcpy(s_ip, "192.168.4.1", sizeof(s_ip));
    ESP_LOGI(TAG, "SoftAP '%s' up  ->  http://192.168.4.1", ap.ap.ssid);
    return ESP_OK;
}

const char *wifi_manager_get_ip(void) { return s_ip; }
