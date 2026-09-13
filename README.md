# esp-status — auto-pi pocket monitor for ESP32

IdeaSpark ESP32 (16 MB + 1.9" ST7789 170×320 LCD) shows the live status of your
autonomous `auto-pi` loop: green / yellow / red, active persona, last activity
and how long ago, run counters and token usage. It polls a tiny JSON endpoint
over your home WiFi every 15 seconds.

```
auto-pi loop (DEV machine) → .pi/logs/*.jsonl → ui/server :8787/api/esp-status
        →─── same LAN WiFi ───→ ESP32 (HOST machine) → LCD
```

## Two machines

| Machine | Role |
|---|---|
| **DEV** (this repo's origin) | Runs `auto-pi` loop + monitor backend on port `8787` |
| **HOST** (your computer) | ESP32 is plugged in here via USB; you build + flash from here |

Both must be on the **same LAN** (ESP32 needs **2.4 GHz** WiFi — it can't use 5 GHz).

## 1. DEV machine — start the status endpoint

On the machine running `auto-pi`:

```bash
cd /path/to/auto-pi
npm run ui:server   # backend on http://<DEV-LAN-IP>:8787
```

Find the DEV machine's LAN IP:

```bash
hostname -I   # e.g. 192.168.1.50 — use the 192.168.x.x / 10.x.x.x one
```

Verify the ESP endpoint (expect ~240 bytes of JSON):

```bash
curl http://localhost:8787/api/esp-status
# {"ok":true,"status":"green","loop":true,"persona":"engineer","last":"...","ago_s":47,...}
```

> Firewall: port `8787` must be reachable from the LAN. If `curl http://<DEV-LAN-IP>:8787/api/esp-status`
> from the HOST machine fails, open the port (e.g. `sudo ufw allow 8787/tcp`).

Status colors (decided server-side, ESP just draws them):

| Color | Meaning |
|---|---|
| 🟢 green | Loop running, activity < 5 min ago |
| 🟡 yellow | Loop running, activity 5–15 min ago |
| 🔴 red | Loop stopped, recent error, or stale > 15 min |
| ⚫ grey | ESP offline (WiFi/HTTP failed) — ESP-side only |

## 2. HOST machine — get this repo

```bash
git clone https://github.com/AntonLapshin/esp-status.git
cd esp-status
```

## 3. HOST machine — install the toolchain (pick one)

**Option A — arduino-cli (recommended, no Python needed):**

```bash
# Linux/macOS — download from https://github.com/arduino/arduino-cli/releases
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "TFT_eSPI" "ArduinoJson"
```

**Option B — PlatformIO:**

```bash
pip install platformio   # or: pipx install platformio
cd firmware && pio run
```

**Option C — Arduino IDE:** install IDE 2.x, add ESP32 core via Boards Manager
(`https://espressif.github.io/arduino-esp32/package_esp32_index.json`),
install libraries `TFT_eSPI` + `ArduinoJson` via Library Manager.

## 4. HOST machine — configure WiFi + server address

Edit **`firmware/src/config.h`**:

```cpp
#define WIFI_SSID "YourHomeWifi"        // 2.4 GHz SSID
#define WIFI_PASS "your-password"
#define SERVER_URL "http://192.168.1.50:8787/api/esp-status"  // DEV LAN IP!
```

> ⚠️ `config.h` holds your WiFi password — don't commit your edits.
> (Tip: `git update-index --skip-worktree firmware/src/config.h`)

## 5. HOST machine — plug in the ESP32

1. Use a **USB data cable** (many USB-C cables are charge-only — if nothing
   appears below, swap the cable first).
2. Drivers: the IdeaSpark uses a **CH340** serial chip.
   - Linux: usually built-in (`dmesg | tail` should show `ch341-uart` + `ttyUSB0`).
     If permission denied: `sudo usermod -aG dialout $USER`, then re-login.
   - macOS: install the CH340 driver from the vendor (WCH) if no `/dev/cu.*` appears.
   - Windows: install CH340 driver, then check Device Manager → Ports (COMx).
3. Find the port:

```bash
# Linux/macOS
ls /dev/ttyUSB* /dev/ttyACM* /dev/cu.* 2>/dev/null
# Windows: Device Manager → Ports, e.g. COM5
```

## 6. HOST machine — build + flash

```bash
# compile-check (no device needed)
./firmware/build-arduino-cli.sh

# flash (Linux/macOS example; Windows: -p COM5)
./firmware/build-arduino-cli.sh --upload /dev/ttyUSB0

# watch boot log (baud 115200, Ctrl+C to exit)
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

PlatformIO equivalent:

```bash
cd firmware
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor -b 115200
```

## 7. What you should see

1. LCD shows `auto-pi / connecting <SSID>`, then your project name in the top bar.
2. Big colored dot + `GREEN`/`YELLOW`/`RED`, persona (`engineer`, `pm`, …) and `47s ago`.
3. `last:` line = last loop activity (e.g. `pr.created #42`, `dispatch review…`).
4. Stats grid: `RUN / OK / FAIL / TOK / LOOP ON|OFF / ERR`.
5. Serial log prints `ok green engineer 47s` each poll.

## Troubleshooting

| Symptom | Fix |
|---|---|
| No serial port appears | Swap USB cable (must be data, not charge-only); install CH340 driver; try another port |
| `wifi fail` / `wifi lost` on LCD | SSID must be 2.4 GHz; check password; move closer to router |
| `poll failed` on LCD, grey screen | DEV unreachable: `curl http://<DEV-IP>:8787/api/esp-status` from HOST; same LAN? firewall open? `SERVER_URL` typo? |
| LCD stays black | Backlight pin is GPIO32 (handled in firmware); check USB power; re-flash |
| Wrong colors / garbled image | You have a different board revision — check `docs/flashing.md` pin table |
| `Sketch uses …` build error | Paste the full error in an issue |

## Layout

```
esp-status/
  README.md                  ← you are here
  firmware/
    src/main.cpp             ← WiFi + HTTP poll + loop
    src/ui.{h,cpp}           ← 170×320 portrait renderer
    src/config.h             ← WiFi + SERVER_URL (edit me)
    platformio.ini           ← PlatformIO build (ST7789 flags)
    build-arduino-cli.sh     ← arduino-cli build/upload script
  server/ENDPOINT.md         ← /api/esp-status spec (live code: auto-pi/ui/server/server.js)
  docs/flashing.md           ← board pins, drivers, notes
```

Display wiring (IdeaSpark 1.9" ST7789, fixed on-board):
`MOSI=23, SCLK=18, CS=15, DC=2, RST=4, BL=32` — no wiring needed.
