/**
 * Physical buttons -> run macros. Button 0 long-press cycles layers.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "buttons.h"
#include "macros.h"

static const char *TAG = "buttons";

#define LONG_PRESS_MS 600

static const int BTN_GPIOS[4] = {
    CONFIG_DECKPAD_BTN0_GPIO,
    CONFIG_DECKPAD_BTN1_GPIO,
    CONFIG_DECKPAD_BTN2_GPIO,
    CONFIG_DECKPAD_BTN3_GPIO,
};

static void btn_task(void *arg)
{
    int n = CONFIG_DECKPAD_NUM_BUTTONS;
    if (n > 4) n = 4;
    bool last[4] = {false};
    int held_ms[4] = {0};
    bool long_fired[4] = {false};

    while (1) {
        for (int i = 0; i < n; i++) {
            bool pressed = gpio_get_level(BTN_GPIOS[i]) == 0; /* active low */

            if (pressed) {
                held_ms[i] += 30;
                /* button 0: long-press switches layer */
                if (i == 0 && held_ms[i] >= LONG_PRESS_MS && !long_fired[i]) {
                    int L = macros_cycle_layer();
                    ESP_LOGI(TAG, "long-press -> layer %d", L);
                    long_fired[i] = true;
                }
            } else if (last[i]) {
                /* release */
                if (!long_fired[i]) {
                    ESP_LOGI(TAG, "button %d -> macro %d (L%d)", i, i, macros_get_layer());
                    macros_run(i);
                }
                held_ms[i] = 0;
                long_fired[i] = false;
            }
            last[i] = pressed;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

esp_err_t buttons_init(void)
{
    int n = CONFIG_DECKPAD_NUM_BUTTONS;
    if (n > 4) n = 4;

    for (int i = 0; i < n; i++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << BTN_GPIOS[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&io);
    }

    xTaskCreate(btn_task, "buttons", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "%d buttons ready (btn0 long-press = next layer)", n);
    return ESP_OK;
}