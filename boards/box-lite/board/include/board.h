#pragma once

// ESP32-S3-BOX-Lite: ST7789 320x240 over SPI, 8 MB PSRAM.
#define BOARD_NAME       "BOX-Lite"
#define BOARD_HOSTNAME   "coverart-box-lite"

#define LCD_WIDTH        320
#define LCD_HEIGHT       240
#define LCD_BOUNCE_ROWS  20
#define LCD_DRAW_ALIGN   1

#define COVER_SIZE       240

// PSRAM: room for a full zone snapshot with many zones.
#define MOO_MAX_MESSAGE_BYTES  (512 * 1024)

// The front keys are not used yet.
#define BOARD_HAS_KNOB   0
