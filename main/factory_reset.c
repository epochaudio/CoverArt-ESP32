#include "factory_reset.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "store.h"
#include "ui.h"

static const char *TAG = "reset";

#define FACTORY_RESET_POWER_CYCLES  3
#define FACTORY_RESET_WINDOW_MS     5000  // a power-on lasting longer ends the sequence
#define FACTORY_RESET_NOTICE_MS     2000

static void window_passed(void *arg)
{
    store_clear_power_cycles();
}

void factory_reset_now(void)
{
    ESP_LOGW(TAG, "factory reset");
    const char *lines[] = {"FACTORY RESET", "", "Settings", "erased"};
    ui_message(lines, 4);
    store_erase_all();
    vTaskDelay(pdMS_TO_TICKS(FACTORY_RESET_NOTICE_MS));
    esp_restart();
}

void factory_reset_check(void)
{
    if (esp_reset_reason() != ESP_RST_POWERON) {
        store_clear_power_cycles();
        return;
    }
    int cycles = store_bump_power_cycles();
    if (cycles >= FACTORY_RESET_POWER_CYCLES) {
        ESP_LOGW(TAG, "%d quick power-ons", cycles);
        factory_reset_now();
    }
    ESP_LOGI(TAG, "quick power-on %d of %d", cycles, FACTORY_RESET_POWER_CYCLES);
    const esp_timer_create_args_t args = {.callback = window_passed, .name = "reset_window"};
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) esp_timer_start_once(timer, FACTORY_RESET_WINDOW_MS * 1000ULL);
}
