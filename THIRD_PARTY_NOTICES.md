# Third-party notices

CoverArt-ESP32 is MIT licensed (see `LICENSE`). Released firmware images also
contain the following third-party software, under their own licenses.

| Component | Used for | License |
|---|---|---|
| [ESP-IDF](https://github.com/espressif/esp-idf) v6.0.1 (Espressif) | SDK: Wi-Fi, TCP/IP (lwIP), HTTP server, NVS, drivers, bootloader | Apache-2.0, with bundled parts under their own permissive licenses |
| [esp_websocket_client](https://components.espressif.com/components/espressif/esp_websocket_client) (Espressif) | WebSocket link to Roon Core | Apache-2.0 |
| [cJSON](https://github.com/DaveGamble/cJSON) via espressif/cjson | Parsing Roon messages | MIT |
| [qrcode](https://components.espressif.com/components/espressif/qrcode) (Espressif), including [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki | Wi-Fi setup QR code | Apache-2.0 (wrapper), MIT (generator) |
| [esp_lcd_gc9a01](https://components.espressif.com/components/espressif/esp_lcd_gc9a01) (Espressif), C3-LCDkit only | Round panel driver | Apache-2.0 |
| TJpgDec by ChaN, in the ESP32 chip ROM | Decoding cover JPEGs | ChaN's permissive license; executed from ROM, not redistributed |
| 5x7 bitmap font from the Hermes Gadget SDK (`main/font5x7.c`) | On-screen text | MIT, see `LICENSE.hermes-gadget` |

The Roon extension protocol (SOOD discovery, MOO messages) is implemented from
scratch with the CoverArt for Android client as reference; no Roon Labs code is
included. Roon is a trademark of Roon Labs LLC; this project is not affiliated
with or endorsed by Roon Labs.
