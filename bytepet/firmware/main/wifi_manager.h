/**
 * bytepet - WiFi station manager (connect + soft-AP fallback)
 */
#pragma once

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up WiFi. Connects to CONFIG_BYTEPET_WIFI_SSID if configured,
 *        otherwise starts an open soft-AP named "bytepet-setup".
 *
 * Blocks until an IP address is obtained (or timeout).
 */
esp_err_t wifi_manager_start(void);

/** @brief Fill buf with the current IPv4 string. Returns length. */
int wifi_manager_get_ip(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
