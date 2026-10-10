/**
 * HTTP + WebSocket server for the off-grid chat dashboard.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "web_server.h"
#include "espnow_chat.h"
#include "wifi_manager.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/* CONFIG_HTTPD_MAX_OPEN_SOCKETS does not exist; size locally. */
#define WEB_MAX_CLIENTS  6
#define WEB_MAX_WS       6

static httpd_handle_t s_srv = NULL;
static SemaphoreHandle_t s_lock = NULL;

static int s_fds[WEB_MAX_WS];
static int s_fd_count = 0;

/* ------------------------------------------------------------------ */
/* JSON state                                                          */
/* ------------------------------------------------------------------ */

static void mac_str(const uint8_t mac[6], char *out, size_t n)
{
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static cJSON *msg_to_json(const chat_msg_t *m)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", m->id);
    cJSON_AddStringToObject(o, "nick", m->nick);
    cJSON_AddStringToObject(o, "text", m->text);
    cJSON_AddNumberToObject(o, "rssi", m->rssi);
    char mac[18];
    mac_str(m->mac, mac, sizeof(mac));
    cJSON_AddStringToObject(o, "mac", mac);
    cJSON_AddBoolToObject(o, "self", m->self);
    cJSON_AddNumberToObject(o, "ts", (double)m->ts_ms);
    return o;
}

static cJSON *state_json(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "nick", espnow_chat_get_nick());
    cJSON_AddStringToObject(root, "ssid", wifi_manager_get_ssid());
    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip());
    cJSON_AddNumberToObject(root, "channel", wifi_manager_get_channel());

    uint8_t mac[6];
    wifi_manager_get_mac(mac);
    char macs[18];
    mac_str(mac, macs, sizeof(macs));
    cJSON_AddStringToObject(root, "mac", macs);

    uint32_t tx, rx, dup, drops;
    espnow_chat_get_stats(&tx, &rx, &dup, &drops);
    cJSON_AddNumberToObject(root, "tx", tx);
    cJSON_AddNumberToObject(root, "rx", rx);
    cJSON_AddNumberToObject(root, "dup", dup);
    cJSON_AddNumberToObject(root, "drops", drops);

    chat_msg_t msgs[CHAT_MSG_MAX];
    int n = espnow_chat_get_messages(msgs, CHAT_MSG_MAX);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON_AddItemToArray(arr, msg_to_json(&msgs[i]));
    }
    cJSON_AddItemToObject(root, "messages", arr);

    chat_peer_t peers[CHAT_PEER_MAX];
    int pn = espnow_chat_get_peers(peers, CHAT_PEER_MAX);
    cJSON *parr = cJSON_CreateArray();
    for (int i = 0; i < pn; i++) {
        cJSON *p = cJSON_CreateObject();
        char pm[18];
        mac_str(peers[i].mac, pm, sizeof(pm));
        cJSON_AddStringToObject(p, "mac", pm);
        cJSON_AddStringToObject(p, "nick", peers[i].nick);
        cJSON_AddNumberToObject(p, "rssi", peers[i].rssi);
        cJSON_AddNumberToObject(p, "count", peers[i].msg_count);
        cJSON_AddNumberToObject(p, "last_seen", (double)peers[i].last_seen_ms);
        cJSON_AddItemToArray(parr, p);
    }
    cJSON_AddItemToObject(root, "peers", parr);

    return root;
}

/* ------------------------------------------------------------------ */
/* WebSocket client registry                                           */
/* ------------------------------------------------------------------ */

static void ws_add(int fd)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_fd_count < WEB_MAX_WS) {
        s_fds[s_fd_count++] = fd;
    }
    xSemaphoreGive(s_lock);
}

static void ws_remove(int fd)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < s_fd_count; i++) {
        if (s_fds[i] == fd) {
            s_fds[i] = s_fds[--s_fd_count];
            break;
        }
    }
    xSemaphoreGive(s_lock);
}

static void ws_send_json(int fd, cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);
    if (!out) {
        return;
    }
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)out,
        .len = strlen(out),
    };
    esp_err_t err = httpd_ws_send_frame_async(s_srv, fd, &frame);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "ws send to fd %d: %s", fd, esp_err_to_name(err));
        ws_remove(fd);
    }
    cJSON_free(out);
}

void web_server_broadcast(void)
{
    if (!s_srv) {
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "state");
    cJSON *st = state_json();
    cJSON_AddItemToObject(root, "data", st);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_fd_count;
    int fds[WEB_MAX_WS];
    memcpy(fds, s_fds, sizeof(int) * n);
    xSemaphoreGive(s_lock);

    for (int i = 0; i < n; i++) {
        ws_send_json(fds[i], root);
    }
    cJSON_Delete(root);
}

/* ------------------------------------------------------------------ */
/* Handlers                                                            */
/* ------------------------------------------------------------------ */

static esp_err_t handle_index(httpd_req_t *req)
{
    size_t len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

static esp_err_t handle_state(httpd_req_t *req)
{
    cJSON *root = state_json();
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t err = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    cJSON_free(out);
    return err;
}

static esp_err_t handle_send(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > CHAT_TEXT_MAX + 64) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    char buf[CHAT_TEXT_MAX + 65] = {0};
    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
        }
        received += r;
    }
    buf[received] = '\0';

    /* Accept raw text or {"text":"..."}. */
    char text[CHAT_TEXT_MAX + 1];
    strlcpy(text, buf, sizeof(text));
    if (buf[0] == '{') {
        cJSON *root = cJSON_Parse(buf);
        cJSON *t = root ? cJSON_GetObjectItem(root, "text") : NULL;
        if (cJSON_IsString(t) && t->valuestring) {
            strlcpy(text, t->valuestring, sizeof(text));
        }
        if (root) {
            cJSON_Delete(root);
        }
    }

    esp_err_t err = espnow_chat_send(text);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", err == ESP_OK);
    if (err != ESP_OK) {
        cJSON_AddStringToObject(resp, "error", esp_err_to_name(err));
    }
    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t r = httpd_resp_send(req, out ? out : "{\"ok\":false}", HTTPD_RESP_USE_STRLEN);
    if (out) {
        cJSON_free(out);
    }
    return r;
}

static esp_err_t handle_ws(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        ws_add(fd);
        ESP_LOGI(TAG, "ws client fd=%d", fd);
        /* Push full state on connect. */
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "state");
        cJSON_AddItemToObject(root, "data", state_json());
        ws_send_json(fd, root);
        cJSON_Delete(root);
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {0};
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) {
        return err;
    }
    if (frame.len == 0) {
        return ESP_OK;
    }

    uint8_t *buf = malloc(frame.len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = buf;
    err = httpd_ws_recv_frame(req, &frame, frame.len + 1);
    if (err != ESP_OK) {
        free(buf);
        return err;
    }
    buf[frame.len] = '\0';

    cJSON *root = cJSON_Parse((char *)buf);
    free(buf);
    if (root) {
        cJSON *type = cJSON_GetObjectItem(root, "type");
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (cJSON_IsString(type) && strcmp(type->valuestring, "send") == 0 &&
            cJSON_IsString(text) && text->valuestring) {
            espnow_chat_send(text->valuestring);
        }
        cJSON_Delete(root);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Server start                                                        */
/* ------------------------------------------------------------------ */

esp_err_t web_server_start(void)
{
    s_lock = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = WEB_MAX_CLIENTS;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    ESP_ERROR_CHECK(httpd_start(&s_srv, &config));

    const httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_index,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_srv, &uri_index);

    const httpd_uri_t uri_state = {
        .uri = "/api/state",
        .method = HTTP_GET,
        .handler = handle_state,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_srv, &uri_state);

    const httpd_uri_t uri_send = {
        .uri = "/api/send",
        .method = HTTP_POST,
        .handler = handle_send,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_srv, &uri_send);

    const httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    httpd_register_uri_handler(s_srv, &uri_ws);

    ESP_LOGI(TAG, "web dashboard on http://%s/", wifi_manager_get_ip());
    return ESP_OK;
}
