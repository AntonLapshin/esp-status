---
name: upload-esp32-firmware
description: Build and flash esp-status firmware to IdeaSpark ESP32 via arduino-cli or PlatformIO, including WiFi config, driver/port checks, and serial verification
---

# Upload ESP32 Firmware (esp-status)

Use this skill to compile and flash `firmware/` in this repo onto the
IdeaSpark ESP32 (16 MB + onboard 1.9" ST7789 170x320 LCD).

## 0. Know the hardware (fixed, no wiring)

- Board: IdeaSpark ESP32, USB-C, USB-serial chip **CH340**.
- Display (onboard, fixed): ST7789, 170x320 portrait.
  `MOSI=23, SCLK=18, CS=15, DC=2, RST=4, BL=32`.
  Firmware drives this in `firmware/src/main.cpp` (`tft.init(170,320)` +
  `invertDisplay(true)`, rotation from `DISPLAY_ROTATION`).
- Backlight pin is GPIO32 at ~50% PWM (`analogWrite(32, 128)`).
- 115200 baud for flash monitor and serial log.
- Most IdeaSpark boards auto-enter download mode; holding BOOT is rarely needed.

## 1. Prerequisites checklist

1. ESP32 plugged into the HOST machine via a **USB data cable**
   (charge-only cables show no serial port — swap cable first).
2. Toolchain — one of:
   - **arduino-cli** (recommended, no Python needed):
     `arduino-cli core install esp32:esp32`
     plus libraries `Adafruit GFX Library`, `Adafruit ST7735 and ST7789 Library`,
     `ArduinoJson` (also `Adafruit BusIO` as dependency, `TFT_eSPI` harmless extra).
   - **PlatformIO**: `pip install platformio` (build defined in `firmware/platformio.ini`,
     env `ideaspark-esp32`, board `esp32dev`, `huge_app.csv` partition).
3. Verify the toolchain before flashing:
   ```bash
   arduino-cli version
   arduino-cli core list        # must show esp32:esp32 (this repo uses 3.3.x)
   arduino-cli lib list         # must show Adafruit GFX, ST7735 and ST7789, ArduinoJson
   ```

## 2. Configure WiFi + server (do this before every flash if network changed)

Edit `firmware/src/config.h` (never commit secrets):

```cpp
#define WIFI_SSID "YourHomeWifi"        // must be 2.4 GHz — ESP32 cannot use 5 GHz
#define WIFI_PASS "your-password"
#define SERVER_URL "http://192.168.1.50:8787/api/esp-status"  // DEV machine LAN IP!
#define POLL_MS 15000
#define DISPLAY_ROTATION 2              // 0 = USB-top, 2 = flipped 180 deg
```

Rules:
- `SERVER_URL` is the DEV machine's LAN IP (`hostname -I` on DEV), port `8787`.
- Confirm reachability from HOST before flashing:
  `curl http://<DEV-LAN-IP>:8787/api/esp-status` (expect ~310 bytes JSON).
- Optional: `git update-index --skip-worktree firmware/src/config.h` to avoid
  committing the password.

## 3. Find the serial port + fix permissions

```bash
ls /dev/ttyUSB* /dev/ttyACM* /dev/cu.* 2>/dev/null
# Linux typical: /dev/ttyUSB0 (CH340 shows as ch341-uart in dmesg)
# macOS typical: /dev/cu.wchusbserial*
# Windows: Device Manager -> Ports, e.g. COM5
```

- Linux: kernel driver `ch341` is usually built-in.
  `dmesg | tail` should show `ch341-uart converter now attached to ttyUSB0`.
- Permission denied: user must be in `dialout` (or `uucp` on Arch/Omarchy):
  `sudo usermod -aG dialout $USER`, then re-login. Check with `groups` and `ls -l /dev/ttyUSB0`.
- macOS/Windows with no port: install the WCH CH340 driver first.
- No port at all: swap USB cable and USB port before anything else.

## 4. Compile first (no device needed)

From the repo root:

```bash
./firmware/build-arduino-cli.sh
```

What the script does (`firmware/build-arduino-cli.sh`):
- Copies `src/main.cpp src/ui.cpp src/ui.h src/config.h` into
  `/tmp/esp-status-sketch/esp-status/` plus a stub `esp-status.ino`.
- Runs `arduino-cli compile --fqbn esp32:esp32:esp32 <sketch>`.

Expected: compile succeeds (~1.09 MB flash, ~49 KB RAM).
If it fails, paste the full `Sketch uses ...` error — do not attempt upload.

PlatformIO equivalent:
```bash
cd firmware && pio run
```

## 5. Upload (the actual flash)

> Known script quirk: the script header comment mentions `--upload-port`,
> but the code only accepts `--upload <port>`. Always use the form below.

```bash
./firmware/build-arduino-cli.sh --upload /dev/ttyUSB0
# generic form: ./firmware/build-arduino-cli.sh --upload <PORT>
# Windows example: ./firmware/build-arduino-cli.sh --upload COM5
# override via env: UPLOAD_PORT=/dev/ttyUSB0 ./firmware/build-arduino-cli.sh --upload
```

Under the hood: `arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32 <sketch>`.

PlatformIO equivalent:
```bash
cd firmware
pio run -t upload --upload-port /dev/ttyUSB0
```

If upload fails:
1. Confirm the port still exists (`ls /dev/ttyUSB*`).
2. Check permissions (step 3).
3. Retry; if the board does not auto-enter download mode, hold BOOT while
   starting the upload, release after `Connecting...` succeeds.
4. Try another cable/port; confirm no other monitor holds the port open.

## 6. Verify via serial monitor (baud 115200)

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
# Ctrl+C to exit. PlatformIO: pio device monitor -b 115200
```

Success signs (each poll, every 15 s):
- LCD: `esp-status v9 / connecting <SSID>`, then top bar with project name +
  `ON`/`OFF` loop badge, small model line, up to 10 green/red LLM bars,
  `last llm call 5m ago` freshness, last action (e.g. `commit 3m ago`),
  persona glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`), red `STUCK` when stuck.
- Serial: `esp-status boot`, `IP: 192.168.x.x`, then per poll:
  `ok timeline ON engineer MiniMax-M2.7 llm47s act:pushed feat/foo 300s bars:4`.

## 7. Troubleshooting matrix

| Symptom | Fix |
|---|---|
| No serial port appears | Swap USB cable (must be data); install CH340 driver; try another port |
| `Permission denied` on port | Add user to `dialout`/`uucp`, re-login; check `ls -l /dev/ttyUSB0` |
| `wifi fail` / `wifi lost` on LCD | SSID must be 2.4 GHz; check password in `config.h`; move closer to router |
| `poll failed` / grey screen | DEV unreachable: `curl` the URL from HOST; same LAN? firewall (`sudo ufw allow 8787/tcp`)? `SERVER_URL` typo? |
| LCD stays black | Backlight is GPIO32 (firmware handles it); check USB power; re-flash |
| Wrong colors / garbled image | Wrong board revision — see `docs/flashing.md` pin table; confirm Adafruit_ST7789 lib |
| Compile `Sketch uses ...` error | Paste full error in an issue; check esp32 core 3.3.x + required libs |
| Upload `Connecting...` timeout | Hold BOOT during upload start; close other serial monitors; swap cable |

## 8. File map (for agents)

```
firmware/src/main.cpp        WiFi + HTTP poll + loop (pins, last-10 bar parsing)
firmware/src/ui.h, ui.cpp    170x320 portrait renderer (v9 layout)
firmware/src/config.h        WiFi + SERVER_URL — EDIT ME, do not commit secrets
firmware/platformio.ini      PlatformIO env ideaspark-esp32 (huge_app.csv)
firmware/build-arduino-cli.sh arduino-cli build/upload script (compile default, --upload <port> to flash)
docs/flashing.md             Board pins, CH340 drivers, memory notes
server/ENDPOINT.md           /api/esp-status spec
```

## 9. Safe agent workflow

1. Read `firmware/src/config.h` and confirm SSID/URL look intentional; ask user
   before changing them.
2. Run `ls /dev/ttyUSB*`, `arduino-cli core list`, `arduino-cli lib list`.
3. Compile first; only upload on compile success.
4. Upload with explicit port (`--upload /dev/ttyUSB0`); report the exact command run.
5. Open the serial monitor briefly to confirm `esp-status boot` + `ok ...` lines.
6. Never commit `config.h` secrets; suggest `git update-index --skip-worktree`.
