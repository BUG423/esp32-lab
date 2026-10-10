/**
 * bytepet - HTTP + WebSocket dashboard server
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the embedded HTTP server (serves / and /index.html),
 *        plus a WebSocket endpoint at /ws that pushes state at 1 Hz
 *        and accepts action commands.
 */
esp_err_t web_server_start(void);

#ifdef __cplusplus
}
#endif
