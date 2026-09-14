#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>

// v5 layout contract (see server/ENDPOINT.md):
//   Project (Loop) / Provider (large) + model (small) / speedometer gauge
//   (succ/total, animated needle, no text) / Persona glyph + freshness
//   (large, no footer)
//   Loop status lives in the header badge + bar color; no status dot.
struct EspStatus {
  bool ok = false;
  String proj = "";
  bool loop = false;
  String status = "red";   // green|red (server-side); grey only while offline
  String provider = "-";   // effective LLM provider (e.g. joingonka)
  String model = "-";      // effective LLM model, basename (e.g. MiniMax-M2.7)
  long succ = 0;           // successful calls in strict last-10 window (gauge numerator)
  long total = 0;          // observed calls in window, max 10 (gauge denominator)
  String persona = "-";    // pm | engineer | qa | review-engineer | ...
  long ago_s = -1;
  // Legacy fields (pre-v3 payloads): still parsed, no longer displayed.
  String last = "";
  int runs = 0;
  int ok_n = 0;
  int fail_n = 0;
  long tok_today = 0;
  int err = 0;
  bool offline = true;
};

uint16_t statusColor(const String& s);
String fmtAgo(long ago_s);
// "pm" -> "PM", "review-engineer" -> "REVIEW", ... (uppercase, truncated).
String personaDisplay(const String& persona);
// Accent color per role: PM gold, engineer cyan, QA green, review magenta.
uint16_t personaColor(const String& persona);
void uiBoot(Adafruit_ST7789& tft, const String& ssid);
// Small boot-line update (no full wipe) — used while connecting to WiFi.
void uiBootStatus(Adafruit_ST7789& tft, const String& msg);
// Differential redraw: full frame once, then only dirty rects (no flicker).
// The gauge needle animates towards new values in uiTick (ease-out sweep).
void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg);
// Needle animation frame (call every ~40ms). Kept as a no-op when idle.
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs);
