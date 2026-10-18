# Adding a board

Everything board-specific lives in `boards/<name>/`; the shared firmware in
`main/` never mentions a board by name. A new board is a new directory:

```
boards/<name>/
  board.cmake          set(COVERART_TARGET esp32xx) - the chip
  board.json           title, chip family (ESP Web Tools name), description, link
  sdkconfig.defaults   flash size, PSRAM, partition table path, memory tuning
  partitions.csv       must keep two OTA app slots (ota_0, ota_1) and otadata
  board/               an ESP-IDF component, always named "board"
    CMakeLists.txt     idf_component_register(... REQUIRES board_interface esp_lcd)
    idf_component.yml  panel driver and other board-only components
    include/board.h    pins and limits, see components/board_interface
    panel.c            board_panel_create(): panel type, orientation, colour order
```

## Steps

1. Copy the closest existing board and rename the directory.
2. Fill in `include/board.h`. The full list of required macros is in
   `components/board_interface/include/board_interface.h`. Take pins from the
   vendor's BSP or schematic and note the source in a comment.
3. Implement `board_panel_create()` in `panel.c` with the panel's esp_lcd
   driver. Mirroring, axis swap, colour inversion and RGB/BGR order are the
   usual quirks; check them on the real panel.
4. Size `MOO_MAX_MESSAGE_BYTES` to the memory: boards without PSRAM need about
   100 KB of free internal heap for a cover plus a zone update.
5. Set `BOARD_HAS_KNOB` to 1 only if the board has a rotary encoder with a
   push switch; other inputs need their own module.
6. Add the board to the matrix in `.github/workflows/build.yml`.
7. Build and flash:

   ```sh
   tools/build.sh <name>
   esptool --port <port> write-flash 0x0 dist/<name>/coverart-<name>.bin
   ```

   Check on the device: setup QR code is readable, Wi-Fi setup completes, the
   extension appears in Roon, covers show with correct colours and orientation,
   and the serial log's heap report stays flat over half an hour.

The first build of a board writes `boards/<name>/dependencies.lock`; commit it
so later builds use the same component versions.
