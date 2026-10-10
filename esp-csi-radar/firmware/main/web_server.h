#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start HTTP server with WebSocket endpoint for live CSI frames.
 *
 * Routes:
 *   GET /           -> embedded dashboard (index.html)
 *   GET /ws         -> WebSocket, binary/JSON CSI frames pushed periodically
 *   GET /api/status -> JSON one-shot status
 */
esp_err_t web_server_start(void);

/**
 * @brief Publish a processed CSI frame to all connected WebSocket clients.
 *
 * Called from the processor publish task. Safe to call even if no client
 * is connected (no-op).
 */
void web_server_broadcast_csi(void);

/**
 * @brief Stop the web server.
 */
void web_server_stop(void);

#ifdef __cplusplus
}
#endif
