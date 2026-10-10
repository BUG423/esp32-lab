/**
 * Home Assistant MQTT discovery for the CSI radar.
 *
 * Uses the built-in esp-mqtt client. Discovery payloads follow
 * https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "sdkconfig.h"

#include "ha_mqtt.h"
#include "wifi_manager.h"

static const char *TAG = "ha_mqtt";

#if CONFIG_CSI_RADAR_MQTT_ENABLE

#define TOPIC_MAX 128

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;
static bool s_discovered = false;

static const char *PREFIX = CONFIG_CSI_RADAR_MQTT_TOPIC_PREFIX;
static const char *NODE_ID = "csi_radar";

static void topic(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, cap, fmt, ap);
    va_end(ap);
}

static void pub(const char *t, const char *data, int retain)
{
    if (!s_client) return;
    esp_mqtt_client_publish(s_client, t, data, 0, 1 /* QoS */, retain);
}

/* ---- discovery ---- */

static void publish_discovery(void)
{
    char t[TOPIC_MAX];
    char unique[64];
    const char *dev_id = CONFIG_CSI_RADAR_HOSTNAME;

    /* helper: build a fresh device block for each config */
    #define ADD_DEV(c) do { \
        cJSON *d = cJSON_CreateObject(); \
        cJSON_AddStringToObject(d, "ids", dev_id); \
        cJSON_AddStringToObject(d, "name", "ESP CSI Radar"); \
        cJSON_AddStringToObject(d, "mf", "ESP32 Lab"); \
        cJSON_AddStringToObject(d, "mdl", "CSI presence radar"); \
        cJSON_AddStringToObject(d, "sw", "1.0"); \
        cJSON_AddItemToObject(c, "dev", d); \
    } while (0)

    /* binary_sensor: occupancy */
    {
        topic(t, sizeof(t), "homeassistant/binary_sensor/%s/presence/config", NODE_ID);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "name", "Presence");
        topic(unique, sizeof(unique), "%s_presence", dev_id);
        cJSON_AddStringToObject(c, "uniq_id", unique);
        topic(unique, sizeof(unique), "%s/state", PREFIX);
        cJSON_AddStringToObject(c, "stat_t", unique);
        cJSON_AddStringToObject(c, "pl_on", "occupied");
        cJSON_AddStringToObject(c, "pl_off", "empty");
        cJSON_AddStringToObject(c, "dev_cla", "occupancy");
        topic(unique, sizeof(unique), "%s/status", PREFIX);
        cJSON_AddStringToObject(c, "avty_t", unique);
        cJSON_AddStringToObject(c, "pl_avail", "online");
        cJSON_AddStringToObject(c, "pl_not_avail", "offline");
        ADD_DEV(c);
        char *s = cJSON_PrintUnformatted(c);
        pub(t, s, 1);
        free(s);
        cJSON_Delete(c);
    }

    /* sensor: motion energy */
    {
        topic(t, sizeof(t), "homeassistant/sensor/%s/energy/config", NODE_ID);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "name", "Motion Energy");
        topic(unique, sizeof(unique), "%s_energy", dev_id);
        cJSON_AddStringToObject(c, "uniq_id", unique);
        topic(unique, sizeof(unique), "%s/state", PREFIX);
        cJSON_AddStringToObject(c, "stat_t", unique);
        cJSON_AddStringToObject(c, "val_tpl", "{{ value_json.energy }}");
        topic(unique, sizeof(unique), "%s/status", PREFIX);
        cJSON_AddStringToObject(c, "avty_t", unique);
        cJSON_AddStringToObject(c, "pl_avail", "online");
        cJSON_AddStringToObject(c, "pl_not_avail", "offline");
        ADD_DEV(c);
        char *s = cJSON_PrintUnformatted(c);
        pub(t, s, 1);
        free(s);
        cJSON_Delete(c);
    }

    /* sensor: presence score */
    {
        topic(t, sizeof(t), "homeassistant/sensor/%s/score/config", NODE_ID);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "name", "Presence Score");
        topic(unique, sizeof(unique), "%s_score", dev_id);
        cJSON_AddStringToObject(c, "uniq_id", unique);
        topic(unique, sizeof(unique), "%s/state", PREFIX);
        cJSON_AddStringToObject(c, "stat_t", unique);
        cJSON_AddStringToObject(c, "val_tpl", "{{ value_json.score }}");
        cJSON_AddNumberToObject(c, "min", 0);
        cJSON_AddNumberToObject(c, "max", 1);
        topic(unique, sizeof(unique), "%s/status", PREFIX);
        cJSON_AddStringToObject(c, "avty_t", unique);
        cJSON_AddStringToObject(c, "pl_avail", "online");
        cJSON_AddStringToObject(c, "pl_not_avail", "offline");
        ADD_DEV(c);
        char *s = cJSON_PrintUnformatted(c);
        pub(t, s, 1);
        free(s);
        cJSON_Delete(c);
    }

    /* sensor: breathing rate */
    {
        topic(t, sizeof(t), "homeassistant/sensor/%s/breath/config", NODE_ID);
        cJSON *c = cJSON_CreateObject();
        cJSON_AddStringToObject(c, "name", "Breathing Rate");
        topic(unique, sizeof(unique), "%s_breath", dev_id);
        cJSON_AddStringToObject(c, "uniq_id", unique);
        topic(unique, sizeof(unique), "%s/state", PREFIX);
        cJSON_AddStringToObject(c, "stat_t", unique);
        cJSON_AddStringToObject(c, "val_tpl", "{{ value_json.breath }}");
        cJSON_AddStringToObject(c, "unit_of_meas", "bpm");
        topic(unique, sizeof(unique), "%s/status", PREFIX);
        cJSON_AddStringToObject(c, "avty_t", unique);
        cJSON_AddStringToObject(c, "pl_avail", "online");
        cJSON_AddStringToObject(c, "pl_not_avail", "offline");
        ADD_DEV(c);
        char *s = cJSON_PrintUnformatted(c);
        pub(t, s, 1);
        free(s);
        cJSON_Delete(c);
    }

    s_discovered = true;
    ESP_LOGI(TAG, "HA discovery published");
}

/* ---- state ---- */

void ha_mqtt_publish_frame(const csi_frame_t *f)
{
    if (!s_connected || !f) return;
    if (!s_discovered) publish_discovery();

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "presence", f->present ? "occupied" : "empty");
    cJSON_AddNumberToObject(j, "energy", f->motion_energy);
    cJSON_AddNumberToObject(j, "score", f->presence_score);
    cJSON_AddNumberToObject(j, "breath", f->breath_bpm);
    cJSON_AddNumberToObject(j, "rssi", 0); /* filled below if available */
    cJSON_AddNumberToObject(j, "samples", f->sample_count);

    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!s) return;

    char t[TOPIC_MAX];
    topic(t, sizeof(t), "%s/state", PREFIX);
    pub(t, s, 0);
    free(s);
}

bool ha_mqtt_is_connected(void)
{
    return s_connected;
}

/* ---- event handlers ---- */

static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
    switch (ev->event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT connected");
        publish_discovery();
        {
            char t[TOPIC_MAX];
            topic(t, sizeof(t), "%s/status", PREFIX);
            pub(t, "online", 1);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

esp_err_t ha_mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_CSI_RADAR_MQTT_URI,
    };
    cfg.credentials.username = CONFIG_CSI_RADAR_MQTT_USERNAME;
    cfg.credentials.authentication.password = CONFIG_CSI_RADAR_MQTT_PASSWORD;

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) return ESP_FAIL;
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t e = esp_mqtt_client_start(s_client);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "mqtt start failed: %s", esp_err_to_name(e));
    }
    return e;
}

#else /* !CONFIG_CSI_RADAR_MQTT_ENABLE */

esp_err_t ha_mqtt_start(void)
{
    ESP_LOGI(TAG, "MQTT disabled (enable in menuconfig → CSI Radar Configuration)");
    return ESP_OK;
}

void ha_mqtt_publish_frame(const csi_frame_t *f) { (void)f; }
bool ha_mqtt_is_connected(void) { return false; }

#endif
