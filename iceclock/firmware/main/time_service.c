#include "time_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ds3231.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define WIFI_CONNECTED_BIT BIT0
#define TIME_SYNCED_BIT     BIT1

static const char *TAG = "time_service";
static EventGroupHandle_t s_events;
static volatile bool s_wifi_connected;
static volatile bool s_time_synchronized;
static volatile bool s_write_rtc_pending;
static volatile bool s_rtc_time_valid;
static bool s_sntp_started;
static bool s_wifi_configured;

static int month_number(const char *month)
{
    static const char *MONTHS[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int i = 0; i < 12; ++i) {
        if (strncmp(month, MONTHS[i], 3) == 0) {
            return i;
        }
    }
    return 0;
}

static void set_build_time_fallback(void)
{
    char month[4] = {0};
    int day = 1;
    int year = 2026;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3 ||
        sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
        return;
    }

    struct tm build_time = {
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month_number(month),
        .tm_year = year - 1900,
        .tm_isdst = -1,
    };
    time_t epoch = mktime(&build_time);
    struct timeval value = {.tv_sec = epoch};
    settimeofday(&value, NULL);
    ESP_LOGW(TAG, "using firmware build time until RTC/NTP is available");
}

static void restore_initial_time(void)
{
    struct tm rtc_time;
    if (ds3231_read_time(&rtc_time) == ESP_OK) {
        time_t epoch = mktime(&rtc_time);
        struct timeval value = {.tv_sec = epoch};
        settimeofday(&value, NULL);
        s_rtc_time_valid = true;
        ESP_LOGI(TAG, "system time restored from DS3231");
        return;
    }
    set_build_time_fallback();
}

static void sync_callback(struct timeval *value)
{
    (void)value;
    s_time_synchronized = true;
    s_write_rtc_pending = true;
    xEventGroupSetBits(s_events, TIME_SYNCED_BIT);
    ESP_LOGI(TAG, "NTP synchronization complete");
}

static void start_sntp(void)
{
    if (s_sntp_started) {
        return;
    }
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_ICECLOCK_NTP_SERVER);
    config.sync_cb = sync_callback;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK) {
        s_sntp_started = true;
        ESP_LOGI(TAG, "SNTP started: %s", CONFIG_ICECLOCK_NTP_SERVER);
    } else {
        ESP_LOGW(TAG, "SNTP start failed: %s", esp_err_to_name(err));
    }
}

static void network_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected; retrying");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_wifi_connected = true;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected: " IPSTR, IP2STR(&event->ip_info.ip));
        start_sntp();
    }
}

static esp_err_t start_wifi(void)
{
    if (CONFIG_ICECLOCK_WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "WiFi SSID is empty; running in offline mode");
        return ESP_OK;
    }
    s_wifi_configured = true;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "initialize WiFi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   network_event, NULL),
                        TAG, "register WiFi event");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   network_event, NULL),
                        TAG, "register IP event");

    wifi_config_t configuration = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    strlcpy((char *)configuration.sta.ssid, CONFIG_ICECLOCK_WIFI_SSID,
            sizeof(configuration.sta.ssid));
    strlcpy((char *)configuration.sta.password, CONFIG_ICECLOCK_WIFI_PASSWORD,
            sizeof(configuration.sta.password));

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "set WiFi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &configuration), TAG,
                        "set WiFi configuration");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start WiFi");
    return ESP_OK;
}

static void rtc_update_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (s_write_rtc_pending && ds3231_is_available()) {
            s_write_rtc_pending = false;
            time_t now;
            struct tm local;
            time(&now);
            localtime_r(&now, &local);
            esp_err_t err = ds3231_write_time(&local);
            if (err == ESP_OK) {
                s_rtc_time_valid = true;
                ESP_LOGI(TAG, "DS3231 updated from NTP");
            } else {
                ESP_LOGW(TAG, "DS3231 update failed: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t time_service_init(void)
{
    setenv("TZ", CONFIG_ICECLOCK_TIMEZONE, 1);
    tzset();

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t rtc_result = ds3231_init();
    if (rtc_result != ESP_OK && rtc_result != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "continuing without DS3231");
    }
    restore_initial_time();

    BaseType_t created = xTaskCreate(rtc_update_task, "rtc_update", 3072,
                                     NULL, 4, NULL);
    if (created != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return start_wifi();
}

bool time_service_wait_for_initial_sync(TickType_t timeout)
{
    if (!s_wifi_configured) {
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(s_events, TIME_SYNCED_BIT,
                                           pdFALSE, pdFALSE, timeout);
    return (bits & TIME_SYNCED_BIT) != 0;
}

void time_service_get_local_time(struct tm *time_info)
{
    time_t now;
    time(&now);
    localtime_r(&now, time_info);
}

bool time_service_wifi_connected(void)
{
    return s_wifi_connected;
}

bool time_service_is_synchronized(void)
{
    return s_time_synchronized;
}

bool time_service_has_valid_rtc_time(void)
{
    return s_rtc_time_valid;
}

esp_err_t time_service_read_temperature(float *temperature_c)
{
    return ds3231_read_temperature(temperature_c);
}
