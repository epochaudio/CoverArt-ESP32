#include "touch.h"

#include "config.h"

#if BOARD_HAS_TOUCH

#include <math.h>
#include <stdlib.h>
#include "board_interface.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "factory_reset.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input.h"
#include "roon.h"
#include "ui.h"

static const char *TAG = "touch";

#define TOUCH_POLL_MS          20
#define TOUCH_TASK_STACK       4096
// Distances are fractions of the panel width so they hold on any panel size.
#define TAP_SLOP_PX            (LCD_WIDTH / 20)   // more movement than this is not a tap
#define TAP_MAX_MS             400
#define SWIPE_MIN_PX           (LCD_WIDTH / 5)
#define SWIPE_MAX_MS           800
#define LONG_PRESS_MS          10000
#define CONFIRM_WINDOW_MS      10000

typedef enum {
    STATE_IDLE,
    STATE_PRESSED,         // finger down, gesture not decided yet
    STATE_IGNORE,          // touch only woke the screen; wait for release
    STATE_CONFIRM_RELEASE, // reset question shown, waiting for the holding finger to lift
    STATE_CONFIRM,         // waiting for a tap on OK
} state_t;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

typedef struct {
    state_t state;
    int x0, y0;            // where the finger went down
    int x, y;              // latest position
    int64_t down_ms;
    int max_travel;
    int64_t confirm_deadline_ms;
} gesture_t;

static void on_release(gesture_t *g, int64_t now)
{
    const int dx = g->x - g->x0, dy = g->y - g->y0;
    const int64_t held = now - g->down_ms;
    ESP_LOGI(TAG, "up at %d,%d after %lld ms (from %d,%d)", g->x, g->y, held, g->x0, g->y0);
    if (g->max_travel <= TAP_SLOP_PX && held <= TAP_MAX_MS) {
        input_control(INPUT_SOURCE_TOUCH, ROON_CONTROL_PLAYPAUSE);
    } else if (abs(dx) >= SWIPE_MIN_PX && abs(dx) > 2 * abs(dy) && held <= SWIPE_MAX_MS) {
        input_control(INPUT_SOURCE_TOUCH, dx < 0 ? ROON_CONTROL_NEXT : ROON_CONTROL_PREVIOUS);
    }
}

static void step(gesture_t *g, bool down, int x, int y, int64_t now)
{
    if (down) {
        g->x = x;
        g->y = y;
    }
    switch (g->state) {
    case STATE_IDLE:
        if (!down) break;
        input_set_touching(true);
        if (!input_begin(INPUT_SOURCE_TOUCH)) {
            g->state = STATE_IGNORE;
            break;
        }
        ESP_LOGI(TAG, "down at %d,%d", x, y);
        g->x0 = x;
        g->y0 = y;
        g->down_ms = now;
        g->max_travel = 0;
        g->state = STATE_PRESSED;
        break;

    case STATE_PRESSED:
        if (!down) {
            on_release(g, now);
            g->state = STATE_IDLE;
            break;
        }
        {
            const int travel = (int)hypotf((float)(x - g->x0), (float)(y - g->y0));
            if (travel > g->max_travel) g->max_travel = travel;
        }
        if (g->max_travel <= TAP_SLOP_PX && now - g->down_ms >= LONG_PRESS_MS) {
            ESP_LOGW(TAG, "long press: asking to confirm factory reset");
            ui_confirm_reset();
            g->state = STATE_CONFIRM_RELEASE;
        }
        break;

    case STATE_IGNORE:
        if (!down) g->state = STATE_IDLE;
        break;

    case STATE_CONFIRM_RELEASE:
        if (!down) {
            g->confirm_deadline_ms = now + CONFIRM_WINDOW_MS;
            g->state = STATE_CONFIRM;
        }
        break;

    case STATE_CONFIRM:
        if (down) {
            g->x0 = x;  // remember where the confirming tap lands
            g->y0 = y;
            g->down_ms = now;
        } else if (g->down_ms > g->confirm_deadline_ms - CONFIRM_WINDOW_MS) {
            if (ui_confirm_hit(g->x0, g->y0)) factory_reset_now();  // does not return
            ESP_LOGI(TAG, "factory reset cancelled");
            roon_redraw();
            g->state = STATE_IDLE;
        } else if (now > g->confirm_deadline_ms) {
            ESP_LOGI(TAG, "factory reset not confirmed in time");
            roon_redraw();
            g->state = STATE_IDLE;
        }
        break;
    }
    if (!down && g->state == STATE_IDLE) input_set_touching(false);
}

static void touch_task(void *arg)
{
    gesture_t g = {.state = STATE_IDLE};
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
        uint16_t x = 0, y = 0;
        const bool down = board_touch_read(&x, &y);
        step(&g, down, x, y, now_ms());
    }
}

void touch_start(void)
{
    xTaskCreate(touch_task, "touch", TOUCH_TASK_STACK, NULL, 4, NULL);
}

#else

void touch_start(void) {}

#endif
