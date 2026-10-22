#pragma once

// Contract between the shared firmware (main/) and a board component
// (boards/<name>/board/). Pins and bus details stay inside the board; main/
// only sees the sizes and capabilities below plus the functions at the end.
//
// A board's include/board.h defines:
//
//   BOARD_NAME, BOARD_HOSTNAME   shown in Roon and on the network
//   LCD_WIDTH, LCD_HEIGHT        panel size in pixels
//   LCD_BOUNCE_ROWS              rows per transfer through the DMA buffer
//   LCD_DRAW_ALIGN               every drawn area must start and end on this
//                                pixel grid (1 = none, 2 for CO5300 AMOLEDs);
//                                display.c pads areas, callers need not care
//   COVER_SIZE                   square cover size requested from Roon
//   MOO_MAX_MESSAGE_BYTES        largest Roon message accepted
//   BOARD_HAS_KNOB               1 with KNOB_PIN_A, KNOB_PIN_B, KNOB_PIN_PRESS

#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// First call at boot: power rails, shared buses, anything the display and
// inputs depend on.
void board_init(void);

// Creates the display bus, IO and panel, applies orientation and colour
// quirks, and switches the panel on with the image dark. `on_flushed` must be
// registered as the IO's colour-transfer-done callback.
esp_lcd_panel_handle_t board_display_init(esp_lcd_panel_io_color_trans_done_cb_t on_flushed, void *ctx);

// 0 = dark. Boards with an on/off backlight treat any other value as full on.
void board_display_set_brightness(uint8_t percent);
