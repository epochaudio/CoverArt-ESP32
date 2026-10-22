#pragma once
#include <stdbool.h>

// Screen power and burn-in policy, per board (see board_interface.h):
//   playing                     -> full brightness
//   idle SCREEN_DIM_AFTER_MS     -> SCREEN_DIM_PERCENT
//   idle SCREEN_OFF_AFTER_MS     -> dark
// "Idle" counts from the last activity: playback stopping, a new cover, a
// touch or the device being picked up. 0 disables a step. On boards with
// SCREEN_PIXEL_SHIFT the cover is moved a few pixels now and then while lit.
void screen_guard_start(void);

// Selected zone is playing (false also while Roon Core is unreachable).
void screen_guard_set_playing(bool playing);

// Device lying screen-down: dark until turned back up (which counts as activity).
void screen_guard_set_face_down(bool face_down);

// Something happened worth looking at. Returns true if the screen was dimmed
// or dark, so an input that only woke it can be swallowed.
bool screen_guard_wake(void);
