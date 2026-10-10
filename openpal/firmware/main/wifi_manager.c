/**
 * WiFi STA manager (same pattern as esp-csi-radar).
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "wifi_manager.h"

static const char *TAG = "wifi";

static EventGroupHandle_t s_events;
static char s_ip[16];

#define CONNECTED_BIT BIT0
#define FAIL_BIT      BIT1
#define MAX_RETRY     20

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        static int retry = 0;
        if (retry < MAX_RETRY) {
            esp_wifi_connect();
            retry++;
        } else {
            xEventGroupSetBits(s_events, FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "got IP %s", s_ip);
        xEventGroupSetBits(s_events, CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_start(void)
{
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, NULL));

    wifi_config_t cfg = { 0 };
    strncpy((char *)cfg.sta.ssid, CONFIG_OPENPAL_WIFI_SSID, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, CONFIG_OPENPAL_WIFI_PASSWORD, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_events, CONNECTED_BIT | FAIL_BIT,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    return (bits & CONNECTED_BIT) ? ESP_OK : ESP_FAIL;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip;
}
