/**
 * ESP CSI Radar — main entry
 *
 * Flow:
 *   1. Connect to WiFi (STA)
 *   2. Start CSI capture (esp_wifi CSI API)
 *   3. Start web dashboard + WebSocket
 *   4. Publish processed frames periodically
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mdns.h"

#include "wifi_manager.h"
#include "csi_processor.h"
#include "ha_mqtt.h"
#include "web_server.h"

static const char *TAG = "main";

/* ------------------------------------------------------------------ */
/* CSI raw callback — feeds the processor                              */
/* ------------------------------------------------------------------ */
static void csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    if (!info || !info->buf || info->len == 0) {
        return;
    }
    int64_t ts_us = esp_timer_get_time();
    csi_processor_feed((const int8_t *)info->buf, info->len,
                       info->rx_ctrl.rssi, ts_us);
}

static esp_err_t csi_capture_start(void)
{
    wifi_csi_config_t cfg = {
        .lltf_en            = true,
        .htltf_en           = true,
        .stbc_htltf2_en     = true,
        .ltf_merge_en       = true,
        .channel_filter_en  = true,
        .manu_scale         = false,
        .shift              = false,
    };

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI(TAG, "CSI capture started");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Publish task — periodically hand frames to the web layer            */
/* ------------------------------------------------------------------ */
static void publish_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(CSI_PUBLISH_PERIOD_MS);
    const TickType_t mqtt_period = pdMS_TO_TICKS(1000); /* MQTT at 1 Hz */
    int mqtt_div = 0;
    for (;;) {
        web_server_broadcast_csi();
        if (++mqtt_div >= 10) {
            mqtt_div = 0;
            csi_frame_t f;
            csi_processor_get_frame(&f);
            ha_mqtt_publish_frame(&f);
        }
        vTaskDelay(period);
    }
}

/* ------------------------------------------------------------------ */
/* mDNS so users can open http://csi-radar.local                       */
/* ------------------------------------------------------------------ */
static void mdns_start(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(CONFIG_CSI_RADAR_HOSTNAME);
    mdns_instance_name_set("ESP CSI Radar");
    mdns_service_add("CSI Radar Web", "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: http://%s.local", CONFIG_CSI_RADAR_HOSTNAME);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP CSI Radar — WiFi presence sensing");
    ESP_LOGI(TAG, "========================================");

    /* NVS is required by WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* WiFi STA */
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "Connected. Dashboard: http://%s / http://%s.local",
             wifi_manager_get_ip(), CONFIG_CSI_RADAR_HOSTNAME);

    /* CSI processing */
    ESP_ERROR_CHECK(csi_processor_init());

    /* CSI hardware capture */
    ESP_ERROR_CHECK(csi_capture_start());

    /* Web dashboard + WebSocket */
    ESP_ERROR_CHECK(web_server_start());

    /* mDNS */
    mdns_start();

    /* Home Assistant MQTT (optional, menuconfig) */
    ha_mqtt_start();

    /* Periodic publisher */
    xTaskCreate(publish_task, "csi_pub", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System up. Waiting for CSI frames...");
}
