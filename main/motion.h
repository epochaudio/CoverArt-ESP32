#pragma once

// Motion gestures on BOARD_HAS_IMU boards:
//   lay face down        pause (if playing) and turn the screen off
//   turn back up         screen on; resume only if face-down paused it, within
//                        MOTION_RESUME_WINDOW_MS, and nothing changed meanwhile
//   pick up or move      wake a dimmed or dark screen
// Ignored while a finger is on the screen.
void motion_start(void);
