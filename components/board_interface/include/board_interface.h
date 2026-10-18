#pragma once

// Contract between the shared firmware (main/) and a board component
// (boards/<name>/board/). A board's include/board.h defines:
//
//   BOARD_NAME, BOARD_HOSTNAME          shown in Roon and on the network
//   LCD_WIDTH, LCD_HEIGHT               panel size in pixels
//   LCD_SPI_MHZ, LCD_PIN_MOSI, LCD_PIN_SCLK, LCD_PIN_CS, LCD_PIN_DC,
//   LCD_PIN_RST, LCD_PIN_BL, LCD_BL_ON_LEVEL, LCD_BOUNCE_ROWS
//   COVER_SIZE                          square size requested from Roon
//   MOO_MAX_MESSAGE_BYTES               largest Roon message accepted
//   BOARD_HAS_KNOB (+ KNOB_PIN_A, KNOB_PIN_B, KNOB_PIN_PRESS when 1)
//
// and its sources implement the function below.

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Creates, resets and initialises the panel on an SPI IO handle and applies
// its orientation and colour quirks.
esp_lcd_panel_handle_t board_panel_create(esp_lcd_panel_io_handle_t io);
