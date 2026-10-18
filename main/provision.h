#pragma once
#include <stdbool.h>

// Wi-Fi setup for an out-of-box device. Opens an open network named
// "CoverArt-XXXXXX" with a captive portal: a phone that joins (by scanning the
// QR code on screen) gets a bilingual page to pick the home network. Once the
// device joins it, the network is saved and the device restarts. While the
// setup network is up, an already saved network is retried periodically, so
// a router that was only down for a while is picked up again on its own.
// Never returns.
void provision_run(void);
