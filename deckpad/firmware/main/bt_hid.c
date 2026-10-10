/**
 * BLE HID keyboard via esp_hid.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

#include "bt_hid.h"

static const char *TAG = "bt_hid";

static esp_hidd_dev_t *s_dev = NULL;
static volatile bool s_connected = false;

/* keyboard (Report ID 1) + consumer control (Report ID 2) */
static const uint8_t keyboard_report_map[] = {
    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07,
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x03,
    0x95, 0x05,
    0x75, 0x01,
    0x05, 0x08,
    0x19, 0x01,
    0x29, 0x05,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x03,
    0x91, 0x03,
    0x95, 0x05,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x65,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x65,
    0x81, 0x00,
    0xC0,

    /* Consumer Control — Report ID 2, 16-bit usage */
    0x05, 0x0C,
    0x09, 0x01,
    0xA1, 0x01,
    0x85, 0x02,
    0x15, 0x00,
    0x26, 0x3C, 0x02,
    0x19, 0x00,
    0x2A, 0x3C, 0x02,
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x00,
    0xC0,
};

static void hid_event_cb(void *args, esp_event_base_t base, int32_t id, void *event)
{
    esp_hidd_event_t event_id = (esp_hidd_event_t)id;
    switch (event_id) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID started");
        break;
    case ESP_HIDD_CONNECT_EVENT:
        s_connected = true;
        ESP_LOGI(TAG, "HID connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        s_connected = false;
        ESP_LOGW(TAG, "HID disconnected");
        break;
    default:
        break;
    }
}

esp_err_t bt_hid_start(const char *device_name)
{
    /* classic BT not needed — BLE only */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    static esp_hid_raw_report_map_t maps[] = {
        {.data = keyboard_report_map, .len = sizeof(keyboard_report_map)},
    };

    esp_hid_device_config_t cfg = {
        .vendor_id = 0x16C0,
        .product_id = 0x05DF,
        .version = 0x0100,
        .device_name = device_name,
        .manufacturer_name = "DeckPad",
        .serial_number = "0001",
        .report_maps = maps,
        .report_maps_len = 1,
    };

    esp_err_t err = esp_hidd_dev_init(&cfg, ESP_HID_TRANSPORT_BLE, hid_event_cb, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hidd init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "BLE HID keyboard '%s' ready", device_name);
    return ESP_OK;
}

bool bt_hid_is_connected(void)
{
    return s_connected && s_dev && esp_hidd_dev_connected(s_dev);
}

esp_err_t bt_hid_send_key(uint8_t modifier, uint8_t key)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[8] = {modifier, 0, key, 0, 0, 0, 0, 0};
    esp_err_t err = esp_hidd_dev_input_set(s_dev, 0, 1, buf, sizeof(buf));
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    memset(buf, 0, sizeof(buf));
    return esp_hidd_dev_input_set(s_dev, 0, 1, buf, sizeof(buf));
}

esp_err_t bt_hid_send_report(const uint8_t report[8])
{
    if (!s_dev || !report) return ESP_ERR_INVALID_STATE;
    uint8_t buf[8];
    memcpy(buf, report, 8);
    esp_err_t err = esp_hidd_dev_input_set(s_dev, 0, 1, buf, sizeof(buf));
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(25));
    memset(buf, 0, sizeof(buf));
    return esp_hidd_dev_input_set(s_dev, 0, 1, buf, sizeof(buf));
}

static void ascii_to_code(uint8_t *mod, uint8_t *key, char c)
{
    *mod = 0;
    if (c >= 'a' && c <= 'z') {
        *key = (uint8_t)(4 + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
        *mod = 0x02; /* left shift */
        *key = (uint8_t)(4 + (c - 'A'));
    } else if (c >= '1' && c <= '9') {
        *key = (uint8_t)(30 + (c - '1'));
    } else if (c == '0') {
        *key = 39;
    } else {
        switch (c) {
        case ' ': *key = 0x2C; break;
        case '\n': *key = 0x28; break;
        case '\t': *key = 0x2B; break;
        case '-': *key = 0x2D; break;
        case '=': *key = 0x2E; break;
        case '[': *key = 0x2F; break;
        case ']': *key = 0x30; break;
        case ';': *key = 0x33; break;
        case '\'': *key = 0x34; break;
        case '`': *key = 0x35; break;
        case ',': *key = 0x36; break;
        case '.': *key = 0x37; break;
        case '/': *key = 0x38; break;
        default: *key = 0; break;
        }
    }
}

esp_err_t bt_hid_send_text(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;
    for (const char *p = text; *p; p++) {
        uint8_t mod, key;
        ascii_to_code(&mod, &key, *p);
        if (key == 0 && *p != '\0') continue;
        esp_err_t err = bt_hid_send_key(mod, key);
        if (err != ESP_OK) return err;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    return ESP_OK;
}

esp_err_t bt_hid_send_consumer(uint16_t usage)
{
    if (!s_dev || usage == 0) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    esp_err_t err = esp_hidd_dev_input_set(s_dev, 0, 2, buf, sizeof(buf));
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(25));
    uint8_t zero[2] = {0, 0};
    return esp_hidd_dev_input_set(s_dev, 0, 2, zero, sizeof(zero));
}