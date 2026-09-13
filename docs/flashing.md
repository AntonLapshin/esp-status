# Board notes — IdeaSpark ESP32 + 1.9" ST7789

## Display (fixed on-board, no wiring)

| Signal | GPIO |
|---|---|
| MOSI (SDA) | 23 |
| SCLK (SCL) | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| BL (backlight) | 32 |

170×320 px, portrait (`tft.setRotation(0)`), ST7789 driver with inversion on.
Display library: `Adafruit_ST7789` + `Adafruit_GFX` (pins in `main.cpp`:
`MOSI=23, SCLK=18, CS=15, DC=2, RST=4, BL=32`; `tft.init(170,320)` +
`invertDisplay(true)`). No `TFT_eSPI` setup flags needed.

## USB / drivers

- Chip: **CH340** USB-serial, USB-C connector. Cable **must** be data-capable.
- Linux: kernel driver `ch341` (built-in on most distros). `dmesg | tail` →
  `ch341-uart converter now attached to ttyUSB0`. Dialout group if needed:
  `sudo usermod -aG dialout $USER` + re-login.
- macOS: install WCH CH340 driver if no `/dev/cu.wchusbserial*` appears.
- Windows: install CH340 driver, note the COM port in Device Manager.
- 115200 baud for monitor/flash. If upload fails, hold BOOT while starting
  the upload (most IdeaSpark boards auto-enter download mode, so rarely needed).

## Memory

Firmware uses ~1.09 MB flash (default 1.25 MB app partition is fine;
`platformio.ini` selects `huge_app.csv` for extra headroom) and ~49 KB RAM.
`ArduinoJson` doc capped at ~2 KB — the endpoint stays under 500 bytes by design.
