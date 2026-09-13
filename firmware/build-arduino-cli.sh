#!/usr/bin/env bash
# Arduino-CLI build for esp-status (no PlatformIO/pip needed).
# Adafruit_ST7789 + Adafruit_GFX build - no extra -D flags required.
# Usage: ./build-arduino-cli.sh [--upload-port /dev/ttyUSB0] [--upload]
set -euo pipefail
cd "$(dirname "$0")"

SKETCH=/tmp/esp-status-sketch/esp-status
rm -rf /tmp/esp-status-sketch
mkdir -p "$SKETCH"
cp src/main.cpp src/ui.cpp src/ui.h src/config.h "$SKETCH/"
printf '// esp-status sketch - sources in main.cpp/ui.cpp\n' > "$SKETCH/esp-status.ino"

export PATH="$HOME/.local/bin:$PATH"
if [[ "${1:-}" == "--upload" ]]; then
  PORT="${2:-${UPLOAD_PORT:-/dev/ttyUSB0}}"
  arduino-cli upload -p "$PORT" --fqbn esp32:esp32:esp32 "$SKETCH"
else
  arduino-cli compile --fqbn esp32:esp32:esp32 "$SKETCH"
fi
