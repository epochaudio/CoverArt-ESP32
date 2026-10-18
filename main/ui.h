#pragma once

// Full-screen text and setup screens, laid out to stay inside a round panel.
// Text is ASCII only (5x7 font); localised wording lives on the setup page.

// Centred lines of text on black, at most UI_MAX_LINES of ~14 characters.
#define UI_MAX_LINES 5
void ui_message(const char *const lines[], int count);

// Setup screen: a QR code that joins the open setup network, plus its name.
void ui_setup(const char *ap_ssid);
