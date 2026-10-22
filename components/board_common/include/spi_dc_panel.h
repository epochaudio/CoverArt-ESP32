#pragma once
#include <stddef.h>
#include "esp_lcd_panel_io.h"

// Classic 4-wire SPI panel (MOSI, SCLK, CS, D/C) on SPI2: initialises the bus
// and returns the panel IO with 8-bit commands and parameters.
typedef struct {
    int mosi, sclk, cs, dc;
    int clock_mhz;
    size_t max_transfer_bytes;
} spi_dc_panel_config_t;

esp_lcd_panel_io_handle_t spi_dc_panel_io(const spi_dc_panel_config_t *cfg,
                                          esp_lcd_panel_io_color_trans_done_cb_t on_flushed, void *ctx);
