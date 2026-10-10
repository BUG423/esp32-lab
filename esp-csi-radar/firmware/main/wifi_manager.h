#pragma once

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise NVS, netif, event loop and connect to WiFi (STA).
 *
 * Blocks until an IP address is obtained (or fails after retries).
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Get the currently assigned IPv4 address as a string.
 */
const char *wifi_manager_get_ip(void);

#ifdef __cplusplus
}
#endif
