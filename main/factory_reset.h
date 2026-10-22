#pragma once

// Factory reset without buttons, so no knob or key can trigger it by
// accident: power the device on, unplug it within FACTORY_RESET_WINDOW_MS,
// and repeat; the FACTORY_RESET_POWER_CYCLES-th quick power-on erases Wi-Fi,
// Roon pairing and zone, then the device restarts into Wi-Fi setup.
// Only cold power-ons count; restarts, crashes and flashing do not.
// Call once at boot, after store_init() and display_init().
// Battery-powered boards never see these cold power-ons; touch boards offer
// a long press on the screen instead (main/touch.c).
void factory_reset_check(void);

// Erases Wi-Fi, Roon pairing and zone now, says so on screen and restarts.
void factory_reset_now(void);
