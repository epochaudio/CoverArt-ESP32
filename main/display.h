#pragma once
#include <stdint.h>

// RGB565 in panel byte order (big-endian), as produced by the JPEG decoder
// with byte swapping enabled.
void display_init(void);
void display_fill(uint16_t color_be);
// 0 = dark; panels with an on/off backlight treat any other value as on.
void display_set_brightness(uint8_t percent);
// Draws a w*h block at (x, y). Pixels may live in PSRAM; they are streamed
// through an internal DMA buffer.
void display_draw(int x, int y, int w, int h, const uint16_t *pixels_be);

// Host-order RGB565 to panel order.
static inline uint16_t display_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (uint16_t)((c >> 8) | (c << 8));
}
