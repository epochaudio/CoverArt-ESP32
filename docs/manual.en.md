# CoverArt ESP32 User Manual

[中文](manual.md) · English

Firmware: 0.6.0

> In 0.5.x the C3-LCDkit and BOX-Lite code was reworked along with the board layer and has not yet been verified on hardware. Users who need these boards to be dependable can stay on 0.4.1.

CoverArt ESP32 turns a small ESP32 display into an album-art screen for Roon. The device is itself a Roon extension: power it up, connect it to Wi-Fi, enable it once in Roon, and from then on it shows the cover of whatever plays in the zone you chose. No computer, phone app or Home Assistant needs to keep running.

---

## 1. Requirements

- A running Roon Core on your local network (Roon Server or Roon on a desktop).
- 2.4 GHz Wi-Fi on the same network as the Roon Core. 5 GHz is not supported.
- Installing the firmware needs a computer with Chrome or Edge and a USB data cable. Devices that come with the firmware installed can skip this.

## 2. Supported devices

| | ESP32-C3-LCDkit | ESP32-S3-BOX-Lite | Waveshare ESP32-S3-Touch-AMOLED-2.16 |
|---|---|---|---|
| Display | 1.28" 240×240 round LCD | 2.4" 320×240 LCD | 2.16" 480×480 AMOLED |
| Album art | ✓ (fills the round screen, corners cropped) | ✓ | ✓ |
| Controls | Knob | — (display only) | Touch and motion |
| Volume on the device | ✓ (knob) | — | — (use Roon or your amplifier) |
| Auto dim / screen off | — | — | ✓ |
| Burn-in protection | — | — | ✓ |
| Battery | — | — | ✓ |
| Factory reset | Power cycling | Power cycling | Hold the screen for 10 s (or power cycling) |

## 3. Installing the firmware

Skip this section if the firmware is already installed.

1. Open <https://epochaudio.github.io/CoverArt-ESP32/> in desktop Chrome or Edge.
2. Choose your board, connect the device over USB, click **Install** and pick the device's serial port from the list.
3. When asked whether to erase the device, choose to erase. Writing takes about a minute.

> Installing from the web page erases all settings on the device (Wi-Fi, Roon pairing, zone). Go through section 4 again afterwards.

## 4. First-time setup

### 4.1 Connecting to Wi-Fi

A device with no saved Wi-Fi starts in setup mode:

1. The screen shows **WI-FI SETUP**, a QR code and the network name **CoverArt-XXXXXX** (the last six characters differ per device).
2. Scan the QR code with your phone's camera to join that network (no password), or pick it from the phone's Wi-Fi list.
3. The setup page opens by itself. If it doesn't, browse to `http://192.168.4.1`.
4. Choose your home Wi-Fi (or type its name), enter the password and tap **Connect**.
5. The device tries to join for about 20 seconds:
   - Success: the screen shows **WI-FI OK / Restarting**, the device restarts, the setup network closes and your phone returns to its usual network.
   - Failure: the QR code comes back. Join the setup network again; the page says the connection failed. Check the password and try again.

Only 2.4 GHz networks are listed. Network names and passwords with non-ASCII characters work.

### 4.2 Enabling the extension in Roon

1. In Roon, open **Settings → Extensions**.
2. Find this device’s extension (publisher “门耳朵制作”) and click **Enable**. The AMOLED-2.16 is listed as **CoverArt Mini**, other boards as **CoverArt_ESP32_<board>**.
3. Open the extension's **Settings**, choose the zone to follow under **Select playback zone**, and save. On the AMOLED-2.16 this is also where **屏幕亮度 Brightness** is set (20 %–100 %, default 100 %); it applies as soon as you save.

The screen shows the zone's current cover right away. Every device is its own extension in Roon, so several devices can run at once, each following a different zone.

In the extension list, a status line under the device reads, for example:

> 已连接 Roon Core，显示 HQ · 电量 80%（充电中）

(“Connected to Roon Core, showing HQ · battery 80% (charging)”.) The battery part only appears on devices with a battery. The status line is in Chinese in this release.

### 4.3 Screen messages

| On screen | Meaning | What to do |
|---|---|---|
| WI-FI SETUP and a QR code | Setup mode | Set up Wi-Fi as in 4.1 |
| CONNECTING WI-FI | Joining the saved Wi-Fi | Wait; after 3 minutes without success the device goes back to setup mode |
| FINDING ROON CORE | Looking for the Roon Core on the network | Wait |
| ROON CORE NOT FOUND / Retrying | No Roon Core found yet; keeps trying | Check that the Roon Core is running and on the same network |
| In Roon open Settings > Extensions, enable CoverArt Mini (CoverArt_ESP32 on other boards) | Waiting to be enabled in Roon | Enable the extension as in 4.2 |
| In Roon extension settings, choose a zone | Enabled, no zone chosen | Choose a zone in the extension's settings |
| NOTHING PLAYING | Nothing plays in the chosen zone | Start playback in Roon |

## 5. Everyday use

### 5.1 Album art

- A new track's cover appears within about a second.
- When playback is paused or stopped, the last cover stays on screen.
- A track without artwork shows a black screen.
- After a power or network outage the device reconnects to Roon by itself; no need to enable it again.

### 5.2 Touch (AMOLED-2.16)

| Gesture | Action |
|---|---|
| Tap | Play / pause |
| Swipe left | Next track |
| Swipe right | Previous track |
| Hold for 10 seconds | Factory reset, after confirmation (see section 8) |

- When the screen is dimmed or off, the first touch only wakes it.
- This device has no volume control; use the Roon app or your amplifier.

### 5.3 Knob (C3-LCDkit)

| Action | Result |
|---|---|
| Turn right / left | Volume up / down, one step per detent |
| Click | Play / pause |
| Double-click | Next track |
| Turn while pressed | Volume only; releasing does not count as a click |

### 5.4 Motion (AMOLED-2.16)

| Movement | Action |
|---|---|
| Lay the device screen-down (about 1 second) | Pause if playing, and turn the screen off |
| Turn it screen-up again | Wake the screen; resume playback when the conditions below are met |
| Pick up or move the device | Wake a dimmed or dark screen |

Turning the device back up resumes playback only if all of these hold, so it never starts playing unexpectedly:

- the pause was caused by laying it screen-down;
- no more than 30 minutes have passed;
- nobody resumed playback or changed the track in Roon meanwhile.

Motion gestures are ignored while a finger is on the screen.

### 5.5 Volume safety (C3-LCDkit knob)

- Each detent moves one step of the size Roon uses for that output; on a dB output one step is 1 dB.
- Volume rises by at most 6 steps per second (6 dB on a dB output), however fast the knob turns. Lowering is not limited.
- If the chosen output has fixed volume in Roon, the knob does not change the volume.

## 6. Screen and burn-in protection (AMOLED-2.16)

An AMOLED screen that shows the same image for a long time can keep a ghost of it. The device protects itself:

| Situation | Screen |
|---|---|
| Playing | Normal brightness: the level chosen in the extension settings (default 100 %) |
| Stopped or paused for 5 minutes | Dimmed (20 %, never above the chosen level) |
| Stopped or paused for 15 minutes | Off |
| Lying screen-down | Off at once |
| Roon Core unreachable | Treated as stopped: dims and turns off on the same schedule |

- Touching the screen, picking up the device, playback starting in Roon or a track change all wake the screen.
- While playing, the cover moves by 2 pixels every 3 minutes, round a small circle. You will hardly notice it; it keeps pixels from wearing unevenly.

The C3-LCDkit and BOX-Lite use LCDs and stay lit.

## 7. Battery (AMOLED-2.16)

- With USB connected, the battery charges.
- The battery level appears in the Roon extension status line, updated in 5 % steps, with a note while charging.
- On battery at 20 % or less:
  - a red battery icon appears at the top centre of the screen;
  - brightness is limited to 50 %.
- A new battery may show an inaccurate percentage for its first few charge cycles.

## 8. Factory reset

A factory reset erases the Wi-Fi settings, the Roon pairing, the chosen zone and the remembered Roon Core address. The device then starts in setup mode; you may need to enable the extension in Roon again.

**Method 1: hold the screen (AMOLED-2.16)**

1. Hold a finger on the screen for 10 seconds without moving it.
2. The screen shows **FACTORY RESET?** and an **OK** button. Lift your finger.
3. Tap **OK** within 10 seconds. Tapping elsewhere or waiting cancels and returns to the cover.

**Method 2: power cycling (all devices)**

1. Plug in the power and unplug it within 5 seconds.
2. Do it once more: plug in, unplug within 5 seconds.
3. On the third plug-in the device resets and shows **FACTORY RESET / Settings erased**.

Only a full power-off and power-on counts. An AMOLED-2.16 with a battery does not lose power when USB is unplugged; use method 1.

## 9. Network behaviour

- The device finds the Roon Core through a local network broadcast and remembers the last address it connected to. If the broadcast occasionally fails, it uses the remembered address.
- If the saved Wi-Fi cannot be joined for 3 minutes (a new router, for example), the device goes to setup mode.
- In setup mode it retries the old Wi-Fi every 2 minutes, so a router that was only switched off for a while is picked up again.
- If Wi-Fi drops for more than 3 minutes while running, the device restarts and follows the rules above.

## 10. Troubleshooting

**The device does not appear in Roon's extension list**
Make sure the screen has gone past FINDING ROON CORE and that the device is on the same network as the Roon Core. Some routers and mesh systems filter multicast; restarting the router, or connecting the device and the Roon Core to the same router, can help.

**The screen keeps showing ROON CORE NOT FOUND**
Check that the Roon Core is running. As above, when multicast is blocked the device cannot find the Core the first time; once it has connected, it remembers the address.

**After reinstalling the firmware, Roon asks to enable the extension again**
Installing from the web page erases the pairing. This is expected; enable it as in 4.2.

**The knob does not change the volume**
The chosen output probably has fixed volume. Make it adjustable in Roon's device settings.

**Covers are slow to change, or sometimes do not change**
Usually a weak 2.4 GHz signal. Move the device closer to the router; it also reconnects by itself and fetches the cover again within 10 seconds.

**The setup page does not open by itself**
While the phone is still connected to CoverArt-XXXXXX, browse to `http://192.168.4.1`.

## 11. Privacy and security

- The device talks only to the Roon Core on your local network; it uses no cloud service.
- The Wi-Fi password is stored only on the device.
- The CoverArt-XXXXXX setup network has no password. It is open only in setup mode and closes once Wi-Fi is set up.

## 12. Open source and support

- Project page: <https://github.com/epochaudio/CoverArt-ESP32> (MIT License)
- Bugs and questions: open an issue on the project page with your board, firmware version and what you see.

This project is not affiliated with Roon Labs. Roon is a trademark of Roon Labs LLC.
