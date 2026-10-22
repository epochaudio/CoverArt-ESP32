#include "input.h"

#include "esp_log.h"
#include "screen_guard.h"

static const char *TAG = "input";
static const char *const kSourceNames[] = {
    [INPUT_SOURCE_KNOB] = "knob", [INPUT_SOURCE_TOUCH] = "touch", [INPUT_SOURCE_MOTION] = "motion",
};

static volatile bool s_touching;

bool input_begin(input_source_t source)
{
    if (screen_guard_wake()) {
        ESP_LOGI(TAG, "%s woke the screen", kSourceNames[source]);
        return false;
    }
    return true;
}

void input_control(input_source_t source, roon_control_t control)
{
    if (source == INPUT_SOURCE_MOTION && s_touching) return;
    roon_control(control);
}

void input_volume(input_source_t source, int steps)
{
    if (source == INPUT_SOURCE_MOTION && s_touching) return;
    roon_change_volume(steps);
}

void input_set_touching(bool touching)
{
    s_touching = touching;
}

bool input_touching(void)
{
    return s_touching;
}
