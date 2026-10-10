#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Demo-grade optical key exchange + message protection. */
#define PAIR_KEY_LEN 8

typedef enum {
    PAIR_IDLE = 0,      /**< not paired */
    PAIR_WAITING,       /**< initiator: offer sent, waiting for ack */
    PAIR_PAIRED,        /**< shared key active */
} pair_state_t;

esp_err_t pairing_init(void);

/** Initiator: generate a fresh key and transmit it as light. */
esp_err_t pairing_start(void);

/** Drop the shared key and return to PAIR_IDLE. */
esp_err_t pairing_unpair(void);

pair_state_t pairing_get_state(void);
bool pairing_is_paired(void);

/** Copy the shared key (8 bytes). Returns false when unpaired. */
bool pairing_get_key(uint8_t key_out[PAIR_KEY_LEN]);

/**
 * @brief Protect plaintext for the optical link.
 *
 * When paired, wraps as ENC_MSG (type 0xB2) with a keystream XOR.
 * When unpaired, copies plaintext unchanged.
 *
 * @return payload length, or -1 on error
 */
int pairing_protect(const char *plain, uint8_t *out, int out_cap);

/**
 * @brief Handle an incoming raw payload (before text dispatch).
 *
 * Consumes pairing control frames. For ENC_MSG it decrypts into
 * `plain_out` and returns the plaintext length. For non-control,
 * non-encrypted frames returns 0 (caller should treat as plaintext).
 *
 * @return >0 plaintext length, 0 = not for us / pass-through, -1 = bad frame
 */
int pairing_on_payload(const uint8_t *data, int len, char *plain_out, int plain_cap);

#ifdef __cplusplus
}
#endif
