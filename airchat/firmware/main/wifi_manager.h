/**
 * WiFi manager — SoftAP + idle STA for ESP-NOW.
 *
 * Brings WiFi up in APSTA mode so the SoftAP hands out an IP
 * (192.168.4.1) for the web dashboard while ESP-NOW broadcasts on the
 * same radio channel. The STA interface is never associated to a router.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Start SoftAP + STA (no AP association) and pin the radio channel. */
esp_err_t wifi_manager_start(void);

/** IP address of the SoftAP interface (e.g. "192.168.4.1"). */
const char *wifi_manager_get_ip(void);

/** SSID of the SoftAP, e.g. "airchat-A1B2". */
const char *wifi_manager_get_ssid(void);

/** Configured WiFi / ESP-NOW channel. */
int wifi_manager_get_channel(void);

/** Base MAC address of this node (6 bytes). */
void wifi_manager_get_mac(uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
