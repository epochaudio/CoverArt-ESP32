#include "screen_guard.h"

#include "config.h"
#include "cover.h"
#include "display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "screen";

#define GUARD_TICK_MS         1000
#define GUARD_TASK_STACK      4096
#define FULL_BRIGHTNESS       100
// Pixel shift: step around a small square, one step per period, while lit.
#define SHIFT_PERIOD_MS       (3 * 60 * 1000)
#define SHIFT_STEP_PX         2

#if SCREEN_PIXEL_SHIFT
_Static_assert(LCD_WIDTH - COVER_SIZE >= 4 * SHIFT_STEP_PX && LCD_HEIGHT - COVER_SIZE >= 4 * SHIFT_STEP_PX,
               "COVER_SIZE must leave room on each side for the pixel shift");
#endif

typedef enum { LEVEL_ON, LEVEL_DIM, LEVEL_OFF } level_t;

static volatile bool s_playing;
static volatile int64_t s_last_activity_ms;
static volatile level_t s_level = LEVEL_ON;
static SemaphoreHandle_t s_lock;  // level changes come from the guard and the Roon task

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void apply(level_t level)
{
    static const char *names[] = {"on", "dim", "off"};
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (level != s_level) {
        s_level = level;
        display_set_brightness(level == LEVEL_ON ? FULL_BRIGHTNESS : level == LEVEL_DIM ? SCREEN_DIM_PERCENT : 0);
        ESP_LOGI(TAG, "screen %s", names[level]);
    }
    xSemaphoreGive(s_lock);
}

static level_t target_level(int64_t now)
{
    if (s_playing) return LEVEL_ON;
    const int64_t idle = now - s_last_activity_ms;
    if (SCREEN_OFF_AFTER_MS && idle >= SCREEN_OFF_AFTER_MS) return LEVEL_OFF;
    if (SCREEN_DIM_AFTER_MS && idle >= SCREEN_DIM_AFTER_MS) return LEVEL_DIM;
    return LEVEL_ON;
}

#if SCREEN_PIXEL_SHIFT
static void shift_cover(void)
{
    // A ring of offsets around the centre, so no edge stays lit in one place.
    static const int8_t kOffsets[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    static unsigned step;
    step = (step + 1) % (sizeof(kOffsets) / sizeof(kOffsets[0]));
    cover_set_offset(kOffsets[step][0] * SHIFT_STEP_PX, kOffsets[step][1] * SHIFT_STEP_PX);
}
#endif

static void guard_task(void *arg)
{
    int64_t last_shift = now_ms();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(GUARD_TICK_MS));
        const int64_t now = now_ms();
        apply(target_level(now));
#if SCREEN_PIXEL_SHIFT
        if (s_level != LEVEL_OFF && now - last_shift >= SHIFT_PERIOD_MS) {
            shift_cover();
            last_shift = now;
        }
#else
        (void)last_shift;
#endif
    }
}

void screen_guard_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_last_activity_ms = now_ms();
    if (!SCREEN_DIM_AFTER_MS && !SCREEN_OFF_AFTER_MS && !SCREEN_PIXEL_SHIFT) return;  // nothing to guard
    xTaskCreate(guard_task, "screen", GUARD_TASK_STACK, NULL, 2, NULL);
}

void screen_guard_set_playing(bool playing)
{
    if (playing == s_playing) return;
    s_playing = playing;
    s_last_activity_ms = now_ms();  // stopping starts the idle countdown afresh
    if (playing) apply(LEVEL_ON);
}

bool screen_guard_wake(void)
{
    const bool was_dark = s_level != LEVEL_ON;
    s_last_activity_ms = now_ms();
    apply(LEVEL_ON);
    return was_dark;
}
