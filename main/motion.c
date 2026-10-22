#include "motion.h"

#include "config.h"

#if BOARD_HAS_IMU

#include <math.h>
#include <string.h>
#include "board_interface.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input.h"
#include "roon.h"
#include "screen_guard.h"

static const char *TAG = "motion";

#define MOTION_POLL_MS            40
#define MOTION_TASK_STACK         4096
#define FILTER_ALPHA              0.2f   // low-pass on acceleration, per sample
// Face down: gravity along -z (screen towards the table), held still.
#define FACE_DOWN_Z_G             (-0.8f)
#define FACE_DOWN_HOLD_MS         1000
// Back up: screen clearly no longer facing down.
#define FACE_UP_Z_G               (-0.3f)
#define FACE_UP_HOLD_MS           300
#define MOTION_RESUME_WINDOW_MS   (30 * 60 * 1000)
// Pick-up: the filtered gravity vector moves this far from where it rested.
#define PICKUP_DELTA_G            0.15f
#define REST_ALPHA                0.02f  // how fast the rest position follows
// 1 logs the filtered acceleration, for checking a board's axis mapping.
#define MOTION_LOG_ACCEL          0
#define MOTION_LOG_PERIOD_MS      2000

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

typedef struct {
    float f[3];             // filtered acceleration
    float rest[3];          // slowly following position while still
    bool face_down;
    int64_t candidate_since;  // when the opposite orientation was first seen
    bool flip_paused;       // face-down paused playback...
    int64_t flip_paused_at;
    char flip_track[sizeof(((roon_playback_t *)0)->track)];  // ...of this track
} motion_t;

static void on_face_down(motion_t *m, int64_t now)
{
    m->face_down = true;
    screen_guard_set_face_down(true);
    const roon_playback_t pb = roon_playback();
    m->flip_paused = false;
    if (pb.known && pb.playing) {
        input_control(INPUT_SOURCE_MOTION, ROON_CONTROL_PAUSE);
        m->flip_paused = true;
        m->flip_paused_at = now;
        strlcpy(m->flip_track, pb.track, sizeof(m->flip_track));
    }
    ESP_LOGI(TAG, "face down%s", m->flip_paused ? ": paused" : "");
}

static void on_face_up(motion_t *m, int64_t now)
{
    m->face_down = false;
    screen_guard_set_face_down(false);
    const roon_playback_t pb = roon_playback();
    // Resume only what face-down paused, and only if nobody has since played,
    // changed track or let too much time pass.
    const bool resume = m->flip_paused && now - m->flip_paused_at <= MOTION_RESUME_WINDOW_MS && pb.known &&
                        pb.paused && strcmp(pb.track, m->flip_track) == 0;
    if (resume) input_control(INPUT_SOURCE_MOTION, ROON_CONTROL_PLAY);
    m->flip_paused = false;
    ESP_LOGI(TAG, "face up%s", resume ? ": resumed" : "");
}

static void step(motion_t *m, const float g[3], int64_t now)
{
    for (int i = 0; i < 3; i++) m->f[i] += FILTER_ALPHA * (g[i] - m->f[i]);

    // Orientation, with hold times so a quick wobble does not count.
    const bool looks_down = m->face_down ? m->f[2] < FACE_UP_Z_G : m->f[2] < FACE_DOWN_Z_G;
    if (looks_down == m->face_down) {
        m->candidate_since = 0;
    } else if (!m->candidate_since) {
        m->candidate_since = now;
    } else if (now - m->candidate_since >= (m->face_down ? FACE_UP_HOLD_MS : FACE_DOWN_HOLD_MS)) {
        m->candidate_since = 0;
        if (m->face_down) on_face_up(m, now);
        else if (!input_touching()) on_face_down(m, now);
    }

    // Pick-up: movement away from the rest position wakes the screen.
    const float moved = sqrtf((m->f[0] - m->rest[0]) * (m->f[0] - m->rest[0]) +
                              (m->f[1] - m->rest[1]) * (m->f[1] - m->rest[1]) +
                              (m->f[2] - m->rest[2]) * (m->f[2] - m->rest[2]));
    if (moved > PICKUP_DELTA_G) {
        if (!m->face_down) screen_guard_wake();
        memcpy(m->rest, m->f, sizeof(m->rest));
    } else {
        for (int i = 0; i < 3; i++) m->rest[i] += REST_ALPHA * (m->f[i] - m->rest[i]);
    }
}

static void motion_task(void *arg)
{
    motion_t m = {0};
    float g[3];
    while (!board_imu_read(g)) vTaskDelay(pdMS_TO_TICKS(MOTION_POLL_MS));
    memcpy(m.f, g, sizeof(m.f));
    memcpy(m.rest, g, sizeof(m.rest));
    int64_t last_log = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MOTION_POLL_MS));
        if (!board_imu_read(g)) continue;
        const int64_t now = now_ms();
        step(&m, g, now);
        if (MOTION_LOG_ACCEL && now - last_log >= MOTION_LOG_PERIOD_MS) {
            ESP_LOGI(TAG, "accel x %+.2f y %+.2f z %+.2f", m.f[0], m.f[1], m.f[2]);
            last_log = now;
        }
    }
}

void motion_start(void)
{
    xTaskCreate(motion_task, "motion", MOTION_TASK_STACK, NULL, 3, NULL);
}

#else

void motion_start(void) {}

#endif
