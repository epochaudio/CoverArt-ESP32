#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-2.16: CO5300 480x480 AMOLED over QSPI.
#define BOARD_NAME       "AMOLED-2.16"
#define BOARD_HOSTNAME   "coverart-amoled-216"

#define LCD_WIDTH        480
#define LCD_HEIGHT       480
#define LCD_BOUNCE_ROWS  20
// The CO5300 only accepts drawing areas on even coordinates.
#define LCD_DRAW_ALIGN   2

// 4 px short of the panel on each side, room for the burn-in pixel shift.
#define COVER_SIZE       472

// PSRAM: room for a full zone snapshot with many zones.
#define MOO_MAX_MESSAGE_BYTES  (512 * 1024)

// CST9217 capacitive touch, single finger, panel coordinates.
#define BOARD_HAS_TOUCH  1
#define BOARD_HAS_KNOB   0

// Screen guard (main/screen_guard.h): AMOLED burn-in and battery.
#define SCREEN_DIM_AFTER_MS   (5 * 60 * 1000)
#define SCREEN_OFF_AFTER_MS   (15 * 60 * 1000)
#define SCREEN_DIM_PERCENT    20
#define SCREEN_PIXEL_SHIFT    1
