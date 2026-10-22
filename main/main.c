#include "board_interface.h"
#include "display.h"
#include "factory_reset.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "knob.h"
#include "motion.h"
#include "provision.h"
#include "roon.h"
#include "screen_guard.h"
#include "store.h"
#include "touch.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "main";

// How long the saved network may be unreachable before the device opens its
// setup network: at boot (e.g. a new router), and later while running
// (restart, then the boot rule applies).
#define WIFI_JOIN_TIMEOUT_MS   (3 * 60 * 1000)
#define WIFI_LOST_RESTART_MS   (3 * 60 * 1000)
#define WIFI_WATCH_PERIOD_MS   10000

void app_main(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Roon cover running from %s", running ? running->label : "?");
    board_init();
    display_init();
    store_init();
    factory_reset_check();
    wifi_init();

    char ssid[33], password[65];
    if (!store_get_wifi(ssid, sizeof(ssid), password, sizeof(password))) provision_run();
    const char *lines[] = {"CONNECTING", "WI-FI"};
    ui_message(lines, 2);
    wifi_connect(ssid, password);
    if (!wifi_wait_connected(WIFI_JOIN_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "saved network unreachable, opening setup");
        provision_run();
    }

    screen_guard_start();
    roon_start();
    knob_start();
    touch_start();
    motion_start();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WIFI_WATCH_PERIOD_MS));
        if (wifi_offline_ms() > WIFI_LOST_RESTART_MS) {
            ESP_LOGW(TAG, "Wi-Fi lost for %d s, restarting", WIFI_LOST_RESTART_MS / 1000);
            esp_restart();
        }
    }
}
