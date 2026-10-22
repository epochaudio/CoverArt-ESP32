#include "board.h"
#include "board_interface.h"

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_co5300.h"
#include "esp_log.h"

static const char *TAG = "board";

// Pins and panel init sequence from Waveshare's BSP component
// waveshare/esp32_s3_touch_amoled_2_16 v2.0.1 (Apache-2.0).
#define PIN_LCD_CS      12
#define PIN_LCD_PCLK    38
#define PIN_LCD_D0      4
#define PIN_LCD_D1      5
#define PIN_LCD_D2      6
#define PIN_LCD_D3      7
#define PIN_LCD_RST     39
#define PIN_I2C_SCL     14
#define PIN_I2C_SDA     15
#define I2C_HZ          400000
#define I2C_PROBE_MS    20

static const co5300_lcd_init_cmd_t kPanelInit[] = {
    {0x11, (uint8_t[]){0x00}, 0, 600},  // sleep out
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},    // 16-bit colour
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0x00}, 1, 0},    // brightness 0 until the first frame
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x36, (uint8_t[]){0xA0}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 600},  // display on
};

static i2c_master_bus_handle_t s_i2c;
static esp_lcd_panel_handle_t s_panel;

// Devices expected on the shared I2C bus, logged at boot so a board revision
// with different parts shows up in the first log.
static void probe_i2c(void)
{
    static const struct { uint8_t addr; const char *part; } kKnown[] = {
        {0x34, "AXP2101 power"}, {0x5A, "CST9217 touch"}, {0x6A, "IMU"}, {0x6B, "IMU"},
        {0x51, "RTC"}, {0x18, "ES8311 codec"}, {0x40, "ES7210 mics"},
    };
    for (size_t i = 0; i < sizeof(kKnown) / sizeof(kKnown[0]); i++) {
        bool found = i2c_master_probe(s_i2c, kKnown[i].addr, I2C_PROBE_MS) == ESP_OK;
        ESP_LOGI(TAG, "i2c 0x%02X %-14s %s", kKnown[i].addr, kKnown[i].part, found ? "found" : "-");
    }
}

void board_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &s_i2c));
    probe_i2c();
}

esp_lcd_panel_handle_t board_display_init(esp_lcd_panel_io_color_trans_done_cb_t on_flushed, void *ctx)
{
    const spi_bus_config_t bus = CO5300_PANEL_BUS_QSPI_CONFIG(PIN_LCD_PCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2,
                                                              PIN_LCD_D3, LCD_WIDTH * LCD_BOUNCE_ROWS * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, on_flushed, ctx);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    co5300_vendor_config_t vendor = {
        .init_cmds = kPanelInit,
        .init_cmds_size = sizeof(kPanelInit) / sizeof(kPanelInit[0]),
        .flags.use_qspi_interface = 1,
    };
    esp_lcd_panel_dev_config_t cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io, &cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    return s_panel;
}

void board_display_set_brightness(uint8_t percent)
{
    if (s_panel) esp_lcd_panel_co5300_set_brightness(s_panel, percent);
}
