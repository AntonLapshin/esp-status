# esp-status — ESP32 build + flash shortcuts (IdeaSpark ESP32, CH340, ST7789)
#
#   make upload              compile + flash to auto-detected port (default /dev/ttyUSB0)
#   make upload PORT=/dev/ttyACM0
#   make build               compile-check only (no device needed)
#   make monitor             serial log @ 115200 (Ctrl+C to exit)
#   make check               verify toolchain + serial port
#
# UPLOAD_PORT env is honored as a fallback for PORT (compat with build-arduino-cli.sh).

PORT      ?= $(firstword $(wildcard /dev/ttyUSB* /dev/ttyACM*) $(UPLOAD_PORT) /dev/ttyUSB0)
FQBN      := esp32:esp32:esp32
SKETCH    := /tmp/esp-status-sketch/esp-status
BUILD_DIR := /tmp/esp-status-sketch/build
BAUD      := 115200

.PHONY: help build upload monitor check

help: ## Show this help
	@echo "Targets:"
	@echo "  make upload [PORT=/dev/ttyUSB0]  compile + flash firmware to ESP32"
	@echo "  make build                       compile-check only (no device needed)"
	@echo "  make monitor [PORT=...]          serial log @ 115200 baud"
	@echo "  make check                       verify arduino-cli core/libs + serial port"
	@echo ""
	@echo "Current PORT: $(PORT)"

check: ## Verify toolchain and serial port
	@arduino-cli version
	@arduino-cli core list | grep -E 'esp32:esp32' || (echo "Missing esp32 core: run 'arduino-cli core install esp32:esp32'"; exit 1)
	@arduino-cli lib list | grep -Ei 'GFX|ST77|ArduinoJson' || echo "WARNING: expected Adafruit GFX / ST7789 / ArduinoJson libs (see README)"
	@echo "PORT=$(PORT)"
	@ls -l $(PORT) || (echo "No serial port at $(PORT). Plug in via USB data cable, then: ls /dev/ttyUSB* /dev/ttyACM*"; exit 1)

# Stage sources into a sketch dir (mirrors firmware/build-arduino-cli.sh layout).
$(SKETCH): firmware/src/main.cpp firmware/src/ui.cpp firmware/src/ui.h firmware/src/config.h
	@rm -rf /tmp/esp-status-sketch
	@mkdir -p $(SKETCH)
	@cp firmware/src/main.cpp firmware/src/ui.cpp firmware/src/ui.h firmware/src/config.h $(SKETCH)/
	@printf '// esp-status sketch - sources in main.cpp/ui.cpp\n' > $(SKETCH)/esp-status.ino

build: $(SKETCH) ## Compile firmware (no device needed)
	arduino-cli compile --fqbn $(FQBN) --output-dir $(BUILD_DIR) $(SKETCH)

upload: $(SKETCH) ## Compile + flash firmware to ESP32 (override: make upload PORT=/dev/ttyACM0)
	@if [ ! -e "$(PORT)" ]; then echo "No serial port at $(PORT). Plug in via USB data cable, then: ls /dev/ttyUSB* /dev/ttyACM*"; exit 1; fi
	arduino-cli compile --fqbn $(FQBN) --output-dir $(BUILD_DIR) $(SKETCH)
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) --input-dir $(BUILD_DIR) $(SKETCH)
	@echo "Flashed $(PORT). Run 'make monitor PORT=$(PORT)' to verify boot log."

monitor: ## Serial log @ 115200 baud (Ctrl+C to exit)
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)
