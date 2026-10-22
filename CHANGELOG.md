# Changelog

## 0.5.0

New board: Waveshare ESP32-S3-Touch-AMOLED-2.16 (480×480 AMOLED, touch,
accelerometer, battery).

- Touch: tap for play/pause, swipe left/right for next/previous track, hold
  10 s and confirm for a factory reset. No on-screen volume; use Roon or the
  amplifier.
- Motion: laying the device screen-down pauses and turns the screen off;
  turning it back up resumes only what that pause stopped, within 30 minutes.
  Moving the device wakes the screen.
- Screen care on AMOLED: dims after 5 minutes and turns off after 15 minutes
  without playback; the cover shifts by 2 px every 3 minutes against burn-in.
- Battery: level and charging in the Roon status line; at 20 % or less on
  battery, a battery badge and brightness capped at 50 %.
- Volume (C3-LCDkit knob) rises by at most 6 steps per second.
- Roon controls gain explicit play, pause and previous.
- Covers no longer flash black when changing.
- Board contract reworked: boards own their display bus, panel and brightness;
  drawing areas are padded to the panel's pixel grid.
- User manual in Chinese and English (docs/).

Verification of this release:

- AMOLED-2.16 tested on hardware: display, touch, face-down pause and resume,
  pixel shift, battery level while charging (the level also fell while
  unplugged, so it ran on battery; behaviour on battery was not watched).
- Not yet seen on hardware: idle dimming and screen-off timing, pick-up wake,
  the low-battery badge.
- **ESP32-C3-LCDkit and ESP32-S3-BOX-Lite were not tested on hardware in this
  release.** Their display and knob code moved to the new board contract and
  only builds have been checked. Users of these boards who need certainty can
  stay on 0.4.1 (images on its GitHub release) until a later release confirms
  them.

Installing from the web page erases the device's settings; set up Wi-Fi and
enable the extension in Roon again afterwards.

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
