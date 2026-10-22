#include "axp2101.h"

#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "axp2101";

// Registers from the AXP2101 datasheet.
#define AXP2101_ADDR          0x34
#define REG_STATUS1           0x00   // bit 5 VBUS good, bit 3 battery present
#define REG_STATUS2           0x01   // bits 6:5 battery current: 01 charging, 10 discharging
#define REG_GAUGE_CTRL        0x18   // bit 3 fuel gauge enable
#define REG_ADC_ENABLE        0x30   // bit 0 battery voltage measurement
#define REG_VBAT_H            0x34   // bits 5:0, then REG_VBAT_L: battery mV
#define REG_BAT_DETECT        0x68   // bit 0 battery detection enable
#define REG_BAT_PERCENT       0xA4
#define STATUS1_VBUS_GOOD     0x20
#define STATUS1_BAT_PRESENT   0x08
#define STATUS2_CURRENT_SHIFT 5
#define STATUS2_CURRENT_MASK  0x03
#define CURRENT_CHARGING      0x01
#define GAUGE_ENABLE          0x08
#define ADC_VBAT_ENABLE       0x01
#define BAT_DETECT_ENABLE     0x01
#define I2C_HZ                400000
#define I2C_TIMEOUT_MS        20

struct axp2101 {
    i2c_master_dev_handle_t dev;
};

static esp_err_t read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, len, I2C_TIMEOUT_MS);
}

static esp_err_t set_bits(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t bits)
{
    uint8_t v;
    esp_err_t err = read_regs(dev, reg, &v, 1);
    if (err != ESP_OK) return err;
    const uint8_t buf[2] = {reg, (uint8_t)(v | bits)};
    return i2c_master_transmit(dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

axp2101_handle_t axp2101_open(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = AXP2101_ADDR, .scl_speed_hz = I2C_HZ};
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) return NULL;
    if (set_bits(dev, REG_BAT_DETECT, BAT_DETECT_ENABLE) != ESP_OK ||
        set_bits(dev, REG_ADC_ENABLE, ADC_VBAT_ENABLE) != ESP_OK ||
        set_bits(dev, REG_GAUGE_CTRL, GAUGE_ENABLE) != ESP_OK) {
        ESP_LOGW(TAG, "not responding");
        i2c_master_bus_rm_device(dev);
        return NULL;
    }
    struct axp2101 *pmu = calloc(1, sizeof(*pmu));
    pmu->dev = dev;
    return pmu;
}

bool axp2101_read(axp2101_handle_t pmu, axp2101_status_t *out)
{
    uint8_t st[2], vbat[2], pct;
    if (!pmu || read_regs(pmu->dev, REG_STATUS1, st, 2) != ESP_OK || read_regs(pmu->dev, REG_VBAT_H, vbat, 2) != ESP_OK ||
        read_regs(pmu->dev, REG_BAT_PERCENT, &pct, 1) != ESP_OK)
        return false;
    out->usb_power = st[0] & STATUS1_VBUS_GOOD;
    out->battery = st[0] & STATUS1_BAT_PRESENT;
    out->charging = ((st[1] >> STATUS2_CURRENT_SHIFT) & STATUS2_CURRENT_MASK) == CURRENT_CHARGING;
    out->millivolts = ((vbat[0] & 0x3F) << 8) | vbat[1];
    out->percent = out->battery && pct <= 100 ? pct : -1;
    return true;
}
