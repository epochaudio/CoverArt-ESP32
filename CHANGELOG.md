# Changelog

## 0.4.1

- Paired devices no longer show "enable in Roon" while reconnecting; the prompt
  appears only for a new pairing or if Core stays silent for 5 s.
- "NOTHING PLAYING" screen when the selected zone is stopped and no cover has
  been shown yet.
- Extension name per board: `CoverArt_ESP32_<board>`.

## 0.4.0

First public layout of the project.

- Boards as components under `boards/`; ESP32-C3-LCDkit and ESP32-S3-BOX-Lite.
- Wi-Fi setup: open `CoverArt-XXXXXX` network, QR code on screen, captive
  portal page in Chinese and English. Setup reopens if the saved network is
  unreachable for 3 minutes.
- Factory reset by three quick power-ons.
- Roon Core found by SOOD; the last working address is kept as a fallback.
- Status line in Roon > Settings > Extensions.
- Two OTA app slots on every board, single merged image for installation.
- Knob on C3-LCDkit: volume, play/pause, next track.
