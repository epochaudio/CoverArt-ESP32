#pragma once
#include <stdbool.h>

// Full-screen text and setup screens, laid out to stay inside a round panel.
// Text is ASCII only (5x7 font); localised wording lives on the setup page.

// Centred lines of text on black, at most UI_MAX_LINES of ~14 characters.
#define UI_MAX_LINES 5
void ui_message(const char *const lines[], int count);

// Setup screen: a QR code that joins the open setup network, plus its name.
void ui_setup(const char *ap_ssid);

// Current volume of the selected output over the middle of the screen.
// Callers restore the screen with roon_redraw() when done.
void ui_volume_overlay(void);

// Battery badge for covers; draws nothing unless the battery is low.
void ui_battery_badge(void);

// Factory-reset question with an OK button; ui_confirm_hit() tests a touch.
void ui_confirm_reset(void);
bool ui_confirm_hit(int x, int y);
