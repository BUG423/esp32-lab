/**
 * ESP-NOW broadcast chat core.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "espnow_chat.h"

static const char *TAG = "espnow";

#define CHAT_WIRE_LEN      240
#define CHAT_DEDUP_SLOTS   64
#define CHAT_RATE_LIMIT_MS 500
#define CHAT_LED_PULSE_MS  60

/* magic u16 | ver u8 | type u8 | msg_id u32 | nick[32] | text[200] */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  type;
    uint32_t msg_id;
    char     nick[CHAT_NICK_MAX];
    char     text[CHAT_TEXT_MAX];
} chat_frame_t;

_Static_assert(sizeof(chat_frame_t) == CHAT_WIRE_LEN, "chat_frame_t must be 240 bytes");

typedef struct {
    uint8_t  mac[6];
    int8_t   rssi;
    uint16_t len;
    uint8_t  data[CHAT_WIRE_LEN];
} rx_item_t;

static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static QueueHandle_t s_rxq = NULL;
static chat_notify_fn s_notify = NULL;

static chat_msg_t  s_msgs[CHAT_MSG_MAX];
static int         s_msg_head = 0;      /* next write slot */
static int         s_msg_count = 0;

static chat_peer_t s_peers[CHAT_PEER_MAX];

static uint32_t s_msg_id;
static uint32_t s_tx_count;
static uint32_t s_rx_count;
static uint32_t s_dup_count;
static uint32_t s_drop_count;
static int64_t  s_last_tx_ms;

static char s_nick[CHAT_NICK_MAX + 1];

static uint32_t s_dedup[CHAT_DEDUP_SLOTS];
static int      s_dedup_pos;

static esp_timer_handle_t s_led_timer;

/* ------------------------------------------------------------------ */
/* LED activity pulse                                                  */
/* ------------------------------------------------------------------ */

static void led_off_cb(void *arg)
{
    gpio_set_level(CONFIG_AIRCHAT_LED_GPIO, 0);
}

static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_AIRCHAT_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(CONFIG_AIRCHAT_LED_GPIO, 0);

    const esp_timer_create_args_t targs = {
        .callback = led_off_cb,
        .name = "led_off",
    };
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_led_timer));
}

static void led_pulse(void)
{
    gpio_set_level(CONFIG_AIRCHAT_LED_GPIO, 1);
    esp_timer_stop(s_led_timer);
    esp_timer_start_once(s_led_timer, CHAT_LED_PULSE_MS * 1000);
}

/* ------------------------------------------------------------------ */
/* Storage helpers                                                     */
/* ------------------------------------------------------------------ */

static bool dedup_seen(uint32_t id)
{
    for (int i = 0; i < CHAT_DEDUP_SLOTS; i++) {
        if (s_dedup[i] == id) {
            return true;
        }
    }
    return false;
}

static void dedup_add(uint32_t id)
{
    s_dedup[s_dedup_pos] = id;
    s_dedup_pos = (s_dedup_pos + 1) % CHAT_DEDUP_SLOTS;
}

static void history_save(void);
static void history_load(void);

static void store_msg(const chat_msg_t *m)
{
    s_msgs[s_msg_head] = *m;
    s_msg_head = (s_msg_head + 1) % CHAT_MSG_MAX;
    if (s_msg_count < CHAT_MSG_MAX) {
        s_msg_count++;
    }
    history_save();
}

/* ---- NVS history persistence ---- */

#define HIST_NS  "airchat"
#define HIST_KEY "msgs"

static void history_save(void)
{
    nvs_handle_t h;
    if (nvs_open(HIST_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, HIST_KEY, s_msgs, sizeof(s_msgs));
    nvs_commit(h);
    nvs_close(h);
}

static void history_load(void)
{
    nvs_handle_t h;
    if (nvs_open(HIST_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_msgs);
    if (nvs_get_blob(h, HIST_KEY, s_msgs, &len) == ESP_OK && len == sizeof(s_msgs)) {
        s_msg_count = 0;
        for (int i = 0; i < CHAT_MSG_MAX; i++) {
            if (s_msgs[i].id != 0 || s_msgs[i].text[0]) s_msg_count++;
        }
        s_msg_head = s_msg_count % CHAT_MSG_MAX;
        ESP_LOGI(TAG, "restored %d messages from NVS", s_msg_count);
    }
    nvs_close(h);
}

static chat_peer_t *peer_find(const uint8_t mac[6])
{
    for (int i = 0; i < CHAT_PEER_MAX; i++) {
        if (s_peers[i].mac[0] | s_peers[i].mac[1] | s_peers[i].mac[2] |
            s_peers[i].mac[3] | s_peers[i].mac[4] | s_peers[i].mac[5]) {
            if (memcmp(s_peers[i].mac, mac, 6) == 0) {
                return &s_peers[i];
            }
        }
    }
    return NULL;
}

static chat_peer_t *peer_alloc(const uint8_t mac[6])
{
    chat_peer_t *free_slot = NULL;
    for (int i = 0; i < CHAT_PEER_MAX; i++) {
        bool occupied = s_peers[i].mac[0] | s_peers[i].mac[1] | s_peers[i].mac[2] |
                        s_peers[i].mac[3] | s_peers[i].mac[4] | s_peers[i].mac[5];
        if (!occupied) {
            free_slot = &s_peers[i];
            break;
        }
    }
    if (!free_slot) {
        /* Evict the oldest. */
        free_slot = &s_peers[0];
        for (int i = 1; i < CHAT_PEER_MAX; i++) {
            if (s_peers[i].last_seen_ms < free_slot->last_seen_ms) {
                free_slot = &s_peers[i];
            }
        }
    }
    memset(free_slot, 0, sizeof(*free_slot));
    memcpy(free_slot->mac, mac, 6);
    return free_slot;
}

static void peer_touch(const uint8_t mac[6], const char *nick, int8_t rssi)
{
    chat_peer_t *p = peer_find(mac);
    if (!p) {
        p = peer_alloc(mac);
        ESP_LOGI(TAG, "new peer %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    if (nick && nick[0]) {
        strlcpy(p->nick, nick, sizeof(p->nick));
    }
    p->rssi = rssi;
    p->msg_count++;
    p->last_seen_ms = esp_timer_get_time() / 1000;
}

/* ------------------------------------------------------------------ */
/* ESP-NOW callbacks                                                   */
/* ------------------------------------------------------------------ */

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "TX failed");
    }
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (info == NULL || data == NULL || len <= 0) {
        return;
    }
    rx_item_t item = {0};
    memcpy(item.mac, info->src_addr, 6);
    item.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    item.len = (uint16_t)((len > CHAT_WIRE_LEN) ? CHAT_WIRE_LEN : len);
    memcpy(item.data, data, item.len);
    if (xQueueSend(s_rxq, &item, 0) != pdTRUE) {
        s_drop_count++;
    }
}

/* ------------------------------------------------------------------ */
/* RX processing task                                                  */
/* ------------------------------------------------------------------ */

static void rx_task(void *arg)
{
    rx_item_t item;
    for (;;) {
        if (xQueueReceive(s_rxq, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (item.len < sizeof(chat_frame_t)) {
            s_drop_count++;
            continue;
        }

        chat_frame_t frame;
        memcpy(&frame, item.data, sizeof(frame));

        if (frame.magic != CHAT_MSG_MAGIC || frame.version != CHAT_MSG_VERSION ||
            frame.type != CHAT_MSG_TYPE_TEXT) {
            s_drop_count++;
            continue;
        }

        frame.nick[CHAT_NICK_MAX - 1] = '\0';
        frame.text[CHAT_TEXT_MAX - 1] = '\0';

        /* Drop frames echoed back from ourselves. */
        uint8_t my_mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, my_mac);
        if (memcmp(item.mac, my_mac, 6) == 0) {
            continue;
        }

        if (dedup_seen(frame.msg_id)) {
            s_dup_count++;
            continue;
        }
        dedup_add(frame.msg_id);

        s_rx_count++;
        peer_touch(item.mac, frame.nick, item.rssi);
        led_pulse();

        chat_msg_t m = {0};
        m.id = frame.msg_id;
        strlcpy(m.nick, frame.nick, sizeof(m.nick));
        strlcpy(m.text, frame.text, sizeof(m.text));
        m.rssi = item.rssi;
        memcpy(m.mac, item.mac, 6);
        m.self = false;
        m.ts_ms = esp_timer_get_time() / 1000;
        store_msg(&m);

        ESP_LOGI(TAG, "RX [%d dBm] %s: %s", item.rssi, m.nick, m.text);

        if (s_notify) {
            s_notify();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t espnow_chat_init(void)
{
    strlcpy(s_nick, CONFIG_AIRCHAT_NICKNAME, sizeof(s_nick));
    s_msg_id = esp_random();

    history_load();
    led_init();

    s_rxq = xQueueCreate(16, sizeof(rx_item_t));
    if (!s_rxq) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    xTaskCreate(rx_task, "chat_rx", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "ESP-NOW chat ready, nick='%s'", s_nick);
    return ESP_OK;
}

esp_err_t espnow_chat_send(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_last_tx_ms < CHAT_RATE_LIMIT_MS) {
        ESP_LOGW(TAG, "rate limited");
        return ESP_ERR_INVALID_STATE;
    }

    chat_frame_t frame = {0};
    frame.magic = CHAT_MSG_MAGIC;
    frame.version = CHAT_MSG_VERSION;
    frame.type = CHAT_MSG_TYPE_TEXT;
    frame.msg_id = ++s_msg_id;
    strlcpy(frame.nick, s_nick, sizeof(frame.nick));
    strlcpy(frame.text, text, sizeof(frame.text));

    esp_err_t err = esp_now_send(s_broadcast_mac, (uint8_t *)&frame, sizeof(frame));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_send: %s", esp_err_to_name(err));
        return err;
    }

    s_last_tx_ms = now;
    s_tx_count++;
    dedup_add(frame.msg_id);
    led_pulse();

    chat_msg_t m = {0};
    m.id = frame.msg_id;
    strlcpy(m.nick, frame.nick, sizeof(m.nick));
    strlcpy(m.text, frame.text, sizeof(m.text));
    m.rssi = 0;
    esp_wifi_get_mac(WIFI_IF_STA, m.mac);
    m.self = true;
    m.ts_ms = now;
    store_msg(&m);

    ESP_LOGI(TAG, "TX: %s", frame.text);

    if (s_notify) {
        s_notify();
    }
    return ESP_OK;
}

void espnow_chat_set_notify_cb(chat_notify_fn cb)
{
    s_notify = cb;
}

int espnow_chat_get_messages(chat_msg_t *out, int max)
{
    int n = (s_msg_count < max) ? s_msg_count : max;
    int start = (s_msg_head - n + CHAT_MSG_MAX * 2) % CHAT_MSG_MAX;
    for (int i = 0; i < n; i++) {
        out[i] = s_msgs[(start + i) % CHAT_MSG_MAX];
    }
    return n;
}

int espnow_chat_get_peers(chat_peer_t *out, int max)
{
    int n = 0;
    for (int i = 0; i < CHAT_PEER_MAX && n < max; i++) {
        bool occupied = s_peers[i].mac[0] | s_peers[i].mac[1] | s_peers[i].mac[2] |
                        s_peers[i].mac[3] | s_peers[i].mac[4] | s_peers[i].mac[5];
        if (occupied) {
            out[n++] = s_peers[i];
        }
    }
    return n;
}

void espnow_chat_get_stats(uint32_t *tx, uint32_t *rx, uint32_t *dup, uint32_t *drops)
{
    if (tx) *tx = s_tx_count;
    if (rx) *rx = s_rx_count;
    if (dup) *dup = s_dup_count;
    if (drops) *drops = s_drop_count;
}

const char *espnow_chat_get_nick(void)
{
    return s_nick;
}