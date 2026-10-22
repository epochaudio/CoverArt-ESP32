#pragma once
#include <stdbool.h>
#include "board_interface.h"

// Battery state on BOARD_HAS_BATTERY boards, polled every POWER_POLL_MS.
void power_start(void);

// Latest reading; `battery` is false on boards without one.
board_battery_t power_status(void);

// Running on a battery at or below POWER_LOW_PERCENT: the screen caps its
// brightness and shows a battery badge over the cover.
#define POWER_LOW_PERCENT 20
bool power_low(void);
