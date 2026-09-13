#pragma once

// --- WiFi ---
#define WIFI_SSID "Cupcake"
#define WIFI_PASS "24547294"

// --- auto-pi backend (same LAN) ---
// Find server IP with `hostname -I` on the machine running `npm run ui:server`
#define SERVER_URL "http://192.168.7.128:8787/api/esp-status"

// --- Polling ---
#define POLL_MS 15000
#define WIFI_TIMEOUT_MS 15000

// --- Display (IdeaSpark 1.9" ST7789 170x320, portrait) ---
#define LCD_BL_PIN 32
#define SCREEN_W 170
#define SCREEN_H 320
