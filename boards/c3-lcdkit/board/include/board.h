#pragma once

// ESP32-C3-LCDkit: GC9A01 240x240 round panel over SPI, no PSRAM, rotary knob.
#define BOARD_NAME       "C3-LCDkit"
#define BOARD_HOSTNAME   "coverart-c3-lcdkit"

#define LCD_WIDTH        240
#define LCD_HEIGHT       240
#define LCD_BOUNCE_ROWS  16
#define LCD_DRAW_ALIGN   1

// Full-bleed cover; the round glass crops the corners.
#define COVER_SIZE       240

// Internal RAM only. A zone snapshot is parsed one zone at a time, so the
// limit covers the raw message, not a parsed tree.
#define MOO_MAX_MESSAGE_BYTES  (96 * 1024)

// Rotary encoder with push switch (active low, internal pull-ups).
#define BOARD_HAS_TOUCH  0
#define BOARD_HAS_KNOB   1
#define KNOB_PIN_A       10
#define KNOB_PIN_B       6
#define KNOB_PIN_PRESS   9

// Screen guard (main/screen_guard.h): an LCD stays lit.
#define SCREEN_DIM_AFTER_MS   0
#define SCREEN_OFF_AFTER_MS   0
#define SCREEN_DIM_PERCENT    100
#define SCREEN_PIXEL_SHIFT    0
