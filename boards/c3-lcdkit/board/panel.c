#include "board_interface.h"

#include "board.h"
#include "esp_lcd_gc9a01.h"

esp_lcd_panel_handle_t board_panel_create(esp_lcd_panel_io_handle_t io)
{
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        // This GC9A01 module expects BGR; RGB swaps red and blue.
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, LCD_MIRROR_X, LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    return panel;
}
