#pragma once
#include <stdbool.h>
#include "driver/i2c_master.h"

// Minimal QMI8658 accelerometer reader (the gyroscope stays off): enough to
// tell which way a device faces and whether it moves.
typedef struct qmi8658 *qmi8658_handle_t;

// Checks the chip id, resets it and starts the accelerometer at +-4 g.
// Returns NULL if no QMI8658 answers at `addr`.
qmi8658_handle_t qmi8658_open(i2c_master_bus_handle_t bus, uint8_t addr);

// Latest acceleration in g along the chip's own x, y, z axes.
bool qmi8658_read_accel(qmi8658_handle_t imu, float g[3]);
