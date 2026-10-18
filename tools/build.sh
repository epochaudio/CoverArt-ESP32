#!/usr/bin/env bash
# Builds one board and writes a single flashable image (bootloader, partition
# table, OTA data and app merged from offset 0) to dist/<board>/.
# Usage: tools/build.sh <board>     (ESP-IDF environment must be exported)
set -euo pipefail

board="${1:?usage: tools/build.sh <board>}"
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/$board"
version="$(head -n1 "$root/version.txt")"
out="$root/dist/$board"

idf.py -C "$root" -B "$build" -D COVERART_BOARD="$board" build
mkdir -p "$out"
(cd "$build" && python -m esptool --chip "$(grep -E '^CONFIG_IDF_TARGET=' sdkconfig | cut -d'"' -f2)" \
    merge-bin -o "$out/coverart-$board.bin" @flash_args)
cp "$build/coverart.bin" "$out/coverart-$board-app.bin"   # app only, for OTA
echo "$version" > "$out/version.txt"
echo "built $board $version -> $out"
