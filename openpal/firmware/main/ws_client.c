/** OpenPal protocol-v2 WebSocket client. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ws_client.h"

#define JSON_MAX_BYTES 4096

static const char *TAG = "ws";
static esp_websocket_client_handle_t s_client;
static ws_client_event_cb_t s_callback;
static void *s_callback_ctx;
static SemaphoreHandle_t s_send_mutex;
static volatile bool s_connected;
static volatile bool s_in_reply;
static char *s_text_buf;
static int s_text_len;

static void emit(const ws_client_event_t *event)
{
    if (s_callback) s_callback(event, s_callback_ctx);
}

static const char *json_string(cJSON *root, const char *key, const char *fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) ? item->valuestring : fallback;
}

static int json_int(cJSON *root, const char *key, int fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static void handle_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "invalid JSON control frame");
        return;
    }

    const char *type = json_string(root, "type", "");
    ws_client_event_t event = {0};

    if (strcmp(type, "status") == 0) {
        event.type = WS_CLIENT_EVENT_STATUS;
        event.state = json_string(root, "state", "");
        emit(&event);
    } else if (strcmp(type, "reply_start") == 0) {
        event.type = WS_CLIENT_EVENT_REPLY_START;
        event.text = json_string(root, "text", "");
        event.sample_rate = json_int(root, "sample_rate", 24000);
        event.audio_bytes = json_int(root, "audio_bytes", 0);
        s_in_reply = event.audio_bytes > 0;
        emit(&event);
    } else if (strcmp(type, "reply_end") == 0) {
        event.type = WS_CLIENT_EVENT_REPLY_END;
        event.audio_bytes = json_int(root, "audio_bytes", 0);
        s_in_reply = false;
        emit(&event);
    } else if (strcmp(type, "error") == 0) {
        event.type = WS_CLIENT_EVENT_ERROR;
        event.text = json_string(root, "message", json_string(root, "code", "server error"));
        emit(&event);
    } else if (strcmp(type, "device_cmd") == 0 &&
               strcmp(json_string(root, "cmd", ""), "led") == 0) {
        cJSON *on = cJSON_GetObjectItemCaseSensitive(root, "on");
        event.type = WS_CLIENT_EVENT_LED;
        event.led_on = cJSON_IsBool(on) && cJSON_IsTrue(on);
        emit(&event);
    } else if (strcmp(type, "interrupted") == 0) {
        s_in_reply = false;
    } else if (strcmp(type, "hello") == 0) {
        int protocol = json_int(root, "protocol", 0);
        if (protocol != 2) {
            ESP_LOGE(TAG, "server protocol %d is not supported", protocol);
        }
    }

    cJSON_Delete(root);
}

static void handle_text_fragment(esp_websocket_event_data_t *data)
{
    if (data->payload_len <= 0 || data->payload_len > JSON_MAX_BYTES ||
        data->payload_offset < 0 || data->data_len < 0 ||
        data->payload_offset + data->data_len > data->payload_len) {
        ESP_LOGW(TAG, "reject control frame (%d bytes)", data->payload_len);
        free(s_text_buf);
        s_text_buf = NULL;
        s_text_len = 0;
        return;
    }

    if (data->payload_offset == 0) {
        free(s_text_buf);
        s_text_buf = calloc(1, data->payload_len + 1);
        s_text_len = data->payload_len;
    }
    if (!s_text_buf || s_text_len != data->payload_len) return;

    memcpy(s_text_buf + data->payload_offset, data->data_ptr, data->data_len);
    if (data->payload_offset + data->data_len == data->payload_len) {
        handle_json(s_text_buf);
        free(s_text_buf);
        s_text_buf = NULL;
        s_text_len = 0;
    }
}

static void ws_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    ws_client_event_t event = {0};

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        event.type = WS_CLIENT_EVENT_CONNECTED;
        emit(&event);
        ws_client_send_text("{\"type\":\"hello\",\"protocol\":2,\"client\":\"esp32\"}");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        s_in_reply = false;
        free(s_text_buf);
        s_text_buf = NULL;
        s_text_len = 0;
        ESP_LOGW(TAG, "disconnected (close=%d)", data ? data->close_status_code : 0);
        event.type = WS_CLIENT_EVENT_DISCONNECTED;
        emit(&event);
        break;

    case WEBSOCKET_EVENT_DATA:
        if (!data || data->data_len <= 0) break;
        if (data->op_code == 0x1 || (data->op_code == 0x0 && s_text_buf)) {
            handle_text_fragment(data);
        } else if ((data->op_code == 0x2 || data->op_code == 0x0) && s_in_reply) {
            event.type = WS_CLIENT_EVENT_AUDIO;
            event.data = (const uint8_t *)data->data_ptr;
            event.data_len = data->data_len;
            emit(&event);
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "transport error");
        event.type = WS_CLIENT_EVENT_ERROR;
        event.text = "WebSocket transport error";
        emit(&event);
        break;

    default:
        break;
    }
}

esp_err_t ws_client_start(const char *uri, ws_client_event_cb_t callback, void *ctx)
{
    if (!uri || !callback) return ESP_ERR_INVALID_ARG;
    s_callback = callback;
    s_callback_ctx = ctx;
    s_send_mutex = xSemaphoreCreateMutex();
    if (!s_send_mutex) return ESP_ERR_NO_MEM;

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .user_agent = "OpenPal-ESP32/0.2",
        .task_stack = 6144,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .reconnect_timeout_ms = 3000,
        .network_timeout_ms = 10000,
        .ping_interval_sec = 10,
        .pingpong_timeout_sec = 20,
        .keep_alive_enable = true,
        .keep_alive_idle = 15,
        .keep_alive_interval = 5,
        .keep_alive_count = 3,
    };

    s_client = esp_websocket_client_init(&cfg);
    if (!s_client) return ESP_FAIL;
    ESP_RETURN_ON_ERROR(
        esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL),
        TAG, "register events");
    return esp_websocket_client_start(s_client);
}

bool ws_client_is_connected(void)
{
    return s_connected;
}

esp_err_t ws_client_send_text(const char *json)
{
    if (!s_client || !s_connected || !json) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(2500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    int sent = esp_websocket_client_send_text(
        s_client, json, strlen(json), pdMS_TO_TICKS(2000));
    xSemaphoreGive(s_send_mutex);
    return sent >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_client_send_binary(const uint8_t *data, size_t len)
{
    if (!s_client || !s_connected || !data || len == 0) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(2500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    int sent = esp_websocket_client_send_bin(
        s_client, (const char *)data, len, pdMS_TO_TICKS(2000));
    xSemaphoreGive(s_send_mutex);
    return sent >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_client_send_audio_start(int sample_rate)
{
    char json[112];
    snprintf(json, sizeof(json),
             "{\"type\":\"audio_start\",\"format\":\"pcm_s16le\",\"sample_rate\":%d}",
             sample_rate);
    return ws_client_send_text(json);
}

esp_err_t ws_client_send_audio_end(void)
{
    return ws_client_send_text("{\"type\":\"audio_end\"}");
}

esp_err_t ws_client_send_interrupt(void)
{
    s_in_reply = false;
    return ws_client_send_text("{\"type\":\"interrupt\"}");
}

esp_err_t ws_client_send_playback_done(void)
{
    return ws_client_send_text("{\"type\":\"playback_done\"}");
}
