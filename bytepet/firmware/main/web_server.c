/**
 * bytepet - HTTP + WebSocket server implementation
 *
 * Static dashboard is embedded from web/index.html via EMBED_FILES.
 * WebSocket JSON protocol:
 *   server -> client  {"type":"state", "stage":..., "emotion":..., ...}
 *   client -> server  {"action":"feed|play|clean|sleep"}
 */
#include "web_server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "pet.h"
#include "sdkconfig.h"

static const char *TAG = "web_server";

/* local define - do NOT use CONFIG_HTTPD_MAX_OPEN_SOCKETS */
#define WEB_MAX_OPEN_SOCKETS 8
#define WEB_WS_CLIENTS_MAX   4

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");

static httpd_handle_t s_server;
static int s_ws_fds[WEB_WS_CLIENTS_MAX];
static int s_ws_count;

/* ---- JSON helpers ---- */

static void web_build_state_json(char *buf, size_t buflen)
{
    pet_state_t st;
    char event[128];
    pet_get_state(&st);
    pet_get_event_log(event, sizeof(event));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "state");
    cJSON_AddNumberToObject(root, "stage", (double)st.stage);
    cJSON_AddStringToObject(root, "stage_name", pet_stage_name(st.stage));
    cJSON_AddNumberToObject(root, "emotion", (double)st.emotion);
    cJSON_AddStringToObject(root, "emotion_name", pet_emotion_name(st.emotion));
    cJSON_AddStringToObject(root, "emoji", pet_emotion_emoji(st.emotion));
    cJSON_AddStringToObject(root, "color", pet_emotion_color(st.emotion));
    cJSON_AddStringToObject(root, "mood", pet_emotion_mood(st.emotion));
    cJSON_AddNumberToObject(root, "hunger", (double)st.hunger);
    cJSON_AddNumberToObject(root, "happiness", (double)st.happiness);
    cJSON_AddNumberToObject(root, "energy", (double)st.energy);
    cJSON_AddNumberToObject(root, "cleanliness", (double)st.cleanliness);
    cJSON_AddNumberToObject(root, "tick", (double)st.tick);
    cJSON_AddNumberToObject(root, "age_minutes", (double)st.age_minutes);
    cJSON_AddBoolToObject(root, "sleeping", st.sleeping);
    cJSON_AddBoolToObject(root, "alive", st.alive);
    cJSON_AddStringToObject(root, "log", event);

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == NULL) {
        buf[0] = '\0';
        return;
    }
    strncpy(buf, printed, buflen - 1);
    buf[buflen - 1] = '\0';
    cJSON_free(printed);
}

static void web_broadcast_state(void)
{
    if (s_ws_count == 0 || s_server == NULL) {
        return;
    }
    char payload[768];
    web_build_state_json(payload, sizeof(payload));
    if (payload[0] == '\0') {
        return;
    }

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)payload,
        .len = strlen(payload),
    };

    for (int i = 0; i < s_ws_count; i++) {
        httpd_ws_send_frame_async(s_server, s_ws_fds[i], &ws_pkt);
    }
}

static void web_ws_task(void *arg)
{
    (void)arg;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        web_broadcast_state();
    }
}

/* ---- HTTP handlers ---- */

static esp_err_t web_root_get(httpd_req_t *req)
{
    size_t size = (size_t)(index_html_end - index_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)index_html_start, size);
}

static esp_err_t web_state_get(httpd_req_t *req)
{
    char payload[768];
    web_build_state_json(payload, sizeof(payload));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t web_action_post(httpd_req_t *req)
{
    char body[64];
    int len = (int)req->content_len;
    if (len <= 0 || len >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_FAIL;
    }
    int received = httpd_req_recv(req, body, len);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv fail");
        return ESP_FAIL;
    }
    body[received] = '\0';

    /* accept {"action":"feed"} or bare word */
    pet_action_t action = PET_ACTION_MAX;
    if (strstr(body, "feed")) {
        action = PET_ACTION_FEED;
    } else if (strstr(body, "play")) {
        action = PET_ACTION_PLAY;
    } else if (strstr(body, "clean")) {
        action = PET_ACTION_CLEAN;
    } else if (strstr(body, "sleep")) {
        action = PET_ACTION_SLEEP;
    }

    if (action == PET_ACTION_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown action");
        return ESP_FAIL;
    }

    pet_do_action(action);
    web_broadcast_state();

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t web_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* register client */
        int fd = httpd_req_to_sockfd(req);
        bool known = false;
        for (int i = 0; i < s_ws_count; i++) {
            if (s_ws_fds[i] == fd) {
                known = true;
                break;
            }
        }
        if (!known && s_ws_count < WEB_WS_CLIENTS_MAX) {
            s_ws_fds[s_ws_count++] = fd;
            ESP_LOGI(TAG, "ws client connected fd=%d (n=%d)", fd, s_ws_count);
        }
        /* push current state immediately */
        char payload[768];
        web_build_state_json(payload, sizeof(payload));
        if (payload[0] != '\0') {
            httpd_ws_frame_t ws_pkt = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)payload,
                .len = strlen(payload),
            };
            httpd_ws_send_frame(req, &ws_pkt);
        }
        return ESP_OK;
    }

    /* data frame: parse action */
    httpd_ws_frame_t ws_pkt = { 0 };
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* first call with NULL payload to learn length */
    esp_err_t err = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ws recv_frame failed: %s", esp_err_to_name(err));
        return err;
    }
    if (ws_pkt.len == 0 || ws_pkt.len > 127) {
        return ESP_OK;
    }
    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    err = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (err != ESP_OK) {
        free(buf);
        return err;
    }

    if (strstr((char *)buf, "feed")) {
        pet_do_action(PET_ACTION_FEED);
    } else if (strstr((char *)buf, "play")) {
        pet_do_action(PET_ACTION_PLAY);
    } else if (strstr((char *)buf, "clean")) {
        pet_do_action(PET_ACTION_CLEAN);
    } else if (strstr((char *)buf, "sleep")) {
        pet_do_action(PET_ACTION_SLEEP);
    }
    free(buf);

    web_broadcast_state();
    return ESP_OK;
}

/* ---- server start ---- */

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_open_sockets = WEB_MAX_OPEN_SOCKETS;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = web_root_get,
        .user_ctx = NULL,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &uri_root);

    const httpd_uri_t uri_index = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = web_root_get,
        .user_ctx = NULL,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &uri_index);

    const httpd_uri_t uri_state = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = web_state_get,
        .user_ctx = NULL,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &uri_state);

    const httpd_uri_t uri_action = {
        .uri = "/api/action",
        .method = HTTP_POST,
        .handler = web_action_post,
        .user_ctx = NULL,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &uri_action);

    const httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = web_ws_handler,
        .user_ctx = NULL,
        .handle_ws_control_frames = true,
        .is_websocket = true,
        .supported_subprotocol = NULL,
    };
    httpd_register_uri_handler(s_server, &uri_ws);

    /* 1 Hz push task */
    xTaskCreate(web_ws_task, "web_ws", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "web server started on port 80");
    return ESP_OK;
}
