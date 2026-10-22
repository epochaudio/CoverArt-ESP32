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
// Touches starting this far from the centre can drive the volume ring; a
// path that dips well inside it is a swipe instead.
#define RING_INNER_PX          (LCD_WIDTH / 3)
#define RING_LEAVE_PX          (RING_INNER_PX * 3 / 4)
#define TOUCH_ARC_STEP_DEG     12.0f
// One ring gesture can raise the volume by at most this many steps (1 dB
// each on a dB output), so a slip of the finger cannot blast the speakers.
#define ARC_MAX_RAISE_STEPS    10
#define OVERLAY_HOLD_MS        2000
#define LONG_PRESS_MS          10000
#define CONFIRM_WINDOW_MS      10000

typedef enum {
    STATE_IDLE,
    STATE_PRESSED,         // finger down, gesture not decided yet
    STATE_RING,            // dragging along the volume ring
    STATE_IGNORE,          // touch only woke the screen; wait for release
    STATE_CONFIRM_RELEASE, // reset question shown, waiting for the holding finger to lift
    STATE_CONFIRM,         // waiting for a tap on OK
} state_t;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static float angle_deg(int x, int y)
{
    return atan2f((float)(y - LCD_HEIGHT / 2), (float)(x - LCD_WIDTH / 2)) * 180.0f / (float)M_PI;
}

static int distance_from_centre(int x, int y)
{
    return (int)hypotf((float)(x - LCD_WIDTH / 2), (float)(y - LCD_HEIGHT / 2));
}

typedef struct {
    state_t state;
    int x0, y0;            // where the finger went down
    int x, y;              // latest position
    int64_t down_ms;
    int max_travel;
    bool ring_candidate;
    float last_angle, angle_acc;
    int raised_steps;
    int64_t overlay_ms;    // last volume change shown; 0 when no overlay is up
    float overlay_value;
    int64_t confirm_deadline_ms;
} gesture_t;

static void show_volume(gesture_t *g)
{
    ui_volume_overlay();
    g->overlay_value = roon_volume().value;
    g->overlay_ms = now_ms();
}

static void ring_move(gesture_t *g)
{
    const float a = angle_deg(g->x, g->y);
    float delta = a - g->last_angle;
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    g->last_angle = a;
    g->angle_acc += delta;
    int steps = 0;
    while (g->angle_acc >= TOUCH_ARC_STEP_DEG) { g->angle_acc -= TOUCH_ARC_STEP_DEG; steps++; }
    while (g->angle_acc <= -TOUCH_ARC_STEP_DEG) { g->angle_acc += TOUCH_ARC_STEP_DEG; steps--; }
    if (steps > ARC_MAX_RAISE_STEPS - g->raised_steps) steps = ARC_MAX_RAISE_STEPS - g->raised_steps;
    if (steps == 0) return;
    if (steps > 0) g->raised_steps += steps;
    input_volume(INPUT_SOURCE_TOUCH, steps);
    show_volume(g);
}

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
        g->ring_candidate = distance_from_centre(x, y) >= RING_INNER_PX;
        g->last_angle = angle_deg(x, y);
        g->angle_acc = 0;
        g->raised_steps = 0;
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
        if (g->ring_candidate && distance_from_centre(x, y) < RING_LEAVE_PX) g->ring_candidate = false;
        if (g->ring_candidate && g->max_travel > TAP_SLOP_PX) {
            g->state = STATE_RING;
            ring_move(g);
        } else if (g->max_travel <= TAP_SLOP_PX && now - g->down_ms >= LONG_PRESS_MS) {
            ESP_LOGW(TAG, "long press: asking to confirm factory reset");
            ui_confirm_reset();
            g->state = STATE_CONFIRM_RELEASE;
        }
        break;

    case STATE_RING:
        if (down) ring_move(g);
        else g->state = STATE_IDLE;
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
        const int64_t now = now_ms();
        step(&g, down, x, y, now);

        // Keep the volume overlay current while Core reports the new value,
        // then give the screen back.
        if (g.overlay_ms) {
            const roon_volume_t v = roon_volume();
            if (v.value != g.overlay_value) show_volume(&g);
            else if (now - g.overlay_ms >= OVERLAY_HOLD_MS && g.state != STATE_RING) {
                g.overlay_ms = 0;
                roon_redraw();
            }
        }
    }
}

void touch_start(void)
{
    xTaskCreate(touch_task, "touch", TOUCH_TASK_STACK, NULL, 4, NULL);
}

#else

void touch_start(void) {}

#endif
