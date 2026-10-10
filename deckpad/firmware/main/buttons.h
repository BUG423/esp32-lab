#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Register up to 4 buttons. Short press -> run macro idx. */
esp_err_t buttons_init(void);

#ifdef __cplusplus
}
#endif
