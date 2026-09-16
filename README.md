# esp-status — auto-pi pocket monitor for ESP32

IdeaSpark ESP32 (16 MB + 1.9" ST7789 170×320 LCD) shows the live status of your
autonomous `auto-pi` loop: project + loop badge, the model, two 10-bar rows —
`PERSONA` (whole persona-run outcomes) and `LLM` (individual LLM-turn
outcomes) — each green/red with solid grey bars on the left when fewer than 10
recorded, per-row freshness, the last GitHub-visible action, the active
persona hero glyph and a large red STUCK banner when the loop is stuck. It
polls a tiny JSON endpoint over your home WiFi every 15 seconds.

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

Verify the ESP endpoint (expect ~400 bytes of JSON):

```bash
curl http://localhost:8787/api/esp-status
# {"ok":true,"proj":"timeline","loop":true,"stuck":false,"llmActive":true,"persona":"engineer","model":"deepseek-ai/DeepSeek-V4-Flash-0731","lastAction":"pushed feat/foo","lastActionAgoS":300,"last10PersonaStatus":[true,true,false,true],"lastPersonaCallFinished":300,"last10LlmStatus":[true,false,true,true],"lastLlmCallFinished":47}
```

> Firewall: port `8787` must be reachable from the LAN. If `curl http://<DEV-LAN-IP>:8787/api/esp-status`
> from the HOST machine fails, open the port (e.g. `sudo ufw allow 8787/tcp`).

Header colors:

| Color | Meaning |
|---|---|
| 🟢 green | Loop on and not stuck |
| 🔴 red | Loop off, or stuck |
| ⚫ grey | ESP offline (WiFi/HTTP failed) — ESP-side only |

Screen layout (v13, top → bottom): header bar with project name + `ON`/`OFF`
loop badge · small model line (e.g. `DeepSeek-V4-Flash-0731`) · `PERSONA`
caption + up to 10 persona-run bars (green = success, red = failure,
right-aligned with solid grey bars on the left when fewer than 10, newest
right with a white top edge) · `{ago}` freshness (e.g. `5m ago`) right after
the Persona bars · `LLM` caption + up to 10 per-turn LLM bars (same style) ·
`{ago}` freshness (e.g. `30s ago`) · last GitHub-visible action (e.g. `commit 3m ago`) ·
`PERSONA` hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`, …) · large red `STUCK`
banner when stuck. Every section is separated by an explicit breathing gap.

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

1. LCD shows `esp-status v13 / connecting <SSID>`, then your project name in the top bar with the `ON`/`OFF` loop badge.
2. Small model line (e.g. `MiniMax-M2.7`).
3. `PERSONA` row: up to 10 green/red bars for whole persona runs (right-aligned, grey bars on the left when fewer than 10) + `{ago}` (e.g. `5m ago`) right after the bars.
4. `LLM` row: up to 10 green/red bars for individual LLM turns (same style) + `{ago}` (e.g. `30s ago`).
5. Last action (e.g. `commit 3m ago`) + `PERSONA` hero glyph (`PM`, `ENGINEER`, `QA`, `REVIEW`); large red `STUCK` when stuck.
6. Serial log prints `ok timeline ON engineer MiniMax-M2.7 pers300s llm47s act:pushed feat/foo 300s pbars:4 lbars:5` each poll.

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
    src/ui.{h,cpp}           ← 170×320 portrait renderer (v13 layout)
    src/config.h             ← WiFi + SERVER_URL (edit me)
    platformio.ini           ← PlatformIO build (ST7789 flags)
    build-arduino-cli.sh     ← arduino-cli build/upload script
  server/ENDPOINT.md         ← /api/esp-status spec (live code: auto-pi/ui/server/server.js)
  docs/flashing.md           ← board pins, drivers, notes
```

Display wiring (IdeaSpark 1.9" ST7789, fixed on-board):
`MOSI=23, SCLK=18, CS=15, DC=2, RST=4, BL=32` — no wiring needed.
