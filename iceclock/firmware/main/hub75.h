#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} hub75_color_t;

esp_err_t hub75_init(void);
void hub75_begin_frame(hub75_color_t background);
void hub75_set_pixel(int x, int y, hub75_color_t color);
void hub75_fill_rect(int x, int y, int width, int height, hub75_color_t color);
void hub75_present(void);
void hub75_set_brightness(uint8_t percent);
int hub75_width(void);
int hub75_height(void);
