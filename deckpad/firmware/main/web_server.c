/**
 * HTTP + WebSocket for macro configuration.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "macros.h"
#include "bt_hid.h"
#include "wifi_manager.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static httpd_handle_t s_srv = NULL;

static cJSON *macros_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    int n = macros_count();
    for (int i = 0; i < n; i++) {
        macro_t m;
        macros_get(i, &m);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "idx", i);
        cJSON_AddStringToObject(o, "name", m.name);
        cJSON_AddNumberToObject(o, "modifier", m.modifier);
        cJSON_AddStringToObject(o, "text", m.text);
        cJSON_AddNumberToObject(o, "consumer", m.consumer);
        cJSON *keys = cJSON_CreateArray();
        for (int k = 0; k < m.nkeys; k++) {
            cJSON_AddItemToArray(keys, cJSON_CreateNumber(m.keys[k]));
        }
        cJSON_AddItemToObject(o, "keys", keys);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json");
    httpd_resp_set_type(req, "application/json");
    esp_err_t e = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out);
    return e;
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

static esp_err_t api_macros(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "macros", macros_json());
    cJSON_AddNumberToObject(root, "layer", macros_get_layer());
    cJSON_AddNumberToObject(root, "layers", MACRO_LAYERS);
    cJSON_AddBoolToObject(root, "connected", bt_hid_is_connected());
    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip());
    return send_json(req, root);
}

static esp_err_t api_layer(httpd_req_t *req)
{
    char buf[64];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        cJSON *j = cJSON_Parse(buf);
        if (j) {
            cJSON *L = cJSON_GetObjectItem(j, "layer");
            if (L && cJSON_IsNumber(L)) macros_set_layer(L->valueint);
            cJSON_Delete(j);
        }
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddNumberToObject(root, "layer", macros_get_layer());
    return send_json(req, root);
}

static esp_err_t api_run(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body");
    buf[n] = 0;
    cJSON *j = cJSON_Parse(buf);
    int idx = 0;
    if (j) {
        cJSON *i = cJSON_GetObjectItem(j, "idx");
        if (i && cJSON_IsNumber(i)) idx = i->valueint;
        cJSON_Delete(j);
    }
    esp_err_t err = macros_run(idx);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", err == ESP_OK);
    return send_json(req, root);
}

static esp_err_t api_set(httpd_req_t *req)
{
    char buf[512];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body");
    buf[n] = 0;
    cJSON *j = cJSON_Parse(buf);
    if (!j) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "json");

    macro_t m = {0};
    int idx = 0;
    cJSON *t;
    if ((t = cJSON_GetObjectItem(j, "idx")) && cJSON_IsNumber(t)) idx = t->valueint;
    if ((t = cJSON_GetObjectItem(j, "name")) && cJSON_IsString(t))
        strlcpy(m.name, t->valuestring, sizeof(m.name));
    if ((t = cJSON_GetObjectItem(j, "modifier")) && cJSON_IsNumber(t))
        m.modifier = (uint8_t)t->valueint;
    if ((t = cJSON_GetObjectItem(j, "consumer")) && cJSON_IsNumber(t))
        m.consumer = (uint16_t)t->valueint;
    if ((t = cJSON_GetObjectItem(j, "text")) && cJSON_IsString(t))
        strlcpy(m.text, t->valuestring, sizeof(m.text));
    if ((t = cJSON_GetObjectItem(j, "keys")) && cJSON_IsArray(t)) {
        int c = cJSON_GetArraySize(t);
        for (int i = 0; i < c && i < MACRO_KEYS_MAX; i++) {
            cJSON *k = cJSON_GetArrayItem(t, i);
            if (k && cJSON_IsNumber(k)) m.keys[m.nkeys++] = (uint8_t)k->valueint;
        }
    }
    cJSON_Delete(j);
    macros_set(idx, &m);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    return send_json(req, root);
}

esp_err_t web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&s_srv, &cfg));

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t macros = {.uri = "/api/macros", .method = HTTP_GET, .handler = api_macros};
    const httpd_uri_t run = {.uri = "/api/run", .method = HTTP_POST, .handler = api_run};
    const httpd_uri_t set = {.uri = "/api/set", .method = HTTP_POST, .handler = api_set};
    const httpd_uri_t layer = {.uri = "/api/layer", .method = HTTP_POST, .handler = api_layer};

    httpd_register_uri_handler(s_srv, &root);
    httpd_register_uri_handler(s_srv, &macros);
    httpd_register_uri_handler(s_srv, &run);
    httpd_register_uri_handler(s_srv, &set);
    httpd_register_uri_handler(s_srv, &layer);

    ESP_LOGI(TAG, "config UI on http://%s/", wifi_manager_get_ip());
    return ESP_OK;
}