#pragma once

#include "board.h"

// Identity shown in Roon > Settings > Extensions. Each device registers its own
// extension id (base + MAC suffix) so several displays can be paired at once.
// The version shown is the firmware version from version.txt.
#define ROON_EXTENSION_ID_BASE  "com.epochaudio.coverart-esp32"
#define ROON_DISPLAY_NAME_BASE  "CoverArt_ESP32"
#define ROON_PUBLISHER          "门耳朵制作"
#define ROON_EMAIL              "masked"

// SOOD discovery, as used by Roon Core and node-roon-api.
#define SOOD_PORT             9003
#define SOOD_MULTICAST_ADDR   "239.255.90.90"
#define SOOD_ROON_SERVICE_ID  "00720724-5143-4a9b-abac-0e50cba674bb"
#define SOOD_LISTEN_MS        3000
#define SOOD_RESEND_MS        1000

#define ROON_WS_PATH             "/api"

// WebSocket liveness: the client pings and drops a link that stops answering.
#define WS_PING_INTERVAL_SEC     10
#define WS_PINGPONG_TIMEOUT_SEC  30
#define WS_NETWORK_TIMEOUT_MS    10000
#define WS_RX_CHUNK_BYTES        4096

// Messages larger than this are logged, to size MOO_MAX_MESSAGE_BYTES per board.
#define MOO_LARGE_MESSAGE_BYTES  8192

// Reconnect backoff after a lost Core: 2 s doubling up to 30 s.
#define RECONNECT_DELAY_MIN_MS   2000
#define RECONNECT_DELAY_MAX_MS   30000
