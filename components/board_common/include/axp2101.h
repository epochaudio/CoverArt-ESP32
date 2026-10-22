#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

// Minimal AXP2101 reader for battery state; power rails are left as the
// chip's defaults (the board's display and sensors work with them).
typedef struct axp2101 *axp2101_handle_t;

typedef struct {
    bool usb_power;      // VBUS present and good
    bool battery;        // battery connected
    bool charging;
    int percent;         // fuel gauge, 0..100; -1 if not available
    int millivolts;      // battery voltage
} axp2101_status_t;

// Switches the whole board off (the chip's software power-off). It comes
// back with a short press of the power key or when USB is plugged in.
void axp2101_power_off(axp2101_handle_t pmu);

// Enables battery detection, voltage measurement and the fuel gauge.
axp2101_handle_t axp2101_open(i2c_master_bus_handle_t bus);
bool axp2101_read(axp2101_handle_t pmu, axp2101_status_t *out);
