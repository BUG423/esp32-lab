/**
 * WebSocket client for OpenPal server.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

#include "ws_client.h"

static const char *TAG = "ws";

static esp_websocket_client_handle_t s_client = NULL;
static ws_reply_cb_t s_on_reply = NULL;
static bool s_connected = false;

static void ws_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x1 && data->data_len > 0 && s_on_reply) {
            /* text JSON */
            char *json = calloc(1, data->data_len + 1);
            if (!json) break;
            memcpy(json, data->data_ptr, data->data_len);

            cJSON *root = cJSON_Parse(json);
            if (root) {
                cJSON *type = cJSON_GetObjectItem(root, "type");
                if (type && cJSON_IsString(type)) {
                    if (strcmp(type->valuestring, "reply") == 0) {
                        cJSON *text = cJSON_GetObjectItem(root, "text");
                        cJSON *audio = cJSON_GetObjectItem(root, "audio_b64");
                        cJSON *rate = cJSON_GetObjectItem(root, "sample_rate");
                        const char *t = (text && cJSON_IsString(text)) ? text->valuestring : "";
                        int sr = (rate && cJSON_IsNumber(rate)) ? rate->valueint : 24000;

                        /* decode base64 audio */
                        uint8_t *pcm = NULL;
                        int pcm_len = 0;
                        if (audio && cJSON_IsString(audio) && audio->valuestring[0]) {
                            int blen = strlen(audio->valuestring);
                            pcm = malloc(blen); /* upper bound */
                            if (pcm) {
                                pcm_len = 0;
                                /* simple base64 decode */
                                static const char tbl[] =
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                                int val = 0, valb = -8;
                                for (int i = 0; i < blen; i++) {
                                    char c = audio->valuestring[i];
                                    if (c == '=') break;
                                    const char *p = strchr(tbl, c);
                                    if (!p) continue;
                                    val = (val << 6) + (int)(p - tbl);
                                    valb += 6;
                                    if (valb >= 0) {
                                        pcm[pcm_len++] = (uint8_t)((val >> valb) & 0xFF);
                                        valb -= 8;
                                    }
                                }
                            }
                        }
                        if (s_on_reply) {
                            s_on_reply(t, pcm, pcm_len, sr);
                        }
                        free(pcm);
                    }
                }
                cJSON_Delete(root);
            }
            free(json);
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "error");
        break;

    default:
        break;
    }
}

esp_err_t ws_client_start(const char *uri, ws_reply_cb_t on_reply)
{
    s_on_reply = on_reply;

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
    };

    s_client = esp_websocket_client_init(&cfg);
    if (!s_client) return ESP_FAIL;

    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    esp_err_t err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool ws_client_is_connected(void)
{
    return s_connected;
}

esp_err_t ws_client_send_text(const char *json)
{
    if (!s_client || !s_connected) return ESP_ERR_INVALID_STATE;
    return esp_websocket_client_send_text(s_client, json, strlen(json), pdMS_TO_TICKS(2000)) >= 0
               ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_client_send_binary(const uint8_t *data, int len)
{
    if (!s_client || !s_connected) return ESP_ERR_INVALID_STATE;
    return esp_websocket_client_send_bin(s_client, (const char *)data, len, pdMS_TO_TICKS(2000)) >= 0
               ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_client_send_audio_end(void)
{
    return ws_client_send_text("{\"type\":\"audio_end\"}");
}
