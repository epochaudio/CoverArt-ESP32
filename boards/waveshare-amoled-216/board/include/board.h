#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-2.16: CO5300 480x480 AMOLED over QSPI.
#define BOARD_NAME       "AMOLED-2.16"
#define BOARD_HOSTNAME   "coverart-amoled-216"

#define LCD_WIDTH        480
#define LCD_HEIGHT       480
#define LCD_BOUNCE_ROWS  20
// The CO5300 only accepts drawing areas on even coordinates.
#define LCD_DRAW_ALIGN   2

#define COVER_SIZE       480

// PSRAM: room for a full zone snapshot with many zones.
#define MOO_MAX_MESSAGE_BYTES  (512 * 1024)

#define BOARD_HAS_KNOB   0
