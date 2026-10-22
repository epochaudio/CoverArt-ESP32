#include "power.h"

#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roon.h"

#define POWER_POLL_MS        30000
#define POWER_TASK_STACK     3072
#define LOG_PERCENT_STEP     5      // log when the level moves this much

static board_battery_t s_status;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

board_battery_t power_status(void)
{
    taskENTER_CRITICAL(&s_lock);
    board_battery_t st = s_status;
    taskEXIT_CRITICAL(&s_lock);
    return st;
}

bool power_low(void)
{
    const board_battery_t st = power_status();
    return st.battery && !st.usb_power && st.percent >= 0 && st.percent <= POWER_LOW_PERCENT;
}

#if BOARD_HAS_BATTERY

static const char *TAG = "power";

static void power_task(void *arg)
{
    board_battery_t logged = {.percent = -100};
    bool was_low = false;
    for (;;) {
        board_battery_t st;
        if (board_battery_read(&st)) {
            taskENTER_CRITICAL(&s_lock);
            s_status = st;
            taskEXIT_CRITICAL(&s_lock);
            if (st.usb_power != logged.usb_power || st.battery != logged.battery || st.charging != logged.charging ||
                st.percent >= logged.percent + LOG_PERCENT_STEP || st.percent <= logged.percent - LOG_PERCENT_STEP) {
                ESP_LOGI(TAG, "usb %s, battery %s, %d%%, %d mV%s", st.usb_power ? "yes" : "no",
                         st.battery ? "yes" : "no", st.percent, st.millivolts, st.charging ? ", charging" : "");
                logged = st;
            }
            if (power_low() != was_low) {
                was_low = !was_low;
                roon_redraw();  // show or remove the battery badge
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
    }
}

void power_start(void)
{
    xTaskCreate(power_task, "power", POWER_TASK_STACK, NULL, 2, NULL);
}

#else

void power_start(void) {}

#endif
