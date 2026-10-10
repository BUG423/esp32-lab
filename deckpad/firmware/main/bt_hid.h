#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Send a keyboard modifier+key combo (USB HID usage codes). */
esp_err_t bt_hid_send_key(uint8_t modifier, uint8_t key);

/** Send a raw 8-byte keyboard report (modifiers + up to 6 keys). */
esp_err_t bt_hid_send_report(const uint8_t report[8]);

/** Send a text string as keystrokes (ASCII only, simple mapping). */
esp_err_t bt_hid_send_text(const char *text);

/** Send a consumer-page media key (play/pause, volume, mute, ...). */
esp_err_t bt_hid_send_consumer(uint16_t usage);

/** Is a BLE host connected? */
bool bt_hid_is_connected(void);

/** Start BLE HID device with the given name. */
esp_err_t bt_hid_start(const char *device_name);

#ifdef __cplusplus
}
#endif