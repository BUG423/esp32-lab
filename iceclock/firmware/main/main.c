/**
 * IceClock — ESP32-S3 HUB75 clock with NTP + DS3231 holdover.
 */

#include <stdbool.h>
#include <time.h>

#include "clock_face.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub75.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "time_service.h"

static const char *TAG = "iceclock";

static bool is_night_hour(int hour)
{
    const int start = CONFIG_ICECLOCK_NIGHT_START_HOUR;
    const int end = CONFIG_ICECLOCK_NIGHT_END_HOUR;
    if (start == end) {
        return false;
    }
    if (start < end) {
        return hour >= start && hour < end;
    }
    return hour >= start || hour < end;
}

#if CONFIG_ICECLOCK_CHIME_ENABLE
static void chime_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << CONFIG_ICECLOCK_CHIME_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(CONFIG_ICECLOCK_CHIME_GPIO, 0);
}

static void chime_once(void)
{
    gpio_set_level(CONFIG_ICECLOCK_CHIME_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(90));
    gpio_set_level(CONFIG_ICECLOCK_CHIME_GPIO, 0);
}
#else
static void chime_init(void) {}
static void chime_once(void) {}
#endif

static void nvs_init(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  IceClock — HUB75 network clock");
    ESP_LOGI(TAG, "========================================");

    nvs_init();
    ESP_ERROR_CHECK(hub75_init());
    clock_face_render_boot();
    chime_init();

    ESP_ERROR_CHECK(time_service_init());
    bool initial_sync = time_service_wait_for_initial_sync(
        pdMS_TO_TICKS(CONFIG_ICECLOCK_WIFI_CONNECT_TIMEOUT_SEC * 1000));
    ESP_LOGI(TAG, "initial time source: %s",
             initial_sync ? "NTP" :
             (time_service_has_valid_rtc_time() ? "DS3231" :
                                                  "build fallback"));

    int last_brightness = -1;
    int last_chimed_yday = -1;
    int last_chimed_hour = -1;
    int temperature_counter = 0;
    float temperature = 0.0f;
    bool temperature_valid = false;

    for (;;) {
        clock_face_state_t state = {0};
        time_service_get_local_time(&state.local_time);
        state.wifi_connected = time_service_wifi_connected();
        state.time_synchronized = time_service_is_synchronized();
        state.rtc_time_valid = time_service_has_valid_rtc_time();

        if (++temperature_counter >= 100) {
            temperature_counter = 0;
            temperature_valid =
                time_service_read_temperature(&temperature) == ESP_OK;
        }
        state.temperature_valid = temperature_valid;
        state.temperature_c = temperature;

        int brightness = is_night_hour(state.local_time.tm_hour) ?
                         CONFIG_ICECLOCK_NIGHT_BRIGHTNESS :
                         CONFIG_ICECLOCK_DAY_BRIGHTNESS;
        if (brightness != last_brightness) {
            hub75_set_brightness((uint8_t)brightness);
            last_brightness = brightness;
            ESP_LOGI(TAG, "brightness: %d%%", brightness);
        }

        if (state.local_time.tm_min == 0 && state.local_time.tm_sec == 0 &&
            (state.local_time.tm_yday != last_chimed_yday ||
             state.local_time.tm_hour != last_chimed_hour)) {
            chime_once();
            last_chimed_yday = state.local_time.tm_yday;
            last_chimed_hour = state.local_time.tm_hour;
        }

        clock_face_render(&state);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
