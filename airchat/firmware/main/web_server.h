/**
 * HTTP + WebSocket server for the off-grid chat dashboard.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Start the HTTP/WebSocket dashboard server. */
esp_err_t web_server_start(void);

/** Push current state to all connected WebSocket clients. */
void web_server_broadcast(void);

#ifdef __cplusplus
}
#endif
