/**
 * bytepet - physical button driver (short press = feed, long press = play)
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the button GPIO (active-low, internal pull-up)
 *        and start the polling task.
 */
esp_err_t button_init(void);

#ifdef __cplusplus
}
#endif
