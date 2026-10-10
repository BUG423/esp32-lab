/**
 * WiFi STA manager.
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
static EventGroupHandle_t s_ev;
static char s_ip[16];

#define BIT_OK   BIT0
#define BIT_FAIL BIT1

static void on_ev(void *a, esp_event_base_t b, int32_t id, void *d)
{
    if (b == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (b == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        static int r = 0;
        if (r++ < 20) esp_wifi_connect();
        else xEventGroupSetBits(s_ev, BIT_FAIL);
    } else if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = d;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_ev, BIT_OK);
    }
}

esp_err_t wifi_manager_start(void)
{
    s_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_ev, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ev, NULL));

    wifi_config_t c = {0};
    strncpy((char *)c.sta.ssid, CONFIG_GLOWLINK_WIFI_SSID, sizeof(c.sta.ssid));
    strncpy((char *)c.sta.password, CONFIG_GLOWLINK_WIFI_PASSWORD, sizeof(c.sta.password));
    c.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &c));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_ev, BIT_OK | BIT_FAIL, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    return (bits & BIT_OK) ? ESP_OK : ESP_FAIL;
}

const char *wifi_manager_get_ip(void) { return s_ip; }
