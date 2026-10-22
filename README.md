# CoverArt ESP32

把 ESP32 小屏变成 Roon 专辑封面显示器。固件本身就是一个 Roon 扩展：直接连接 Roon Core，不需要电脑、Home Assistant 或其他服务常驻。

Turn an ESP32 display into a Roon album-art screen. The firmware is itself a Roon extension and talks to Roon Core directly; no computer, Home Assistant or other service needs to keep running.

**安装 Install:** <https://epochaudio.github.io/CoverArt-ESP32/>（电脑版 Chrome / Edge，USB 连接 · desktop Chrome or Edge over USB）

**使用说明书 Manual:** [中文](docs/manual.md) · [English](docs/manual.en.md)

## 功能 Features

- 自动发现 Roon Core，在 Roon 中启用一次即可，断电后自动重连 · Finds Roon Core on its own; enable once in Roon, reconnects after power loss
- 在 Roon 扩展设置中选择 Zone，跟随播放显示封面；暂停或停止时保留封面 · Pick the zone in the extension settings; the cover follows playback and stays up when paused or stopped
- 手机扫码配置 Wi-Fi，中英双语页面 · Wi-Fi setup from a phone by scanning a QR code, bilingual page
- 旋钮板：转动调音量，单击播放/暂停，双击下一首 · Knob boards: turn for volume, click to play/pause, double-click for next track
- 可同时接入多块屏，各自选择 Zone · Several displays can be paired at once, each following its own zone

## 支持的开发板 Supported boards

| 开发板 Board | 屏幕 Display | 控制 Controls |
|---|---|---|
| [ESP32-C3-LCDkit](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/) | 1.28" 240×240 圆屏 round | 旋钮 Knob |
| [ESP32-S3-BOX-Lite](https://github.com/espressif/esp-box) | 2.4" 320×240 | — |
| [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm) | 2.16" 480×480 AMOLED | 开发中 In progress |

更多开发板陆续支持，欢迎贡献，见 [docs/adding-a-board.md](docs/adding-a-board.md)。· More boards are coming; contributions welcome.

## 使用 Getting started

1. 打开安装页面，选择开发板，USB 连接后点击安装。· Open the installer, choose your board, connect over USB and click Install.
2. 屏幕显示二维码：手机扫码加入 `CoverArt-XXXXXX`，在弹出的页面选择 2.4 GHz Wi-Fi 并输入密码（未弹出时打开 `192.168.4.1`）。· Scan the QR code to join `CoverArt-XXXXXX` and pick your 2.4 GHz Wi-Fi on the page that opens (or browse to `192.168.4.1`).
3. Roon → 设置 → 扩展，启用 `CoverArt_ESP32_<开发板>`。· In Roon, Settings → Extensions, enable `CoverArt_ESP32_<board>`.
4. 点该扩展的“设置”，选择 Zone。· Open its Settings and choose a zone.

**换路由器 New router:** 已存的 Wi-Fi 连续 3 分钟连不上时，设备自动重新进入配网。· If the saved Wi-Fi is unreachable for 3 minutes, the device opens setup again by itself.

**恢复出厂 Factory reset:** 插电后 5 秒内拔掉，重复两次；第三次插电时清除 Wi-Fi、Roon 配对和 Zone 设置。· Plug in and unplug within 5 seconds, twice; the third plug-in erases Wi-Fi, Roon pairing and zone.

## 从源码构建 Building from source

需要 ESP-IDF v6.0.1。· Requires ESP-IDF v6.0.1.

```sh
. $IDF_PATH/export.sh
tools/build.sh c3-lcdkit          # -> dist/c3-lcdkit/coverart-c3-lcdkit.bin
esptool --port <port> write-flash 0x0 dist/c3-lcdkit/coverart-c3-lcdkit.bin
```

`coverart-<board>.bin` 是从 0x0 开始的完整镜像；`coverart-<board>-app.bin` 只含应用。· `coverart-<board>.bin` is a full image from 0x0; `coverart-<board>-app.bin` is the app alone.

结构 Layout: `main/` 共用固件（Roon 协议、配网、显示）· shared firmware; `boards/<board>/` 每块板的引脚、屏幕驱动、分区表与配置 · per-board pins, panel driver, partitions and config; `web/` 安装页面 · installer page。

## 许可 License

MIT，见 [LICENSE](LICENSE)；第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

本项目与 Roon Labs 无关，Roon 是 Roon Labs LLC 的商标。· Not affiliated with or endorsed by Roon Labs; Roon is a trademark of Roon Labs LLC.
