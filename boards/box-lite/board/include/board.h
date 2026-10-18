#pragma once
#include <stdbool.h>

// ESP32-S3-BOX-Lite: ST7789 320x240 over SPI. Pin map from Espressif esp-bsp
// esp-box-lite (revision 19471d0be4e7149f704f306b326907a1ae8ac462).
#define BOARD_NAME       "BOX-Lite"
#define BOARD_HOSTNAME   "coverart-box-lite"

#define LCD_WIDTH        320
#define LCD_HEIGHT       240
#define LCD_SPI_MHZ      40
#define LCD_PIN_MOSI     6
#define LCD_PIN_SCLK     7
#define LCD_PIN_CS       5
#define LCD_PIN_DC       4
#define LCD_PIN_RST      48
#define LCD_PIN_BL       45
#define LCD_BL_ON_LEVEL  0    // backlight is active low
#define LCD_BOUNCE_ROWS  20

#define COVER_SIZE       240

// PSRAM: room for a full zone snapshot with many zones.
#define MOO_MAX_MESSAGE_BYTES  (512 * 1024)

// The front keys are not used yet.
#define BOARD_HAS_KNOB   0
