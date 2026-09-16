#include "ui.h"
#include "config.h"

#define COL_BLACK 0x0000
#define COL_WHITE 0xFFFF
#define COL_GREEN 0x07E0
#define COL_YELLOW 0xFFE0
#define COL_RED 0xF800
#define COL_CYAN 0x07FF
#define COL_MAGENTA 0xF81F
#define COL_GREY 0x8410
#define COL_LGREY 0xC618
#define COL_DGREY 0x7BEF

// Layout v9 (170x320 portrait, works for rotation 0 and 2)
//   header  -> project + ON/OFF loop badge (loop status lives here, no dot)
//   model (small) / "LLM" caption + up to 10 outcome bars (green/red,
//   oldest left, newest right) / last-llm-call freshness / last action +
//   freshness / persona glyph / large red STUCK banner when stuck
#define TOP_H 30
#define MODEL_Y 38
#define LLM_CAP_Y 54
#define BAR_TOP 64
#define BAR_H 40
#define BAR_W 13
#define BAR_GAP 3
#define BAR_N 10
#define BAR_X0 ((SCREEN_W - (BAR_N * BAR_W + (BAR_N - 1) * BAR_GAP)) / 2)
#define LLM_AGO_Y 112
#define ACT_Y 130
#define PERS_Y 152
#define STUCK_ZONE_TOP 226
#define STUCK_Y 232

// Header bar: grey while offline, red when stuck or loop off, green when on.
static uint16_t headerColor(const EspStatus& st) {
  if (st.offline) return COL_GREY;
  if (st.stuck || !st.loop) return COL_RED;
  return COL_GREEN;
}

String fmtAgo(long ago_s) {
  if (ago_s < 0) return "never";
  if (ago_s < 60) return String(ago_s) + "s ago";
  if (ago_s < 3600) return String(ago_s / 60) + "m ago";
  return String(ago_s / 3600) + "h ago";
}

String personaDisplay(const String& persona) {
  String p = persona;
  p.toUpperCase();
  if (p == "REVIEW-ENGINEER") return "REVIEW";
  if (p.length() > 10) p = p.substring(0, 10);
  return p;
}

uint16_t personaColor(const String& persona) {
  String p = persona;
  p.toLowerCase();
  if (p == "pm") return COL_YELLOW;
  if (p == "engineer") return COL_CYAN;
  if (p == "qa") return COL_GREEN;
  if (p.startsWith("review")) return COL_MAGENTA;
  return COL_WHITE;
}

// Centered text helper (Adafruit GFX has no drawString/datum).
static void centerText(Adafruit_ST7789& tft, const String& s, int y, uint8_t size) {
  tft.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - (int)w) / 2, y);
  tft.print(s);
}

static String titleText(const EspStatus& st) {
  String title = st.proj.length() ? st.proj : "auto-pi";
  if (title.length() > 10) title = title.substring(0, 10);
  return title;
}

static String modelText(const EspStatus& st) {
  String m = st.model.length() ? st.model : "-";
  // Show the model basename ("org/MiniMax-M2.7" -> "MiniMax-M2.7").
  // Size-1 font fits 28 chars.
  int slash = m.lastIndexOf('/');
  if (slash >= 0) m = m.substring(slash + 1);
  if (m.length() > 28) m = m.substring(0, 28);
  return m;
}

static String llmAgoText(const EspStatus& st) {
  return "last llm call " + fmtAgo(st.lastLlmCallFinished);
}

static String actionText(const EspStatus& st) {
  if (st.lastAction.length() == 0 || st.lastAction == "-") return "no action yet";
  String ago = fmtAgo(st.lastActionAgoS);
  // Size-1 font fits 28 chars; truncate the label, keep the freshness suffix.
  int keep = 28 - (int)ago.length() - 1;
  if (keep < 8) keep = 8;
  String a = st.lastAction;
  if ((int)a.length() > keep) a = a.substring(0, keep);
  return a + " " + ago;
}

static uint8_t personaSize(const String& disp) {
  if (disp.length() <= 2) return 4; // PM, QA — hero glyphs
  if (disp.length() <= 7) return 3; // ENGINEER, REVIEW
  return 2;
}

// --- boot -------------------------------------------------------------------
void uiBoot(Adafruit_ST7789& tft, const String& ssid) {
  tft.fillScreen(COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextWrap(false);
  centerText(tft, "esp-status", 122, 3);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  String sub = "v9 connecting " + ssid;
  if (sub.length() > 28) sub = sub.substring(0, 28);
  centerText(tft, sub, 162, 1);
}

void uiBootStatus(Adafruit_ST7789& tft, const String& msg) {
  String m = msg;
  if (m.length() > 28) m = m.substring(0, 28);
  tft.fillRect(0, 178, SCREEN_W, 12, COL_BLACK);
  tft.setTextColor(COL_YELLOW, COL_BLACK);
  centerText(tft, m, 178, 1);
}

// --- differential data screen -----------------------------------------------
struct Snap {
  String title;
  String loopBadge;
  uint16_t headerC = 0;
  String model;
  uint16_t barsMask = 0; // bit i = llmStatus[i] (oldest first), valid < count
  uint8_t llmCount = 0;
  String llmAgo;
  String action;
  String persona;
  uint16_t personaC = 0;
  uint8_t personaSize = 0;
  bool stuck = false;
  bool offline = true;
  bool valid = false;
};
static Snap prev;

static void drawTopBar(Adafruit_ST7789& tft, const EspStatus& st, uint16_t c) {
  tft.fillRect(0, 0, SCREEN_W, TOP_H, c);
  // Project name, left-aligned.
  tft.setTextSize(2);
  tft.setTextColor(COL_BLACK, c);
  tft.setCursor(6, 8);
  tft.print(titleText(st));
  // LOOP badge: black pill, top-right.
  const int bw = 40, bh = 18, bx = SCREEN_W - bw - 6, by = 6;
  tft.fillRect(bx, by, bw, bh, COL_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(st.loop ? COL_GREEN : COL_RED, COL_BLACK);
  String b = st.loop ? "ON" : "OFF";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(b, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(bx + (bw - (int)w) / 2, by + 5);
  tft.print(b);
}

static void drawCaption(Adafruit_ST7789& tft, const String& cap, int y) {
  tft.setTextSize(1);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  centerText(tft, cap, y, 1);
}

static void drawModel(Adafruit_ST7789& tft, const EspStatus& st) {
  // Small model line under the header (no caption — single line, v9 style).
  tft.fillRect(0, TOP_H, SCREEN_W, LLM_CAP_Y - TOP_H, COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  centerText(tft, modelText(st), MODEL_Y, 1);
}

// --- LLM outcome bars -------------------------------------------------------
// Up to 10 bars, oldest left / newest right: green = success, red = failure.
// Empty slots (fewer than 10 calls recorded) are dim outlines. The newest
// bar gets a white top edge so recency is visible at a glance.
static void drawBars(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, LLM_CAP_Y - 2, SCREEN_W, BAR_TOP + BAR_H - LLM_CAP_Y + 2, COL_BLACK);
  drawCaption(tft, "LLM", LLM_CAP_Y);
  uint8_t n = st.llmCount > BAR_N ? BAR_N : st.llmCount;
  for (uint8_t i = 0; i < BAR_N; i++) {
    int x = BAR_X0 + i * (BAR_W + BAR_GAP);
    if (i < n) {
      uint16_t c = st.llmStatus[i] ? COL_GREEN : COL_RED;
      tft.fillRect(x, BAR_TOP, BAR_W, BAR_H, c);
      if (i == n - 1) tft.fillRect(x, BAR_TOP, BAR_W, 2, COL_WHITE);
    } else {
      tft.drawRect(x, BAR_TOP, BAR_W, BAR_H, COL_DGREY);
    }
  }
}

static void drawLlmAgo(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, LLM_AGO_Y - 4, SCREEN_W, ACT_Y - LLM_AGO_Y, COL_BLACK);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  centerText(tft, llmAgoText(st), LLM_AGO_Y, 1);
}

static void drawAction(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, ACT_Y - 4, SCREEN_W, PERS_Y - 10 - ACT_Y + 4, COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  centerText(tft, actionText(st), ACT_Y, 1);
}

static void drawPersona(Adafruit_ST7789& tft, const EspStatus& st) {
  // Persona glyph; clears down to the STUCK zone (freshness now lives in
  // the llm/action lines above).
  tft.fillRect(0, PERS_Y - 10, SCREEN_W, STUCK_ZONE_TOP - (PERS_Y - 10), COL_BLACK);
  String disp = personaDisplay(st.persona);
  uint16_t pc = personaColor(st.persona);
  tft.setTextColor(pc, COL_BLACK);
  centerText(tft, disp, PERS_Y, personaSize(disp));
}

static void drawStuck(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, STUCK_ZONE_TOP, SCREEN_W, SCREEN_H - STUCK_ZONE_TOP, COL_BLACK);
  if (!st.stuck) return;
  tft.setTextColor(COL_RED, COL_BLACK);
  centerText(tft, "STUCK", STUCK_Y, 4);
}

static void drawFull(Adafruit_ST7789& tft, const EspStatus& st, uint16_t c) {
  tft.fillScreen(COL_BLACK);
  tft.setTextWrap(false);
  drawTopBar(tft, st, c);
  drawModel(tft, st);
  drawBars(tft, st);
  drawLlmAgo(tft, st);
  drawAction(tft, st);
  drawPersona(tft, st);
  drawStuck(tft, st);
}

static Snap snapOf(const EspStatus& st) {
  Snap cur;
  cur.title = titleText(st);
  cur.loopBadge = st.loop ? "ON" : "OFF";
  cur.headerC = headerColor(st);
  cur.model = modelText(st);
  cur.barsMask = 0;
  uint8_t n = st.llmCount > BAR_N ? BAR_N : st.llmCount;
  for (uint8_t i = 0; i < n; i++) {
    if (st.llmStatus[i]) cur.barsMask |= (uint16_t)(1u << i);
  }
  cur.llmCount = n;
  cur.llmAgo = llmAgoText(st);
  cur.action = actionText(st);
  cur.persona = personaDisplay(st.persona);
  cur.personaC = personaColor(st.persona);
  cur.personaSize = personaSize(cur.persona);
  cur.stuck = st.stuck;
  cur.offline = st.offline;
  cur.valid = true;
  return cur;
}

void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  (void)errMsg; // no footer to show it on; offline state still greys the UI
  uint16_t c = headerColor(st);
  tft.setTextWrap(false);
  Snap cur = snapOf(st);

  if (!prev.valid) {
    drawFull(tft, st, c);
    prev = cur;
    return;
  }

  // Top bar carries the loop status (ON/OFF + bar color) — no dot.
  if (cur.headerC != prev.headerC || cur.title != prev.title || cur.loopBadge != prev.loopBadge) {
    drawTopBar(tft, st, c);
  }
  if (cur.model != prev.model) drawModel(tft, st);
  if (cur.barsMask != prev.barsMask || cur.llmCount != prev.llmCount) drawBars(tft, st);
  if (cur.llmAgo != prev.llmAgo) drawLlmAgo(tft, st);
  if (cur.action != prev.action) drawAction(tft, st);
  if (cur.persona != prev.persona || cur.personaC != prev.personaC ||
      cur.personaSize != prev.personaSize)
    drawPersona(tft, st);
  if (cur.stuck != prev.stuck) drawStuck(tft, st);

  prev = cur;
}

// --- animation frame: v9 has no animated elements ---------------------------
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs) {
  (void)tft;
  (void)st;
  (void)nowMs;
  (void)lastPollMs;
}
