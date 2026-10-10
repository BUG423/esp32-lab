/**
 * HTTP + WebSocket server
 *
 * Serves the embedded dashboard and pushes CSI frames as JSON over WS.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "web_server.h"
#include "csi_processor.h"
#include "wifi_manager.h"

static const char *TAG = "web";

/* Embedded by CMake EMBED_FILES */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;

/* ---- helpers ---- */

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t err = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out);
    return err;
}

static cJSON *frame_to_json(void)
{
    csi_frame_t f;
    csi_processor_get_frame(&f);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "csi");
    cJSON_AddNumberToObject(root, "ts", (double)f.ts_us / 1e6);
    cJSON_AddNumberToObject(root, "samples", f.sample_count);
    cJSON_AddNumberToObject(root, "energy", f.motion_energy);
    cJSON_AddNumberToObject(root, "baseline", f.baseline);
    cJSON_AddNumberToObject(root, "score", f.presence_score);
    cJSON_AddBoolToObject(root, "present", f.present);
    cJSON_AddNumberToObject(root, "breath", f.breath_bpm);

    cJSON *amp = cJSON_CreateArray();
    cJSON *vari = cJSON_CreateArray();
    for (int i = 0; i < CSI_MAX_SUBCARRIERS; i++) {
        cJSON_AddItemToArray(amp, cJSON_CreateNumber(f.subcarrier_amp[i]));
        cJSON_AddItemToArray(vari, cJSON_CreateNumber(f.subcarrier_var[i]));
    }
    cJSON_AddItemToObject(root, "amp", amp);
    cJSON_AddItemToObject(root, "var", vari);
    return root;
}

/* ---- handlers ---- */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = frame_to_json();
    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip());
    return send_json(req, root);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WS handshake");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { 0 };
    /* probe length */
    esp_err_t err = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (err != ESP_OK) {
        return err;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        return ESP_OK;
    }

    /* if client sent a command */
    if (ws_pkt.len > 0) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        ws_pkt.payload = buf;
        err = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (err == ESP_OK && ws_pkt.payload) {
            /* handle {"cmd":"recalibrate"} etc. */
            cJSON *cmd = cJSON_Parse((char *)buf);
            if (cmd) {
                cJSON *c = cJSON_GetObjectItem(cmd, "cmd");
                if (c && cJSON_IsString(c)) {
                    if (strcmp(c->valuestring, "recalibrate") == 0) {
                        csi_processor_recalibrate();
                    }
                }
                cJSON_Delete(cmd);
            }
        }
        free(buf);
    }

    /* send current frame back */
    cJSON *root = frame_to_json();
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return ESP_ERR_NO_MEM;

    httpd_ws_frame_t tx = {
        .payload = (uint8_t *)out,
        .len = strlen(out),
        .type = HTTPD_WS_TYPE_TEXT,
        .final = true,
    };
    err = httpd_ws_send_frame(req, &tx);
    free(out);
    return err;
}

void web_server_broadcast_csi(void)
{
    if (!s_server) return;

    cJSON *root = frame_to_json();
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return;

    size_t clients = 8;
    int fds[8];
    if (httpd_get_client_list(s_server, &clients, fds) == ESP_OK) {
        for (size_t i = 0; i < clients; i++) {
            if (httpd_ws_get_fd_info(s_server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                httpd_ws_frame_t tx = {
                    .payload = (uint8_t *)out,
                    .len = strlen(out),
                    .type = HTTPD_WS_TYPE_TEXT,
                    .final = true,
                };
                httpd_ws_send_frame_async(s_server, fds[i], &tx);
            }
        }
    }
    free(out);
}

esp_err_t web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    const httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
    };
    const httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };

    httpd_register_uri_handler(s_server, &root_uri);
    httpd_register_uri_handler(s_server, &status_uri);
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "HTTP server listening on :80  (/, /ws, /api/status)");
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
