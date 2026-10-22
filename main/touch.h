#pragma once

// Touch gestures on BOARD_HAS_TOUCH boards:
//   tap                         play / pause
//   swipe left / right          next / previous track
//   drag along the outer ring   volume, clockwise up (one step per TOUCH_ARC_STEP_DEG)
//   hold 10 s, then tap OK      factory reset
// The first touch on a dimmed or dark screen only wakes it.
void touch_start(void);
