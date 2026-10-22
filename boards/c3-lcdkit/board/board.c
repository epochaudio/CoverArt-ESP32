#include "board.h"
#include "board_interface.h"

#include "driver/gpio.h"
#include "esp_lcd_gc9a01.h"
#include "spi_dc_panel.h"

// Pins from the ESP32-C3-LCDkit schematic.
#define PIN_MOSI      0
#define PIN_SCLK      1
#define PIN_DC        2
#define PIN_CS        7
#define PIN_BL        5   // backlight, active high
#define SPI_MHZ       40

void board_init(void) {}

esp_lcd_panel_handle_t board_display_init(esp_lcd_panel_io_color_trans_done_cb_t on_flushed, void *ctx)
{
    gpio_config_t bl = {.pin_bit_mask = 1ULL << PIN_BL, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&bl));
    board_display_set_brightness(0);

    const spi_dc_panel_config_t bus = {
        .mosi = PIN_MOSI, .sclk = PIN_SCLK, .cs = PIN_CS, .dc = PIN_DC, .clock_mhz = SPI_MHZ,
        .max_transfer_bytes = LCD_WIDTH * LCD_BOUNCE_ROWS * sizeof(uint16_t),
    };
    esp_lcd_panel_io_handle_t io = spi_dc_panel_io(&bus, on_flushed, ctx);

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t cfg = {
        .reset_gpio_num = GPIO_NUM_NC,  // no reset line; the panel is reset by command
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // this module swaps red and blue
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));  // horizontal scan is reversed
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return panel;
}

void board_display_set_brightness(uint8_t percent)
{
    gpio_set_level(PIN_BL, percent > 0);
}
