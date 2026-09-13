#!/usr/bin/env bash
# Arduino-CLI build for esp-status (no PlatformIO/pip needed).
# Usage: ./build-arduino-cli.sh [--upload-port /dev/ttyUSB0] [--upload]
set -euo pipefail
cd "$(dirname "$0")"

SKETCH=/tmp/esp-status-sketch/esp-status
rm -rf /tmp/esp-status-sketch
mkdir -p "$SKETCH"
cp src/main.cpp src/ui.cpp src/ui.h src/config.h "$SKETCH/"
printf '// esp-status sketch - sources in main.cpp/ui.cpp\n' > "$SKETCH/esp-status.ino"

FLAGS="-DST7789_DRIVER=1 -DTFT_WIDTH=170 -DTFT_HEIGHT=320 -DTFT_MISO=-1 \
 -DTFT_MOSI=23 -DTFT_SCLK=18 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=4 -DTFT_BL=32 \
 -DTFT_BACKLIGHT_ON=1 -DTFT_INVERSION_ON=1 -DLOAD_GLCD=1 -DLOAD_FONT2=1 \
 -DLOAD_FONT4=1 -DLOAD_FONT6=1 -DLOAD_FONT7=1 -DLOAD_FONT8=1 -DLOAD_GFXFF=1 \
 -DSPI_FREQUENCY=40000000"

export PATH="$HOME/.local/bin:$PATH"
if [[ "${1:-}" == "--upload" ]]; then
  PORT="${2:-${UPLOAD_PORT:-/dev/ttyUSB0}}"
  arduino-cli upload -p "$PORT" --fqbn esp32:esp32:esp32 "$SKETCH"
else
  # shellcheck disable=SC2086
  arduino-cli compile --fqbn esp32:esp32:esp32 \
    --build-property "build.extra_flags=$FLAGS" "$SKETCH"
fi
