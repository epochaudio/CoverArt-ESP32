#include "board_interface.h"

#include "board.h"
#include "esp_lcd_panel_st7789.h"

esp_lcd_panel_handle_t board_panel_create(esp_lcd_panel_io_handle_t io)
{
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    // Orientation and inversion match the BOX-Lite BSP (landscape, buttons below).
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    return panel;
}
