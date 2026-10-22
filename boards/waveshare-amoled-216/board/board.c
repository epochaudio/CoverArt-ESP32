#include "board.h"
#include "board_interface.h"

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"
#include "axp2101.h"

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
#define PIN_TP_RST      40
#define PIN_TP_INT      11
#define PIN_I2C_SCL     14
#define PIN_I2C_SDA     15
#define I2C_HZ          400000
#define I2C_PROBE_MS    20
#define IMU_ADDR        0x6B

// QMI8658 chip axes to panel axes: panel axis i = sign[i] * chip axis axis[i].
// z verified on hardware (face up +1 g, face down -1 g); x and y are not yet
// checked and no gesture uses them.
static const int kImuAxis[3] = {0, 1, 2};
static const float kImuSign[3] = {1, 1, 1};

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
static esp_lcd_touch_handle_t s_touch;
static qmi8658_handle_t s_imu;
static axp2101_handle_t s_pmu;

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

static void touch_init(void)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    io_cfg.scl_speed_hz = I2C_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c, &io_cfg, &io));
    const esp_lcd_touch_config_t cfg = {
        .x_max = LCD_WIDTH,
        .y_max = LCD_HEIGHT,
        .rst_gpio_num = PIN_TP_RST,
        .int_gpio_num = PIN_TP_INT,
        .levels = {.reset = 0, .interrupt = 0},
        // Matches the panel orientation set by MADCTL 0xA0 (as in the BSP).
        .flags = {.swap_xy = 1, .mirror_x = 0, .mirror_y = 1},
    };
    if (esp_lcd_touch_new_i2c_cst9217(io, &cfg, &s_touch) != ESP_OK) {
        ESP_LOGE(TAG, "touch controller not responding; touch disabled");
        s_touch = NULL;
    }
}

bool board_touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_touch || esp_lcd_touch_read_data(s_touch) != ESP_OK) return false;
    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;
    if (esp_lcd_touch_get_data(s_touch, &point, &count, 1) != ESP_OK || count == 0) return false;
    *x = point.x;
    *y = point.y;
    return true;
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
    touch_init();
    s_imu = qmi8658_open(s_i2c, IMU_ADDR);
    s_pmu = axp2101_open(s_i2c);
}

void board_power_off(void)
{
    ESP_LOGW(TAG, "powering off");
    axp2101_power_off(s_pmu);
    for (;;) vTaskDelay(portMAX_DELAY);  // the rails drop; nothing runs after this
}

bool board_battery_read(board_battery_t *out)
{
    axp2101_status_t st;
    if (!axp2101_read(s_pmu, &st)) return false;
    *out = (board_battery_t){.usb_power = st.usb_power, .battery = st.battery, .charging = st.charging,
                             .percent = st.percent, .millivolts = st.millivolts};
    return true;
}

bool board_imu_read(float g[3])
{
    float chip[3];
    if (!qmi8658_read_accel(s_imu, chip)) return false;
    for (int i = 0; i < 3; i++) g[i] = kImuSign[i] * chip[kImuAxis[i]];
    return true;
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
