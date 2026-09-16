#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>

// v9 layout contract (see server/ENDPOINT.md):
//   Project + ON/OFF loop badge (header) / model (small) / up to 10 LLM
//   outcome bars (green=true, red=false, oldest left, newest right) /
//   last-LLM-call freshness ("last llm call 5m ago") / last GitHub-visible
//   action + freshness ("commit 3m ago") / persona glyph / large red STUCK
//   banner when stuck.
struct EspStatus {
  bool ok = false;
  String proj = "";
  bool loop = false;
  bool stuck = false;
  String persona = "-";    // pm | engineer | qa | review-engineer | ...
  String model = "-";      // effective LLM model, basename (e.g. MiniMax-M2.7)
  String lastAction = "-"; // e.g. "pushed feat/foo" ("-" when none yet)
  long lastActionAgoS = -1;
  bool llmStatus[10];      // oldest first (reversed at parse); true=ok
  uint8_t llmCount = 0;    // valid entries in llmStatus (0..10)
  long lastLlmCallFinished = -1;
  bool offline = true;
};

String fmtAgo(long ago_s);
// "pm" -> "PM", "review-engineer" -> "REVIEW", ... (uppercase, truncated).
String personaDisplay(const String& persona);
// Accent color per role: PM gold, engineer cyan, QA green, review magenta.
uint16_t personaColor(const String& persona);
void uiBoot(Adafruit_ST7789& tft, const String& ssid);
// Small boot-line update (no full wipe) — used while connecting to WiFi.
void uiBootStatus(Adafruit_ST7789& tft, const String& msg);
// Differential redraw: full frame once, then only dirty rects (no flicker).
void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg);
// Animation frame (call every ~40ms). v9 has no animated elements — no-op.
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs);
