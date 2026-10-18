#pragma once

// Rotary knob on boards that have one (BOARD_HAS_KNOB):
//   turn clockwise / counter-clockwise -> volume up / down
//   single click -> play/pause, double click -> next track
// Turning while pressed only changes volume; the release is not a click.
void knob_start(void);
