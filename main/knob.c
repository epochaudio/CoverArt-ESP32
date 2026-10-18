#include "knob.h"

#include "config.h"

#if BOARD_HAS_KNOB

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roon.h"

static const char *TAG = "knob";

// Timing tuned on the C3-LCDkit encoder.
#define KNOB_POLL_MS           10
#define KNOB_DEBOUNCE_MS       30
#define KNOB_MULTI_CLICK_MS    300   // wait after the last release before acting
#define KNOB_DETENT_TRANSITIONS 4    // Gray-code transitions per detent
#define KNOB_TASK_STACK        3072

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static int read_ab(void)
{
    return (gpio_get_level(KNOB_PIN_A) << 1) | gpio_get_level(KNOB_PIN_B);
}

static void on_clicks(int count)
{
    if (count == 1) {
        roon_control(ROON_CONTROL_PLAYPAUSE);
    } else if (count == 2) {
        roon_control(ROON_CONTROL_NEXT);
    } else {
        ESP_LOGI(TAG, "%d clicks ignored", count);
    }
}

static void knob_task(void *arg)
{
    // Index: (previous AB << 2) | current AB. Valid quarter steps are +-1;
    // bounces and impossible two-bit jumps contribute 0.
    static const int8_t kTransitions[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    int ab = read_ab(), quarter_steps = 0;
    bool raw_pressed = gpio_get_level(KNOB_PIN_PRESS) == 0, pressed = raw_pressed, turned_while_pressed = false;
    int64_t raw_changed_at = now_ms(), last_release_at = 0;
    int clicks = 0;

    for (;;) {
        const int64_t now = now_ms();

        int cur = read_ab();
        if (cur != ab) {
            quarter_steps += kTransitions[(ab << 2) | cur];
            ab = cur;
            if (cur == 3) {  // resting position: one detent completed
                int detent = quarter_steps >= KNOB_DETENT_TRANSITIONS ? 1
                           : quarter_steps <= -KNOB_DETENT_TRANSITIONS ? -1 : 0;
                quarter_steps = 0;
                if (detent) {
                    roon_change_volume(detent);
                    clicks = 0;  // turning cancels a pending click sequence
                    if (pressed) turned_while_pressed = true;
                }
            }
        }

        bool level_pressed = gpio_get_level(KNOB_PIN_PRESS) == 0;
        if (level_pressed != raw_pressed) {
            raw_pressed = level_pressed;
            raw_changed_at = now;
        }
        if (raw_pressed != pressed && now - raw_changed_at >= KNOB_DEBOUNCE_MS) {
            pressed = raw_pressed;
            if (pressed) {
                turned_while_pressed = false;
            } else if (!turned_while_pressed) {
                clicks++;
                last_release_at = now;
            }
        }
        if (clicks && !pressed && !raw_pressed && now - last_release_at >= KNOB_MULTI_CLICK_MS) {
            on_clicks(clicks);
            clicks = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(KNOB_POLL_MS));
    }
}

void knob_start(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << KNOB_PIN_A) | (1ULL << KNOB_PIN_B) | (1ULL << KNOB_PIN_PRESS),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    xTaskCreate(knob_task, "knob", KNOB_TASK_STACK, NULL, 4, NULL);
}

#else

void knob_start(void) {}

#endif
