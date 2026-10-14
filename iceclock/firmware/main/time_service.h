#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t time_service_init(void);
bool time_service_wait_for_initial_sync(TickType_t timeout);
void time_service_get_local_time(struct tm *time_info);
bool time_service_wifi_connected(void);
bool time_service_is_synchronized(void);
bool time_service_has_valid_rtc_time(void);
esp_err_t time_service_read_temperature(float *temperature_c);
