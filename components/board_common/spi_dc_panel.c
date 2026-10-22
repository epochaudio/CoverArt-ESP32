#include "spi_dc_panel.h"

#include "driver/spi_master.h"

#define SPI_DC_TRANS_QUEUE_DEPTH 4

esp_lcd_panel_io_handle_t spi_dc_panel_io(const spi_dc_panel_config_t *cfg,
                                          esp_lcd_panel_io_color_trans_done_cb_t on_flushed, void *ctx)
{
    spi_bus_config_t bus = {
        .mosi_io_num = cfg->mosi,
        .miso_io_num = -1,
        .sclk_io_num = cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = cfg->max_transfer_bytes,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = cfg->dc,
        .cs_gpio_num = cfg->cs,
        .pclk_hz = cfg->clock_mhz * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = SPI_DC_TRANS_QUEUE_DEPTH,
        .on_color_trans_done = on_flushed,
        .user_ctx = ctx,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));
    return io;
}
