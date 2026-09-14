#include "ui.h"
#include "config.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define COL_BLACK 0x0000
#define COL_WHITE 0xFFFF
#define COL_GREEN 0x07E0
#define COL_YELLOW 0xFFE0
#define COL_RED 0xF800
#define COL_CYAN 0x07FF
#define COL_MAGENTA 0xF81F
#define COL_ORANGE 0xFD20
#define COL_GREY 0x8410
#define COL_LGREY 0xC618
#define COL_DGREY 0x7BEF
#define COL_FOOTER_BG 0x2104
#define COL_BAR_BG 0x18E3
#define COL_GAUGE_BG 0x18E3

// Layout v3 (170x320 portrait, works for rotation 0 and 2)
//   header  -> project + LOOP badge
//   hero    -> pulsating dot + GREEN/RED label
//   provider / gauge / persona stack
//   footer  -> poll countdown + freshness
#define TOP_H 30
#define BAR_Y TOP_H
#define BAR_H 4
#define DOT_CX (SCREEN_W / 2)
#define DOT_CY 76
#define DOT_R 26
#define DOT_ZONE_TOP 42
#define DOT_ZONE_BOT 110
#define LABEL_Y 114
#define LABEL_H 30
#define PROV_CAP_Y 150
#define PROV_Y 160
#define GAUGE_CAP_Y 184
#define GAUGE_Y 194
#define GAUGE_H 14
#define GAUGE_X 14
#define GAUGE_W (SCREEN_W - 2 * GAUGE_X)
#define GAUGE_TXT_Y 212
#define PERS_CAP_Y 236
#define PERS_Y 246
#define AGO_Y 280
#define FOOT_H 20
#define FOOT_Y (SCREEN_H - FOOT_H)

uint16_t statusColor(const String& s) {
  if (s == "green") return COL_GREEN;
  if (s == "red") return COL_RED;
  if (s == "yellow") return COL_YELLOW; // legacy pre-v3 servers
  return COL_GREY; // grey / offline
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

// Scale an RGB565 color by num/den (for glow / pulse effects).
static uint16_t dimColor(uint16_t c, uint8_t num, uint8_t den) {
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >> 5) & 0x3F;
  uint8_t b = c & 0x1F;
  r = (r * num) / den;
  g = (g * num) / den;
  b = (b * num) / den;
  return (r << 11) | (g << 5) | b;
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

// Clear a horizontal band then print centered text inside it.
static void centerTextBand(Adafruit_ST7789& tft, const String& s, int y, int h,
                           uint8_t size, uint16_t fg, uint16_t bg) {
  tft.fillRect(0, y, SCREEN_W, h, bg);
  tft.setTextColor(fg, bg);
  centerText(tft, s, y, size);
}

static String statusLabel(const EspStatus& st) {
  String label = st.offline ? "OFFLINE" : st.status;
  label.toUpperCase();
  return label;
}

static String titleText(const EspStatus& st) {
  String title = st.proj.length() ? st.proj : "auto-pi";
  if (title.length() > 10) title = title.substring(0, 10);
  return title;
}

static String providerText(const EspStatus& st) {
  String p = st.provider.length() ? st.provider : "-";
  if (p.length() > 12) p = p.substring(0, 12);
  return p;
}

static int successPct(const EspStatus& st) {
  if (st.total <= 0) return 0;
  return (int)((st.succ * 100L) / st.total);
}

static uint16_t gaugeColor(int pct) {
  if (pct >= 90) return COL_GREEN;
  if (pct >= 60) return COL_YELLOW;
  return COL_RED;
}

static String gaugeText(const EspStatus& st) {
  return String(st.succ) + "/" + String(st.total) + " " + String(successPct(st)) + "%";
}

// --- hero dot -------------------------------------------------------------
// Static paint (used by full redraw); the per-frame pulse lives in uiTick.
static void drawDot(Adafruit_ST7789& tft, uint16_t c) {
  // Soft outer glow (two dim halos), solid core, glossy highlight.
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 8, dimColor(c, 1, 6));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 4, dimColor(c, 1, 3));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R, c);
  // Gloss: small white specular dot, dimmed so it reads as shine.
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 6, dimColor(COL_WHITE, 3, 4));
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 3, COL_WHITE);
}

// --- boot -----------------------------------------------------------------
void uiBoot(Adafruit_ST7789& tft, const String& ssid) {
  tft.fillScreen(COL_BLACK);
  uint16_t c = COL_CYAN;
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 8, dimColor(c, 1, 6));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R, c);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextWrap(false);
  centerText(tft, "esp-status", 122, 3);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  String sub = "v3 connecting " + ssid;
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

// --- differential data screen ---------------------------------------------
struct Snap {
  String title;
  String loopBadge;
  String label;
  String provider;
  String gauge;
  int gaugeW = -1;
  uint16_t gaugeC = 0;
  String persona;
  uint16_t personaC = 0;
  uint8_t personaSize = 0;
  String ago;
  String footer;
  uint16_t color = 0;
  bool offline = true;
  bool valid = false;
};
static Snap prev;
static int prevBarW = -1;

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
  // Poll progress track under the bar.
  tft.fillRect(0, BAR_Y, SCREEN_W, BAR_H, COL_BAR_BG);
  prevBarW = -1; // force progress repaint
}

static void drawLabel(Adafruit_ST7789& tft, const String& label) {
  centerTextBand(tft, label, LABEL_Y, LABEL_H, 3, COL_WHITE, COL_BLACK);
}

static void drawCaption(Adafruit_ST7789& tft, const String& cap, int y) {
  tft.setTextSize(1);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  centerText(tft, cap, y, 1);
}

static void drawProvider(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, PROV_CAP_Y - 2, SCREEN_W, 30, COL_BLACK);
  drawCaption(tft, "PROVIDER", PROV_CAP_Y);
  tft.setTextColor(COL_CYAN, COL_BLACK);
  centerText(tft, providerText(st), PROV_Y, 2);
}

static void drawGauge(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, GAUGE_CAP_Y - 2, SCREEN_W, 48, COL_BLACK);
  drawCaption(tft, "SUCCESS", GAUGE_CAP_Y);
  int pct = successPct(st);
  uint16_t gc = gaugeColor(pct);
  int fillW = st.total > 0 ? (int)((GAUGE_W * st.succ) / st.total) : 0;
  // Track + fill + hairline border.
  tft.fillRect(GAUGE_X, GAUGE_Y, GAUGE_W, GAUGE_H, COL_GAUGE_BG);
  if (fillW > 0) tft.fillRect(GAUGE_X, GAUGE_Y, fillW, GAUGE_H, gc);
  tft.drawRect(GAUGE_X - 1, GAUGE_Y - 1, GAUGE_W + 2, GAUGE_H + 2, COL_DGREY);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  centerText(tft, gaugeText(st), GAUGE_TXT_Y, 2);
}

static uint8_t personaSize(const String& disp) {
  if (disp.length() <= 2) return 4; // PM, QA — hero glyphs
  if (disp.length() <= 7) return 3; // ENGINEER, REVIEW
  return 2;
}

static void drawPersona(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, PERS_CAP_Y - 2, SCREEN_W, AGO_Y + 10 - PERS_CAP_Y, COL_BLACK);
  drawCaption(tft, "PERSONA", PERS_CAP_Y);
  String disp = personaDisplay(st.persona);
  uint16_t pc = personaColor(st.persona);
  tft.setTextColor(pc, COL_BLACK);
  centerText(tft, disp, PERS_Y, personaSize(disp));
  tft.setTextColor(COL_LGREY, COL_BLACK);
  centerText(tft, fmtAgo(st.ago_s), AGO_Y, 1);
}

static void drawFooter(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  tft.fillRect(0, FOOT_Y, SCREEN_W, FOOT_H, COL_FOOTER_BG);
  tft.setTextColor(COL_LGREY, COL_FOOTER_BG);
  String f = st.offline
      ? (errMsg.length() ? errMsg : "poll failed - retry")
      : ("poll 15s - " + fmtAgo(st.ago_s));
  if (f.length() > 28) f = f.substring(0, 28);
  centerText(tft, f, FOOT_Y + 6, 1);
}

static void drawFull(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg,
                     uint16_t c) {
  tft.fillScreen(COL_BLACK);
  tft.setTextWrap(false);
  drawTopBar(tft, st, c);
  drawDot(tft, c);
  drawLabel(tft, statusLabel(st));
  drawProvider(tft, st);
  drawGauge(tft, st);
  drawPersona(tft, st);
  drawFooter(tft, st, errMsg);
}

static Snap snapOf(const EspStatus& st, const String& errMsg) {
  Snap cur;
  cur.title = titleText(st);
  cur.loopBadge = st.loop ? "ON" : "OFF";
  cur.label = statusLabel(st);
  cur.provider = providerText(st);
  cur.gauge = gaugeText(st);
  cur.gaugeW = st.total > 0 ? (int)((GAUGE_W * st.succ) / st.total) : 0;
  cur.gaugeC = gaugeColor(successPct(st));
  cur.persona = personaDisplay(st.persona);
  cur.personaC = personaColor(st.persona);
  cur.personaSize = personaSize(cur.persona);
  cur.ago = fmtAgo(st.ago_s);
  cur.footer = st.offline
      ? (errMsg.length() ? errMsg.substring(0, 28) : "poll failed - retry")
      : ("poll 15s - " + cur.ago);
  if (cur.footer.length() > 28) cur.footer = cur.footer.substring(0, 28);
  cur.color = statusColor(st.offline ? "grey" : st.status);
  cur.offline = st.offline;
  cur.valid = true;
  return cur;
}

void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  uint16_t c = statusColor(st.offline ? "grey" : st.status);
  tft.setTextWrap(false);
  Snap cur = snapOf(st, errMsg);

  if (!prev.valid) {
    drawFull(tft, st, errMsg, c);
    prev = cur;
    return;
  }

  // Top bar (color, project or loop state changed) — dot glow shares the
  // color, so repaint the dot too.
  if (cur.color != prev.color || cur.title != prev.title || cur.loopBadge != prev.loopBadge) {
    drawTopBar(tft, st, c);
    tft.fillRect(0, DOT_ZONE_TOP, SCREEN_W, DOT_ZONE_BOT - DOT_ZONE_TOP, COL_BLACK);
    drawDot(tft, c);
  }
  if (cur.label != prev.label) drawLabel(tft, cur.label);
  if (cur.provider != prev.provider) drawProvider(tft, st);
  if (cur.gauge != prev.gauge || cur.gaugeW != prev.gaugeW || cur.gaugeC != prev.gaugeC)
    drawGauge(tft, st);
  if (cur.persona != prev.persona || cur.personaC != prev.personaC ||
      cur.personaSize != prev.personaSize || cur.ago != prev.ago)
    drawPersona(tft, st);
  if (cur.footer != prev.footer) drawFooter(tft, st, errMsg);

  prev = cur;
}

// --- animation tick (no fillScreen — tiny shapes only) ----------------------
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs) {
  uint16_t c = statusColor(st.offline ? "grey" : st.status);

  // Breathing pulse: 2.4 s period, core brightness 70-100% + orbiting halos.
  float t = (float)(nowMs % 2400) / 2400.0f * 2.0f * (float)M_PI;
  float k = 0.5f + 0.5f * sinf(t); // 0..1
  uint8_t num = (uint8_t)(7.0f + 3.0f * k); // 7..10
  uint16_t core = dimColor(c, num, 10);

  // Repaint the dot zone (small band — no visible flicker).
  tft.fillRect(0, DOT_ZONE_TOP, SCREEN_W, DOT_ZONE_BOT - DOT_ZONE_TOP, COL_BLACK);
  // Outer static glow + breathing mid halo + pulsing core + gloss.
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 8, dimColor(c, 1, 6));
  int mid = DOT_R + 3 + (int)(3.0f * k);
  tft.fillCircle(DOT_CX, DOT_CY, mid, dimColor(c, 1, 3));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R, core);
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 6, dimColor(COL_WHITE, 3, 4));
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 3, COL_WHITE);
  // Pulse ring orbiting just outside the dot.
  int r = DOT_R + 6 + (int)(3.0f * k);
  tft.drawCircle(DOT_CX, DOT_CY, r, dimColor(c, 1, 2));

  // Poll countdown bar (grows 0 -> full width over POLL_MS).
  unsigned long elapsed = nowMs - lastPollMs;
  if (elapsed > POLL_MS) elapsed = POLL_MS;
  int w = (int)((elapsed * (unsigned long)SCREEN_W) / POLL_MS);
  if (w != prevBarW) {
    if (w < prevBarW || prevBarW < 0) {
      // New cycle (or first run): repaint track then fill.
      tft.fillRect(0, BAR_Y, SCREEN_W, BAR_H, COL_BAR_BG);
      if (w > 0) tft.fillRect(0, BAR_Y, w, BAR_H, c);
    } else {
      tft.fillRect(prevBarW, BAR_Y, w - prevBarW, BAR_H, c);
    }
    prevBarW = w;
  }
}
