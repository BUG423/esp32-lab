#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
    struct tm local_time;
    bool wifi_connected;
    bool time_synchronized;
    bool rtc_time_valid;
    bool temperature_valid;
    float temperature_c;
} clock_face_state_t;

void clock_face_render_boot(void);
void clock_face_render(const clock_face_state_t *state);
