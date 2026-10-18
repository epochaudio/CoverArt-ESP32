#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi_types.h"

// Station plus optional setup access point. The driver keeps its config in
// RAM only; the home network lives in store.c, so the Wi-Fi settings saved by
// any other firmware on the shared nvs partition are left alone.
void wifi_init(void);

// Joins (and keeps rejoining) the given network.
void wifi_connect(const char *ssid, const char *password);
// Stops joining; used by setup between retries of the saved network, since a
// station that keeps scanning disturbs the setup network on the same radio.
void wifi_disconnect(void);
bool wifi_wait_connected(uint32_t timeout_ms);
bool wifi_is_connected(void);
// Milliseconds since the station lost or never got an IP; 0 while connected.
int64_t wifi_offline_ms(void);

// Opens an open setup network next to the station (AP+STA mode).
void wifi_start_ap(const char *ssid);
// Scans for networks; returns how many records were written (strongest first).
int wifi_scan(wifi_ap_record_t *records, int max);
