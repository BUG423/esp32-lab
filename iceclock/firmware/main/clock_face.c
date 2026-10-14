#include "clock_face.h"

#include <stdio.h>
#include <string.h>

#include "hub75.h"

typedef struct {
    char character;
    uint8_t rows[7];
} glyph_t;

/* Five-bit-wide glyphs, most-significant visible pixel first. */
static const glyph_t FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}},
    {':', {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00}},
    {'0', {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}},
    {'1', {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}},
    {'2', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}},
    {'3', {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}},
    {'4', {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}},
    {'5', {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}},
    {'6', {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}},
    {'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}},
    {'9', {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}},
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'C', {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f}},
    {'D', {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
    {'G', {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'Q', {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}},
    {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}},
};

typedef struct {
    int x;
    int y;
    int speed;
} snowflake_t;

static snowflake_t s_snow[] = {
    {2, 1, 1}, {9, 18, 2}, {16, 5, 1}, {23, 27, 3},
    {31, 12, 2}, {39, 3, 1}, {46, 23, 2}, {54, 9, 1},
    {61, 30, 3}, {6, 41, 1}, {19, 53, 2}, {35, 44, 1},
    {49, 58, 2}, {58, 47, 1},
};

static const uint8_t *find_glyph(char character)
{
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].character == character) {
            return FONT[i].rows;
        }
    }
    return FONT[0].rows;
}

static void draw_character(int x, int y, char character, int scale,
                           hub75_color_t color)
{
    const uint8_t *rows = find_glyph(character);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if (rows[row] & (1U << (4 - column))) {
                hub75_fill_rect(x + column * scale, y + row * scale,
                                scale, scale, color);
            }
        }
    }
}

static int text_width(const char *text, int scale)
{
    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }
    return (int)(length * 6 - 1) * scale;
}

static void draw_text(int x, int y, const char *text, int scale,
                      hub75_color_t color)
{
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        draw_character(x, y, *cursor, scale, color);
        x += 6 * scale;
    }
}

static void draw_centered(int y, const char *text, int scale,
                          hub75_color_t color)
{
    int x = (hub75_width() - text_width(text, scale)) / 2;
    draw_text(x, y, text, scale, color);
}

static void draw_snow(void)
{
    const hub75_color_t snow = {.r = 8, .g = 30, .b = 55};
    int width = hub75_width();
    int height = hub75_height();

    for (size_t i = 0; i < sizeof(s_snow) / sizeof(s_snow[0]); ++i) {
        snowflake_t *flake = &s_snow[i];
        flake->y += flake->speed;
        if (flake->y >= height) {
            flake->y = 0;
            flake->x = (flake->x * 17 + 11 + (int)i) % width;
        }
        if (flake->x < width && flake->y < height) {
            hub75_set_pixel(flake->x, flake->y, snow);
        }
    }
}

void clock_face_render_boot(void)
{
    const hub75_color_t background = {.r = 0, .g = 2, .b = 8};
    const hub75_color_t ice = {.r = 60, .g = 210, .b = 255};
    const hub75_color_t blue = {.r = 20, .g = 70, .b = 180};

    hub75_begin_frame(background);
    draw_centered(hub75_height() / 2 - 12, "ICE", 2, ice);
    draw_centered(hub75_height() / 2 + 5, "CLOCK", 1, blue);
    hub75_present();
}

void clock_face_render(const clock_face_state_t *state)
{
    static const char *WEEKDAYS[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
    const hub75_color_t background = {.r = 0, .g = 1, .b = 5};
    const hub75_color_t ice = {.r = 55, .g = 205, .b = 255};
    const hub75_color_t ice_dim = {.r = 12, .g = 68, .b = 100};
    const hub75_color_t calendar = {.r = 70, .g = 100, .b = 210};
    const hub75_color_t healthy = {.r = 20, .g = 210, .b = 100};
    const hub75_color_t fallback = {.r = 255, .g = 130, .b = 20};

    char time_text[6];
    char date_text[16];
    snprintf(time_text, sizeof(time_text), "%02d:%02d",
             state->local_time.tm_hour, state->local_time.tm_min);
    snprintf(date_text, sizeof(date_text), "%02d-%02d %s",
             state->local_time.tm_mon + 1, state->local_time.tm_mday,
             WEEKDAYS[state->local_time.tm_wday]);

    hub75_begin_frame(background);
    draw_snow();

    int time_scale = hub75_width() >= 96 ? 3 : 2;
    int time_y = hub75_height() == 32 ? 1 : 8;
    draw_centered(time_y, time_text, time_scale,
                  (state->local_time.tm_sec & 1) ? ice_dim : ice);

    if (hub75_height() == 32) {
        draw_centered(22, date_text, 1, calendar);
        hub75_set_pixel(0, 31, state->time_synchronized ? healthy : fallback);
        hub75_set_pixel(2, 31, state->wifi_connected ? healthy : ice_dim);
    } else {
        draw_centered(35, date_text, 1, calendar);

        const char *source = state->time_synchronized ? "NTP" :
                             (state->rtc_time_valid ? "RTC" : "BUILD");
        draw_text(2, 53, source, 1,
                  state->time_synchronized ? healthy : fallback);

        if (state->temperature_valid) {
            char temperature[10];
            snprintf(temperature, sizeof(temperature), "%.1fC",
                     (double)state->temperature_c);
            int x = hub75_width() - text_width(temperature, 1) - 1;
            draw_text(x, 53, temperature, 1, ice_dim);
        }
        hub75_set_pixel(hub75_width() - 1, hub75_height() - 1,
                        state->wifi_connected ? healthy : fallback);
    }

    hub75_present();
}
