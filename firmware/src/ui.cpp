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

// Layout v13 (170x320 portrait, works for rotation 0 and 2)
//   header  -> project + ON/OFF loop badge (loop status lives here, no dot)
//   model (size 2) / "PERSONA" caption (size 2) + up to 10 persona-run bars
//   (green/red, right-aligned, solid grey bars on the left when < 10,
//   newest rightmost with white top edge) / "{ago}" freshness
//   (size 2, right after the Persona bars, no "Persona" prefix) / "LLM"
//   caption (size 2) + up to 10 per-turn LLM bars (same style) / "{ago}"
//   freshness (size 2, no "LLM" prefix) / last action + freshness (size 2) /
//   persona glyph / large red STUCK banner when stuck.
//   Every section is separated by an explicit breathing gap.
#define TOP_H 28
#define MODEL_Y 34
#define PERSONA_CAP_Y 58
#define PERSONA_BAR_TOP 80
#define PERSONA_AGO_Y 102
#define LLM_CAP_Y 130
#define LLM_BAR_TOP 152
#define BAR_H 14
#define BAR_W 13
#define BAR_GAP 3
#define BAR_N 10
#define BAR_X0 ((SCREEN_W - (BAR_N * BAR_W + (BAR_N - 1) * BAR_GAP)) / 2)
#define LLM_AGO_Y 174
#define ACT_Y 198
#define PERS_Y 224
#define STUCK_ZONE_TOP 264
#define STUCK_Y 272

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
  // Size-2 font fits 14 chars.
  int slash = m.lastIndexOf('/');
  if (slash >= 0) m = m.substring(slash + 1);
  if (m.length() > 14) m = m.substring(0, 14);
  return m;
}

static String personaAgoText(const EspStatus& st) {
  return fmtAgo(st.lastPersonaCallFinished);
}

static String llmAgoText(const EspStatus& st) {
  return fmtAgo(st.lastLlmCallFinished);
}

static String actionText(const EspStatus& st) {
  if (st.lastAction.length() == 0 || st.lastAction == "-") return "no action yet";
  String ago = fmtAgo(st.lastActionAgoS);
  // Size-2 font fits 14 chars; truncate the label, keep the freshness suffix.
  int keep = 14 - (int)ago.length() - 1;
  if (keep < 4) keep = 4;
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
  String sub = "v13 connecting " + ssid;
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
  uint16_t personaMask = 0; // bit i = personaStatus[i] (oldest first), valid < count
  uint8_t personaCount = 0;
  String personaAgo;
  uint16_t llmMask = 0; // bit i = llmStatus[i] (oldest first), valid < count
  uint8_t llmCount = 0;
  String llmAgo;
  String action;
  String persona;
  uint16_t personaC = 0;
  uint8_t personaSz = 0;
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
  tft.setCursor(6, 7);
  tft.print(titleText(st));
  // LOOP badge: black pill, top-right.
  const int bw = 40, bh = 18, bx = SCREEN_W - bw - 6, by = 5;
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
  tft.setTextSize(2);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  centerText(tft, cap, y, 2);
}

static void drawModel(Adafruit_ST7789& tft, const EspStatus& st) {
  // Model line under the header (size 2, v13 style).
  tft.fillRect(0, TOP_H, SCREEN_W, PERSONA_CAP_Y - TOP_H, COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  centerText(tft, modelText(st), MODEL_Y, 2);
}

// --- outcome bars -----------------------------------------------------------
// Up to 10 bars, right-aligned: data ends at the newest (right), empty slots
// (< 10 recorded) are solid grey bars on the left. Recorded bars are
// green = success, red = failure. The newest bar gets a white top edge so
// recency is visible at a glance. Both PERSONA and LLM rows share this style.
static void drawBarRow(Adafruit_ST7789& tft, const bool* status, uint8_t count, int barTop) {
  uint8_t n = count > BAR_N ? BAR_N : count;
  uint8_t empty = BAR_N - n;
  for (uint8_t i = 0; i < BAR_N; i++) {
    int x = BAR_X0 + i * (BAR_W + BAR_GAP);
    if (i < empty) {
      tft.fillRect(x, barTop, BAR_W, BAR_H, COL_GREY);
    } else {
      uint8_t di = i - empty; // 0..n-1, oldest left, newest right
      uint16_t c = status[di] ? COL_GREEN : COL_RED;
      tft.fillRect(x, barTop, BAR_W, BAR_H, c);
      if (di == n - 1) tft.fillRect(x, barTop, BAR_W, 2, COL_WHITE);
    }
  }
}

static void drawPersonaBars(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, PERSONA_CAP_Y - 2, SCREEN_W, PERSONA_BAR_TOP + BAR_H - PERSONA_CAP_Y + 2, COL_BLACK);
  drawCaption(tft, "PERSONA", PERSONA_CAP_Y);
  drawBarRow(tft, st.personaStatus, st.personaCount, PERSONA_BAR_TOP);
}

static void drawLlmBars(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, LLM_CAP_Y - 2, SCREEN_W, LLM_BAR_TOP + BAR_H - LLM_CAP_Y + 2, COL_BLACK);
  drawCaption(tft, "LLM", LLM_CAP_Y);
  drawBarRow(tft, st.llmStatus, st.llmCount, LLM_BAR_TOP);
}

static void drawPersonaAgo(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, PERSONA_AGO_Y - 2, SCREEN_W, 20, COL_BLACK);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  centerText(tft, personaAgoText(st), PERSONA_AGO_Y, 2);
}

static void drawLlmAgo(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, LLM_AGO_Y - 2, SCREEN_W, 20, COL_BLACK);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  centerText(tft, llmAgoText(st), LLM_AGO_Y, 2);
}

static void drawAction(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, ACT_Y - 4, SCREEN_W, PERS_Y - 10 - ACT_Y + 4, COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  centerText(tft, actionText(st), ACT_Y, 2);
}

static void drawPersona(Adafruit_ST7789& tft, const EspStatus& st) {
  // Persona glyph; clears down to the STUCK zone (freshness now lives in
  // the persona/llm lines above).
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
  drawPersonaBars(tft, st);
  drawPersonaAgo(tft, st);
  drawLlmBars(tft, st);
  drawLlmAgo(tft, st);
  drawAction(tft, st);
  drawPersona(tft, st);
  drawStuck(tft, st);
}
// NOTE: drawFull order matches the top-to-bottom layout (persona freshness
// sits right after the Persona bars, LLM freshness after the LLM bars).

static Snap snapOf(const EspStatus& st) {
  Snap cur;
  cur.title = titleText(st);
  cur.loopBadge = st.loop ? "ON" : "OFF";
  cur.headerC = headerColor(st);
  cur.model = modelText(st);
  cur.personaMask = 0;
  uint8_t pn = st.personaCount > BAR_N ? BAR_N : st.personaCount;
  for (uint8_t i = 0; i < pn; i++) {
    if (st.personaStatus[i]) cur.personaMask |= (uint16_t)(1u << i);
  }
  cur.personaCount = pn;
  cur.personaAgo = personaAgoText(st);
  cur.llmMask = 0;
  uint8_t n = st.llmCount > BAR_N ? BAR_N : st.llmCount;
  for (uint8_t i = 0; i < n; i++) {
    if (st.llmStatus[i]) cur.llmMask |= (uint16_t)(1u << i);
  }
  cur.llmCount = n;
  cur.llmAgo = llmAgoText(st);
  cur.action = actionText(st);
  cur.persona = personaDisplay(st.persona);
  cur.personaC = personaColor(st.persona);
  cur.personaSz = personaSize(cur.persona);
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
  if (cur.personaMask != prev.personaMask || cur.personaCount != prev.personaCount) drawPersonaBars(tft, st);
  if (cur.personaAgo != prev.personaAgo) drawPersonaAgo(tft, st);
  if (cur.llmMask != prev.llmMask || cur.llmCount != prev.llmCount) drawLlmBars(tft, st);
  if (cur.llmAgo != prev.llmAgo) drawLlmAgo(tft, st);
  if (cur.action != prev.action) drawAction(tft, st);
  if (cur.persona != prev.persona || cur.personaC != prev.personaC ||
      cur.personaSz != prev.personaSz)
    drawPersona(tft, st);
  if (cur.stuck != prev.stuck) drawStuck(tft, st);

  prev = cur;
}

// --- animation frame: v13 has no animated elements ---------------------------
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs) {
  (void)tft;
  (void)st;
  (void)nowMs;
  (void)lastPollMs;
}
