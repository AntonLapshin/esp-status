#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>

struct EspStatus {
  bool ok = false;
  String status = "grey";   // green|yellow|red|grey
  bool loop = false;
  String persona = "-";
  String last = "waiting...";
  long ago_s = -1;
  int runs = 0;
  int ok_n = 0;
  int fail_n = 0;
  long tok_today = 0;
  int err = 0;
  String proj = "";
  bool offline = true;
};

uint16_t statusColor(const String& s);
String fmtAgo(long ago_s);
String fmtTokens(long n);
void uiBoot(Adafruit_ST7789& tft, const String& ssid);
// Small boot-line update (no full wipe) — used while connecting to WiFi.
void uiBootStatus(Adafruit_ST7789& tft, const String& msg);
// Differential redraw: full frame once, then only dirty rects (no flicker).
void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg);
// Animation frame (~25fps, no fillScreen): pulse rings + poll progress bar.
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs);
