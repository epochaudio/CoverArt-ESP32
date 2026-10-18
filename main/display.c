#include "display.h"
#include "board_interface.h"

#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "display";
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_bounce;
static SemaphoreHandle_t s_done;

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

void display_init(void)
{
    s_done = xSemaphoreCreateBinary();
    s_bounce = heap_caps_malloc(LCD_WIDTH * LCD_BOUNCE_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(s_bounce);

    // Backlight off until the first frame is drawn.
    gpio_config_t bl = {.pin_bit_mask = 1ULL << LCD_PIN_BL, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&bl));
    gpio_set_level(LCD_PIN_BL, !LCD_BL_ON_LEVEL);

    spi_bus_config_t bus = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_BOUNCE_ROWS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_MHZ * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    s_panel = board_panel_create(io);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    display_fill(0);
    gpio_set_level(LCD_PIN_BL, LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "%s panel %dx%d ready", BOARD_NAME, LCD_WIDTH, LCD_HEIGHT);
}

static void push_rows(int x, int y, int w, int rows)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + rows, s_bounce));
    xSemaphoreTake(s_done, portMAX_DELAY);
}

void display_fill(uint16_t color_be)
{
    for (int i = 0; i < LCD_WIDTH * LCD_BOUNCE_ROWS; i++) s_bounce[i] = color_be;
    for (int y = 0; y < LCD_HEIGHT; y += LCD_BOUNCE_ROWS) {
        int rows = LCD_HEIGHT - y < LCD_BOUNCE_ROWS ? LCD_HEIGHT - y : LCD_BOUNCE_ROWS;
        push_rows(0, y, LCD_WIDTH, rows);
    }
}

void display_draw(int x, int y, int w, int h, const uint16_t *pixels_be)
{
    const int max_rows = (LCD_WIDTH * LCD_BOUNCE_ROWS) / w;
    for (int row = 0; row < h; row += max_rows) {
        int rows = h - row < max_rows ? h - row : max_rows;
        memcpy(s_bounce, pixels_be + (size_t)row * w, (size_t)rows * w * sizeof(uint16_t));
        push_rows(x, y + row, w, rows);
    }
}
