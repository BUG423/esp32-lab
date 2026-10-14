#include "hub75.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define PANEL_WIDTH  CONFIG_ICECLOCK_PANEL_WIDTH
#define PANEL_HEIGHT CONFIG_ICECLOCK_PANEL_HEIGHT_PIXELS
#define SCAN_ROWS    (PANEL_HEIGHT / 2)

static const char *TAG = "hub75";

static hub75_color_t s_frames[2][PANEL_HEIGHT][PANEL_WIDTH];
static volatile int s_front_index;
static volatile int s_back_index = 1;
static volatile bool s_swap_pending;
static volatile uint8_t s_brightness = CONFIG_ICECLOCK_DAY_BRIGHTNESS;
static portMUX_TYPE s_frame_lock = portMUX_INITIALIZER_UNLOCKED;

static const int s_pins[] = {
    CONFIG_ICECLOCK_PIN_R1, CONFIG_ICECLOCK_PIN_G1,
    CONFIG_ICECLOCK_PIN_B1, CONFIG_ICECLOCK_PIN_R2,
    CONFIG_ICECLOCK_PIN_G2, CONFIG_ICECLOCK_PIN_B2,
    CONFIG_ICECLOCK_PIN_A, CONFIG_ICECLOCK_PIN_B,
    CONFIG_ICECLOCK_PIN_C, CONFIG_ICECLOCK_PIN_D,
    CONFIG_ICECLOCK_PIN_E, CONFIG_ICECLOCK_PIN_LAT,
    CONFIG_ICECLOCK_PIN_OE, CONFIG_ICECLOCK_PIN_CLK,
};

static bool pins_are_valid(void)
{
    const size_t pin_count = sizeof(s_pins) / sizeof(s_pins[0]);
    for (size_t i = 0; i < pin_count; ++i) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(s_pins[i])) {
            ESP_LOGE(TAG, "GPIO %d cannot be used as an output", s_pins[i]);
            return false;
        }
        for (size_t j = i + 1; j < pin_count; ++j) {
            if (s_pins[i] == s_pins[j]) {
                ESP_LOGE(TAG, "GPIO %d is assigned to multiple HUB75 signals", s_pins[i]);
                return false;
            }
        }
    }
    return true;
}

static inline void set_row_address(int row)
{
    gpio_set_level(CONFIG_ICECLOCK_PIN_A, (row >> 0) & 1);
    gpio_set_level(CONFIG_ICECLOCK_PIN_B, (row >> 1) & 1);
    gpio_set_level(CONFIG_ICECLOCK_PIN_C, (row >> 2) & 1);
    gpio_set_level(CONFIG_ICECLOCK_PIN_D, (row >> 3) & 1);
#if CONFIG_ICECLOCK_PANEL_HEIGHT_64
    gpio_set_level(CONFIG_ICECLOCK_PIN_E, (row >> 4) & 1);
#endif
}

static inline void shift_pixel(const hub75_color_t *upper,
                               const hub75_color_t *lower,
                               uint8_t mask)
{
    gpio_set_level(CONFIG_ICECLOCK_PIN_R1, (upper->r & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_G1, (upper->g & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_B1, (upper->b & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_R2, (lower->r & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_G2, (lower->g & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_B2, (lower->b & mask) != 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_CLK, 1);
    gpio_set_level(CONFIG_ICECLOCK_PIN_CLK, 0);
}

static void refresh_task(void *arg)
{
    (void)arg;

    for (;;) {
        portENTER_CRITICAL(&s_frame_lock);
        if (s_swap_pending) {
            int previous_front = s_front_index;
            s_front_index = s_back_index;
            s_back_index = previous_front;
            s_swap_pending = false;
        }
        int frame_index = s_front_index;
        uint8_t brightness = s_brightness;
        portEXIT_CRITICAL(&s_frame_lock);

        for (int plane = 0; plane < CONFIG_ICECLOCK_COLOR_DEPTH; ++plane) {
            const uint8_t mask = 1U << (8 - CONFIG_ICECLOCK_COLOR_DEPTH + plane);
            uint32_t on_time_us =
                ((uint32_t)CONFIG_ICECLOCK_PWM_BASE_US << plane) * brightness / 100U;
            if (brightness > 0 && on_time_us == 0) {
                on_time_us = 1;
            }

            for (int row = 0; row < SCAN_ROWS; ++row) {
                gpio_set_level(CONFIG_ICECLOCK_PIN_OE, 1);
                set_row_address(row);

                for (int x = 0; x < PANEL_WIDTH; ++x) {
                    shift_pixel(&s_frames[frame_index][row][x],
                                &s_frames[frame_index][row + SCAN_ROWS][x],
                                mask);
                }

                gpio_set_level(CONFIG_ICECLOCK_PIN_LAT, 1);
                gpio_set_level(CONFIG_ICECLOCK_PIN_LAT, 0);
                if (on_time_us > 0) {
                    gpio_set_level(CONFIG_ICECLOCK_PIN_OE, 0);
                    esp_rom_delay_us(on_time_us);
                    gpio_set_level(CONFIG_ICECLOCK_PIN_OE, 1);
                }
            }
        }

        /* Let the core's idle task feed the task watchdog once per frame. */
        vTaskDelay(1);
    }
}

esp_err_t hub75_init(void)
{
    ESP_RETURN_ON_FALSE(PANEL_HEIGHT == 32 || PANEL_HEIGHT == 64,
                        ESP_ERR_INVALID_ARG, TAG, "unsupported panel height");
    ESP_RETURN_ON_FALSE(pins_are_valid(), ESP_ERR_INVALID_ARG, TAG,
                        "invalid HUB75 pin map");

    uint64_t pin_mask = 0;
    for (size_t i = 0; i < sizeof(s_pins) / sizeof(s_pins[0]); ++i) {
#if CONFIG_ICECLOCK_PANEL_HEIGHT_32
        if (s_pins[i] == CONFIG_ICECLOCK_PIN_E) {
            continue;
        }
#endif
        pin_mask |= 1ULL << s_pins[i];
    }

    gpio_config_t io = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "configure HUB75 GPIOs");

    gpio_set_level(CONFIG_ICECLOCK_PIN_OE, 1);
    gpio_set_level(CONFIG_ICECLOCK_PIN_CLK, 0);
    gpio_set_level(CONFIG_ICECLOCK_PIN_LAT, 0);
    memset(s_frames, 0, sizeof(s_frames));

    BaseType_t created = xTaskCreatePinnedToCore(refresh_task, "hub75_refresh",
                                                 4096, NULL, 20, NULL, 1);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "create refresh task");

    ESP_LOGI(TAG, "panel ready: %dx%d, 1/%d scan, %d-bit PWM",
             PANEL_WIDTH, PANEL_HEIGHT, SCAN_ROWS,
             CONFIG_ICECLOCK_COLOR_DEPTH);
    return ESP_OK;
}

void hub75_begin_frame(hub75_color_t background)
{
    while (s_swap_pending) {
        vTaskDelay(1);
    }

    int back = s_back_index;
    for (int y = 0; y < PANEL_HEIGHT; ++y) {
        for (int x = 0; x < PANEL_WIDTH; ++x) {
            s_frames[back][y][x] = background;
        }
    }
}

void hub75_set_pixel(int x, int y, hub75_color_t color)
{
    if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
        return;
    }
    s_frames[s_back_index][y][x] = color;
}

void hub75_fill_rect(int x, int y, int width, int height, hub75_color_t color)
{
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            hub75_set_pixel(px, py, color);
        }
    }
}

void hub75_present(void)
{
    portENTER_CRITICAL(&s_frame_lock);
    s_swap_pending = true;
    portEXIT_CRITICAL(&s_frame_lock);
}

void hub75_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_brightness = percent;
}

int hub75_width(void)
{
    return PANEL_WIDTH;
}

int hub75_height(void)
{
    return PANEL_HEIGHT;
}
