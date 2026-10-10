/**
 * HTTP + WebSocket for the link-quality dashboard and message console.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "optical.h"
#include "protocol.h"
#include "pairing.h"
#include "wifi_manager.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static httpd_handle_t s_srv = NULL;

/* message log for the UI */
#define LOG_MAX 32
static char s_log[LOG_MAX][160];
static int s_log_n = 0;

void web_log_msg(const char *dir, const char *text)
{
    int i = s_log_n % LOG_MAX;
    snprintf(s_log[i], sizeof(s_log[i]), "%s %s", dir, text);
    s_log_n++;
}

static cJSON *stats_json(void)
{
    optical_stats_t o;
    optical_get_stats(&o);
    proto_stats_t p;
    protocol_get_stats(&p);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "snr_db", o.snr_db);
    cJSON_AddNumberToObject(root, "signal", o.signal);
    cJSON_AddNumberToObject(root, "noise", o.noise_floor);
    cJSON_AddNumberToObject(root, "quality", o.rssi_like);
    cJSON_AddNumberToObject(root, "tx", (double)p.tx_frames);
    cJSON_AddNumberToObject(root, "rx", (double)p.rx_frames);
    cJSON_AddNumberToObject(root, "err", (double)p.rx_errors);
    cJSON_AddStringToObject(root, "last_rx", p.last_rx_text);

    cJSON *wave = cJSON_CreateArray();
    for (int i = 0; i < o.count; i++) {
        cJSON_AddItemToArray(wave, cJSON_CreateNumber(o.level[i]));
    }
    cJSON_AddItemToObject(root, "wave", wave);

    cJSON *logj = cJSON_CreateArray();
    int start = s_log_n > LOG_MAX ? s_log_n - LOG_MAX : 0;
    for (int i = start; i < s_log_n; i++) {
        cJSON_AddItemToArray(logj, cJSON_CreateString(s_log[i % LOG_MAX]));
    }
    cJSON_AddItemToObject(root, "log", logj);

    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip());

    const char *pair_state = "idle";
    switch (pairing_get_state()) {
    case PAIR_WAITING: pair_state = "waiting"; break;
    case PAIR_PAIRED:  pair_state = "paired";  break;
    default:           pair_state = "idle";    break;
    }
    cJSON_AddStringToObject(root, "pair", pair_state);
    cJSON_AddBoolToObject(root, "encrypted", pairing_is_paired());
    return root;
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t e = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out);
    return e;
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

static esp_err_t api_stats(httpd_req_t *req)
{
    return send_json(req, stats_json());
}

static esp_err_t api_send(httpd_req_t *req)
{
    char buf[256];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body");
    buf[n] = 0;

    cJSON *j = cJSON_Parse(buf);
    const char *text = "";
    if (j) {
        cJSON *t = cJSON_GetObjectItem(j, "text");
        if (t && cJSON_IsString(t)) text = t->valuestring;
    }

    web_log_msg("TX", text);
    int sent = protocol_send_text(text);
    if (j) cJSON_Delete(j);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", sent > 0);
    cJSON_AddNumberToObject(root, "bytes", sent);
    cJSON_AddBoolToObject(root, "encrypted", pairing_is_paired());
    return send_json(req, root);
}

static esp_err_t api_pair(httpd_req_t *req)
{
    esp_err_t e = pairing_start();
    const char *msg = (e == ESP_OK) ? "offer sent" : "send failed";
    if (e == ESP_OK) web_log_msg("PAIR", msg);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", e == ESP_OK);
    cJSON_AddStringToObject(root, "msg", msg);
    return send_json(req, root);
}

static esp_err_t api_unpair(httpd_req_t *req)
{
    pairing_unpair();
    web_log_msg("PAIR", "unpaired");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddStringToObject(root, "msg", "unpaired");
    return send_json(req, root);
}

static esp_err_t ws_hdl(httpd_req_t *req)
{
    if (req->method == HTTP_GET) return ESP_OK;

    httpd_ws_frame_t f = {0};
    esp_err_t err = httpd_ws_recv_frame(req, &f, 0);
    if (err != ESP_OK) return err;
    if (f.type == HTTPD_WS_TYPE_CLOSE) return ESP_OK;

    if (f.len > 0) {
        uint8_t *b = calloc(1, f.len + 1);
        if (!b) return ESP_ERR_NO_MEM;
        f.payload = b;
        httpd_ws_recv_frame(req, &f, f.len);
        cJSON *j = cJSON_Parse((char *)b);
        if (j) {
            cJSON *cmd = cJSON_GetObjectItem(j, "cmd");
            if (cmd && cJSON_IsString(cmd)) {
                if (strcmp(cmd->valuestring, "send") == 0) {
                    cJSON *t = cJSON_GetObjectItem(j, "text");
                    const char *text = (t && cJSON_IsString(t)) ? t->valuestring : "";
                    web_log_msg("TX", text);
                    protocol_send_text(text);
                } else if (strcmp(cmd->valuestring, "calibrate") == 0) {
                    optical_calibrate();
                }
            }
            cJSON_Delete(j);
        }
        free(b);
    }

    /* push stats */
    cJSON *root = stats_json();
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (out) {
        httpd_ws_frame_t tx = {.payload = (uint8_t *)out, .len = strlen(out),
                               .type = HTTPD_WS_TYPE_TEXT, .final = true};
        httpd_ws_send_frame(req, &tx);
        free(out);
    }
    return ESP_OK;
}

static void broadcast_task(void *arg)
{
    while (1) {
        if (!s_srv) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        cJSON *root = stats_json();
        char *out = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (out) {
            size_t n = 8;
            int fds[8];
            if (httpd_get_client_list(s_srv, &n, fds) == ESP_OK) {
                for (size_t i = 0; i < n; i++) {
                    if (httpd_ws_get_fd_info(s_srv, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                        httpd_ws_frame_t tx = {.payload = (uint8_t *)out, .len = strlen(out),
                                               .type = HTTPD_WS_TYPE_TEXT, .final = true};
                        httpd_ws_send_frame_async(s_srv, fds[i], &tx);
                    }
                }
            }
            free(out);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;

    ESP_ERROR_CHECK(httpd_start(&s_srv, &cfg));

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t stats = {.uri = "/api/stats", .method = HTTP_GET, .handler = api_stats};
    const httpd_uri_t send = {.uri = "/api/send", .method = HTTP_POST, .handler = api_send};
    const httpd_uri_t pair = {.uri = "/api/pair", .method = HTTP_POST, .handler = api_pair};
    const httpd_uri_t unpair = {.uri = "/api/unpair", .method = HTTP_POST, .handler = api_unpair};
    const httpd_uri_t ws = {.uri = "/ws", .method = HTTP_GET, .handler = ws_hdl,
                            .is_websocket = true, .handle_ws_control_frames = true};

    httpd_register_uri_handler(s_srv, &root);
    httpd_register_uri_handler(s_srv, &stats);
    httpd_register_uri_handler(s_srv, &send);
    httpd_register_uri_handler(s_srv, &pair);
    httpd_register_uri_handler(s_srv, &unpair);
    httpd_register_uri_handler(s_srv, &ws);

    xTaskCreate(broadcast_task, "ws_bcast", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "dashboard on http://%s/", wifi_manager_get_ip());
    return ESP_OK;
}
