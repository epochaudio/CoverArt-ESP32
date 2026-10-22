#pragma once
#include <stdbool.h>

// Runs the Roon extension forever: discover Core, pair, follow the zone chosen
// in the extension settings and show its cover. Call once from app_main.
void roon_start(void);

// Playback controls for the selected zone. Safe to call from any task: they
// only queue a request for the Roon task, and are dropped while unpaired.
// PLAY and PAUSE are idempotent; use them when the intent is a state, not a toggle.
typedef enum {
    ROON_CONTROL_PLAYPAUSE,
    ROON_CONTROL_PLAY,
    ROON_CONTROL_PAUSE,
    ROON_CONTROL_NEXT,
    ROON_CONTROL_PREVIOUS,
} roon_control_t;
void roon_control(roon_control_t control);
// Positive steps raise the volume, in the output's own step size.
void roon_change_volume(int steps);

// Playback state of the selected zone, for gestures that act on state.
typedef struct {
    bool known;       // false before the first zone report and while Core is unreachable
    bool playing, paused;
    char track[96];   // title of the current track ("" if none)
} roon_playback_t;
roon_playback_t roon_playback(void);

// Redraws the current cover, or the current status screen if no cover is up.
// For overlays (volume, confirmations) that need to give the screen back.
void roon_redraw(void);
