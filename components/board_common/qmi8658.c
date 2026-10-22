#include "qmi8658.h"

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "qmi8658";

// Registers and values from the QMI8658A datasheet.
#define REG_WHO_AM_I      0x00
#define REG_CTRL1         0x02   // serial interface
#define REG_CTRL2         0x03   // accelerometer full scale and output rate
#define REG_CTRL7         0x08   // sensor enable
#define REG_AX_L          0x35   // AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H
#define REG_RESET         0x60
#define WHO_AM_I_VALUE    0x05
#define RESET_COMMAND     0xB0
#define CTRL1_ADDR_AUTO_INCREMENT  0x40
#define CTRL2_ACCEL_4G_125HZ       0x16  // aFS = +-4 g, aODR ~125 Hz
#define CTRL7_ACCEL_ENABLE         0x01
#define ACCEL_LSB_PER_G   8192.0f        // at +-4 g
#define RESET_SETTLE_MS   20
#define I2C_HZ            400000
#define I2C_TIMEOUT_MS    20

struct qmi8658 {
    i2c_master_dev_handle_t dev;
};

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, len, I2C_TIMEOUT_MS);
}

qmi8658_handle_t qmi8658_open(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = I2C_HZ};
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) return NULL;
    uint8_t id = 0;
    if (read_regs(dev, REG_WHO_AM_I, &id, 1) != ESP_OK || id != WHO_AM_I_VALUE) {
        ESP_LOGW(TAG, "no QMI8658 at 0x%02X (id 0x%02X)", addr, id);
        i2c_master_bus_rm_device(dev);
        return NULL;
    }
    write_reg(dev, REG_RESET, RESET_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(RESET_SETTLE_MS));
    if (write_reg(dev, REG_CTRL1, CTRL1_ADDR_AUTO_INCREMENT) != ESP_OK ||
        write_reg(dev, REG_CTRL2, CTRL2_ACCEL_4G_125HZ) != ESP_OK ||
        write_reg(dev, REG_CTRL7, CTRL7_ACCEL_ENABLE) != ESP_OK) {
        ESP_LOGE(TAG, "configuration failed");
        i2c_master_bus_rm_device(dev);
        return NULL;
    }
    struct qmi8658 *imu = calloc(1, sizeof(*imu));
    imu->dev = dev;
    ESP_LOGI(TAG, "accelerometer ready at 0x%02X", addr);
    return imu;
}

bool qmi8658_read_accel(qmi8658_handle_t imu, float g[3])
{
    uint8_t raw[6];
    if (!imu || read_regs(imu->dev, REG_AX_L, raw, sizeof(raw)) != ESP_OK) return false;
    for (int i = 0; i < 3; i++) g[i] = (int16_t)(raw[2 * i] | (raw[2 * i + 1] << 8)) / ACCEL_LSB_PER_G;
    return true;
}
