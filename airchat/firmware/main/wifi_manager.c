/**
 * WiFi manager — SoftAP + idle STA for ESP-NOW.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "wifi_manager.h"

static const char *TAG = "wifi";

static esp_netif_t *s_ap_netif = NULL;
static char s_ip[16] = "192.168.4.1";
static char s_ssid[24] = "airchat";
static uint8_t s_mac[6];

esp_err_t wifi_manager_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /* MAC is needed for the unique SSID before start. */
    ESP_ERROR_CHECK(esp_read_mac(s_mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(s_ssid, sizeof(s_ssid), "airchat-%02X%02X", s_mac[4], s_mac[5]);

    wifi_config_t ap = {0};
    strlcpy((char *)ap.ap.ssid, s_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = (uint8_t)strlen(s_ssid);
    ap.ap.channel = CONFIG_AIRCHAT_CHANNEL;
    ap.ap.max_connection = 8;
    ap.ap.beacon_interval = 100;

    if (CONFIG_AIRCHAT_AP_PASSWORD[0] != '\0') {
        strlcpy((char *)ap.ap.password, CONFIG_AIRCHAT_AP_PASSWORD, sizeof(ap.ap.password));
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    /* APSTA: SoftAP serves the dashboard, STA stays idle for ESP-NOW. */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Pin the shared radio channel (AP config also sets it). */
    esp_err_t err = esp_wifi_set_channel(CONFIG_AIRCHAT_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_channel returned %s (AP config already pins it)", esp_err_to_name(err));
    }

    esp_netif_ip_info_t info;
    if (s_ap_netif && esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK) {
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&info.ip));
    }

    ESP_LOGI(TAG, "SoftAP '%s' up on channel %d, ip=%s", s_ssid, CONFIG_AIRCHAT_CHANNEL, s_ip);
    return ESP_OK;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip;
}

const char *wifi_manager_get_ssid(void)
{
    return s_ssid;
}

int wifi_manager_get_channel(void)
{
    return CONFIG_AIRCHAT_CHANNEL;
}

void wifi_manager_get_mac(uint8_t mac[6])
{
    memcpy(mac, s_mac, 6);
}
