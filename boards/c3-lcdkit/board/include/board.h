#pragma once
#include <stdbool.h>

// ESP32-C3-LCDkit: GC9A01 240x240 round panel over SPI, no PSRAM. Pins and
// panel settings verified on hardware (GC9A01: BGR order, inverted, mirrored X).
#define BOARD_NAME       "C3-LCDkit"
#define BOARD_HOSTNAME   "coverart-c3-lcdkit"

#define LCD_WIDTH        240
#define LCD_HEIGHT       240
#define LCD_SPI_MHZ      40
#define LCD_PIN_MOSI     0
#define LCD_PIN_SCLK     1
#define LCD_PIN_CS       7
#define LCD_PIN_DC       2
#define LCD_PIN_RST      (-1)  // no reset line; the panel is reset by command
#define LCD_PIN_BL       5
#define LCD_BL_ON_LEVEL  1
#define LCD_BOUNCE_ROWS  16
// Panel quirks: BGR colour order, inverted colours, mirrored horizontally.
#define LCD_MIRROR_X     true
#define LCD_MIRROR_Y     false

// Full-bleed cover; the round glass crops the corners.
#define COVER_SIZE       240

// Internal RAM only. A zone snapshot is parsed one zone at a time, so the
// limit covers the raw message, not a parsed tree.
#define MOO_MAX_MESSAGE_BYTES  (96 * 1024)

// Rotary encoder with push switch (active low, internal pull-ups).
#define BOARD_HAS_KNOB   1
#define KNOB_PIN_A       10
#define KNOB_PIN_B       6
#define KNOB_PIN_PRESS   9
