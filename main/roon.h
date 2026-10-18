#pragma once

// Runs the Roon extension forever: discover Core, pair, follow the zone chosen
// in the extension settings and show its cover. Call once from app_main.
void roon_start(void);

// Playback controls for the selected zone. Safe to call from any task: they
// only queue a request for the Roon task, and are dropped while unpaired.
typedef enum { ROON_CONTROL_PLAYPAUSE, ROON_CONTROL_NEXT } roon_control_t;
void roon_control(roon_control_t control);
// Positive steps raise the volume, in the output's own step size.
void roon_change_volume(int steps);
