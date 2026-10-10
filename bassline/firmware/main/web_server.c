#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "web";

#define WS_MAX_CLIENTS 8
#define WS_FRAME_MS    33

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server;
static int s_ws_fds[WS_MAX_CLIENTS];
static SemaphoreHandle_t s_lock;
static bassline_state_t s_state;

static void s_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.n_bins = CONFIG_BASSLINE_LOG_BINS;
    if (s_state.n_bins > BASSLINE_MAX_BINS) {
        s_state.n_bins = BASSLINE_MAX_BINS;
    }
    s_state.mode = BASSLINE_MODE_SPECTRUM;
    s_state.theme = BASSLINE_THEME_RAINBOW;
    s_state.gain = 1.0f;
    s_state.decay = 0.85f;
}

void web_server_get_state(bassline_state_t *out)
{
    if (!out || !s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_state;
    xSemaphoreGive(s_lock);
}

void web_server_update_bins(const float *bins, int n_bins, float vu)
{
    if (!s_lock || !bins) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (n_bins > BASSLINE_MAX_BINS) {
        n_bins = BASSLINE_MAX_BINS;
    }
    s_state.n_bins = n_bins;
    float max_b = 0.0f;
    for (int i = 0; i < n_bins; i++) {
        s_state.bins[i] = bins[i];
        if (bins[i] > s_state.peaks[i]) {
            s_state.peaks[i] = bins[i];
        } else {
            s_state.peaks[i] *= s_state.decay;
        }
        if (s_state.peaks[i] > max_b) {
            max_b = s_state.peaks[i];
        }
    }
    s_state.vu = vu;
    if (vu > s_state.vu_peak) {
        s_state.vu_peak = vu;
    } else {
        s_state.vu_peak *= 0.92f;
    }
    xSemaphoreGive(s_lock);
}

static void s_apply_control(cJSON *root)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    cJSON *j;
    if ((j = cJSON_GetObjectItem(root, "gain")) != NULL && cJSON_IsNumber(j)) {
        float g = (float)j->valuedouble;
        if (g < 0.1f) {
            g = 0.1f;
        }
        if (g > 4.0f) {
            g = 4.0f;
        }
        s_state.gain = g;
    }
    if ((j = cJSON_GetObjectItem(root, "decay")) != NULL && cJSON_IsNumber(j)) {
        float d = (float)j->valuedouble;
        if (d < 0.5f) {
            d = 0.5f;
        }
        if (d > 0.98f) {
            d = 0.98f;
        }
        s_state.decay = d;
    }
    if ((j = cJSON_GetObjectItem(root, "theme")) != NULL && cJSON_IsString(j)) {
        if (strcmp(j->valuestring, "rainbow") == 0) {
            s_state.theme = BASSLINE_THEME_RAINBOW;
        } else if (strcmp(j->valuestring, "fire") == 0) {
            s_state.theme = BASSLINE_THEME_FIRE;
        } else if (strcmp(j->valuestring, "ice") == 0) {
            s_state.theme = BASSLINE_THEME_ICE;
        }
    }
    if ((j = cJSON_GetObjectItem(root, "mode")) != NULL && cJSON_IsString(j)) {
        if (strcmp(j->valuestring, "spectrum") == 0) {
            s_state.mode = BASSLINE_MODE_SPECTRUM;
        } else if (strcmp(j->valuestring, "vu") == 0) {
            s_state.mode = BASSLINE_MODE_VU;
        } else if (strcmp(j->valuestring, "breathing") == 0) {
            s_state.mode = BASSLINE_MODE_BREATHING;
        } else if (strcmp(j->valuestring, "sparkle") == 0) {
            s_state.mode = BASSLINE_MODE_SPARKLE;
        }
    }

    ESP_LOGI(TAG, "control: gain=%.2f decay=%.2f theme=%d mode=%d",
             s_state.gain, s_state.decay, s_state.theme, s_state.mode);
    xSemaphoreGive(s_lock);
}

static const char *s_mode_str(bassline_mode_t m)
{
    switch (m) {
    case BASSLINE_MODE_SPECTRUM: return "spectrum";
    case BASSLINE_MODE_VU: return "vu";
    case BASSLINE_MODE_BREATHING: return "breathing";
    case BASSLINE_MODE_SPARKLE: return "sparkle";
    default: return "spectrum";
    }
}

static const char *s_theme_str(bassline_theme_t t)
{
    switch (t) {
    case BASSLINE_THEME_RAINBOW: return "rainbow";
    case BASSLINE_THEME_FIRE: return "fire";
    case BASSLINE_THEME_ICE: return "ice";
    default: return "rainbow";
    }
}

void web_server_broadcast(void)
{
    if (!s_server || !s_lock) {
        return;
    }

    bassline_state_t st;
    web_server_get_state(&st);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }
    cJSON_AddStringToObject(root, "t", "frame");
    cJSON_AddStringToObject(root, "mode", s_mode_str(st.mode));
    cJSON_AddStringToObject(root, "theme", s_theme_str(st.theme));
    cJSON_AddNumberToObject(root, "gain", st.gain);
    cJSON_AddNumberToObject(root, "decay", st.decay);
    cJSON_AddNumberToObject(root, "vu", st.vu);
    cJSON_AddNumberToObject(root, "vu_peak", st.vu_peak);

    cJSON *jbins = cJSON_CreateArray();
    cJSON *jpeaks = cJSON_CreateArray();
    for (int i = 0; i < st.n_bins; i++) {
        cJSON_AddItemToArray(jbins, cJSON_CreateNumber(st.bins[i]));
        cJSON_AddItemToArray(jpeaks, cJSON_CreateNumber(st.peaks[i]));
    }
    cJSON_AddItemToObject(root, "bins", jbins);
    cJSON_AddItemToObject(root, "peaks", jpeaks);

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        return;
    }

    size_t len = strlen(printed);
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)printed,
        .len = len,
    };

    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] < 0) {
            continue;
        }
        esp_err_t err = httpd_ws_send_frame_async(s_server, s_ws_fds[i], &frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ws send fd=%d failed: %s", s_ws_fds[i], esp_err_to_name(err));
            s_ws_fds[i] = -1;
        }
    }
    free(printed);
}

static void s_ws_register(int fd)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            return;
        }
    }
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] < 0) {
            s_ws_fds[i] = fd;
            ESP_LOGI(TAG, "ws client fd=%d registered", fd);
            return;
        }
    }
}

static void s_ws_unregister(int fd)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = -1;
            ESP_LOGI(TAG, "ws client fd=%d gone", fd);
            return;
        }
    }
}

static esp_err_t s_ws_handler(httpd_req_t *req)
{
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "ws handshake fd=%d", fd);
        s_ws_register(fd);
        return ESP_OK;
    }

    httpd_ws_frame_t frame = { 0 };
    frame.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws recv failed: %s", esp_err_to_name(err));
        s_ws_unregister(fd);
        return err;
    }

    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        s_ws_unregister(fd);
        return ESP_OK;
    }
    if (frame.len == 0 || frame.len > 1024) {
        return ESP_OK;
    }

    uint8_t *buf = calloc(1, frame.len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = buf;
    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err == ESP_OK && frame.len > 0) {
        cJSON *root = cJSON_Parse((char *)buf);
        if (root) {
            s_apply_control(root);
            cJSON_Delete(root);
        }
    }
    free(buf);
    return ESP_OK;
}

static esp_err_t s_index_handler(httpd_req_t *req)
{
    size_t len = (size_t)(index_html_end - index_html_start);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

static esp_err_t s_health_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"app\":\"bassline\"}");
}

esp_err_t web_server_start(void)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        s_ws_fds[i] = -1;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    s_state_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = s_index_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &index_uri);

    const httpd_uri_t health_uri = {
        .uri = "/health",
        .method = HTTP_GET,
        .handler = s_health_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &health_uri);

    const httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = s_ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "HTTP+WS server listening on port %d", config.server_port);
    return ESP_OK;
}
