#pragma once
#include <stdbool.h>
#include "roon.h"

// Single entry point for every physical input (knob, touch, motion), so the
// rules that span them live in one place:
//   - a gesture that starts while the screen rests only wakes it;
//   - motion gestures are ignored while a finger is on the screen.
typedef enum { INPUT_SOURCE_KNOB, INPUT_SOURCE_TOUCH, INPUT_SOURCE_MOTION } input_source_t;

// Call when a gesture starts. False means the screen was dim or dark and the
// gesture only woke it: ignore the rest of it.
bool input_begin(input_source_t source);

void input_control(input_source_t source, roon_control_t control);
void input_volume(input_source_t source, int steps);

// Touch reports a finger down/up; motion asks before acting.
void input_set_touching(bool touching);
bool input_touching(void);
