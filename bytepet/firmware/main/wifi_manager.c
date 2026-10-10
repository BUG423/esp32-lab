/**
 * bytepet - WiFi station manager implementation
 */
#include "wifi_manager.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     20

static EventGroupHandle_t s_wifi_events;
static int s_retry_count;
static char s_ip_str[16];

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(TAG, "retry connect (%d)", s_retry_count);
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip: %s", s_ip_str);
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_start_sta(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, CONFIG_BYTEPET_WIFI_SSID,
            sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, CONFIG_BYTEPET_WIFI_PASS,
            sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to SSID '%s'", CONFIG_BYTEPET_WIFI_SSID);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "STA connect failed, falling back to soft-AP");
    esp_wifi_stop();
    return ESP_FAIL;
}

static esp_err_t wifi_start_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.ap.ssid, "bytepet-setup",
            sizeof(wifi_cfg.ap.ssid) - 1);
    wifi_cfg.ap.ssid_len = (uint8_t)strlen("bytepet-setup");
    strncpy((char *)wifi_cfg.ap.password, "bytepet123",
            sizeof(wifi_cfg.ap.password) - 1);
    wifi_cfg.ap.channel = 1;
    wifi_cfg.ap.max_connection = 4;
    wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* soft-AP always has 192.168.4.1 */
    snprintf(s_ip_str, sizeof(s_ip_str), "192.168.4.1");
    ESP_LOGI(TAG, "soft-AP started: bytepet-setup / bytepet123");
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    s_wifi_events = xEventGroupCreate();
    if (s_wifi_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const char *ssid = CONFIG_BYTEPET_WIFI_SSID;
    if (ssid != NULL && strlen(ssid) > 0) {
        esp_netif_create_default_wifi_sta();
        if (wifi_start_sta() == ESP_OK) {
            return ESP_OK;
        }
        /* tear down before soft-AP */
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
        esp_netif_create_default_wifi_ap();
        return wifi_start_softap();
    }

    esp_netif_create_default_wifi_ap();
    return wifi_start_softap();
}

int wifi_manager_get_ip(char *buf, size_t buflen)
{
    if (buf == NULL || buflen == 0) {
        return 0;
    }
    strncpy(buf, s_ip_str, buflen - 1);
    buf[buflen - 1] = '\0';
    return (int)strlen(buf);
}
