/**
 * bytepet - physical button driver
 *
 * GPIO button (default 0, active-low):
 *   short press (< 800 ms)  -> feed
 *   long press  (>= 800 ms) -> play
 */
#include "button.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "pet.h"
#include "sdkconfig.h"

static const char *TAG = "button";

#define BUTTON_POLL_MS     20
#define BUTTON_LONG_MS     800
#define BUTTON_DEBOUNCE_MS 40

static void button_task(void *arg)
{
    (void)arg;
    bool pressed = false;
    int press_ms = 0;
    int debounce_ms = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
        int level = gpio_get_level(CONFIG_BYTEPET_BUTTON_GPIO);

        if (level == 0) {
            /* pressed (active-low) */
            if (!pressed) {
                pressed = true;
                press_ms = 0;
                debounce_ms = 0;
            } else {
                press_ms += BUTTON_POLL_MS;
            }
        } else {
            /* released */
            if (pressed) {
                debounce_ms += BUTTON_POLL_MS;
                if (debounce_ms >= BUTTON_DEBOUNCE_MS) {
                    if (press_ms >= BUTTON_LONG_MS) {
                        ESP_LOGI(TAG, "long press -> play");
                        pet_do_action(PET_ACTION_PLAY);
                    } else if (press_ms > 0) {
                        ESP_LOGI(TAG, "short press -> feed");
                        pet_do_action(PET_ACTION_FEED);
                    }
                    pressed = false;
                    press_ms = 0;
                    debounce_ms = 0;
                }
            }
        }
    }
}

esp_err_t button_init(void)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << CONFIG_BYTEPET_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ok = xTaskCreate(button_task, "button", 2560, NULL, 6, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "button ready on GPIO %d", CONFIG_BYTEPET_BUTTON_GPIO);
    return ESP_OK;
}
