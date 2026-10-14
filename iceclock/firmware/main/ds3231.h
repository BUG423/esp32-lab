#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

esp_err_t ds3231_init(void);
bool ds3231_is_available(void);
esp_err_t ds3231_read_time(struct tm *time_info);
esp_err_t ds3231_write_time(const struct tm *time_info);
esp_err_t ds3231_read_temperature(float *temperature_c);
