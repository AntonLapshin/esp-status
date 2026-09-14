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
#define COL_GAUGE_BG 0x18E3

// Layout v5 (170x320 portrait, works for rotation 0 and 2)
//   header  -> project + LOOP badge (loop status lives here, no dot)
//   provider (large) / speedometer gauge (no text) / persona glyph + ago
//   (no SUCCESS/PERSONA captions, no footer, no poll bar)
#define TOP_H 30
#define PROV_CAP_Y 44
#define PROV_Y 56
#define GAUGE_CX (SCREEN_W / 2)
#define GAUGE_CY 184
#define GAUGE_R 54
#define GAUGE_ZONE_TOP 112
#define GAUGE_ZONE_BOT 202
#define PERS_Y 212
#define AGO_Y 258

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

// Scale an RGB565 color by num/den (for dimmed gauge zones / dot halo).
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

static String titleText(const EspStatus& st) {
  String title = st.proj.length() ? st.proj : "auto-pi";
  if (title.length() > 10) title = title.substring(0, 10);
  return title;
}

static String providerText(const EspStatus& st) {
  String p = st.provider.length() ? st.provider : "-";
  if (p.length() > 9) p = p.substring(0, 9); // size-3 text must fit 170px
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

// --- boot -------------------------------------------------------------------
void uiBoot(Adafruit_ST7789& tft, const String& ssid) {
  tft.fillScreen(COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextWrap(false);
  centerText(tft, "esp-status", 122, 3);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  String sub = "v5 connecting " + ssid;
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
  String provider;
  int pct = -1;
  uint16_t gaugeC = 0;
  String persona;
  uint16_t personaC = 0;
  uint8_t personaSize = 0;
  String ago;
  uint16_t color = 0;
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

static void drawProvider(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, PROV_CAP_Y - 2, SCREEN_W, PROV_Y + 26 - PROV_CAP_Y + 2, COL_BLACK);
  drawCaption(tft, "PROVIDER", PROV_CAP_Y);
  tft.setTextColor(COL_CYAN, COL_BLACK);
  centerText(tft, providerText(st), PROV_Y, 3);
}

// --- speedometer gauge ------------------------------------------------------
// Semicircle (180..360 deg): zone backdrop + value sweep,
// 11 white major ticks only (no minor marks), white needle, colored hub.
// pctF is animated (see uiDraw/uiTick); backdrop zones stay static.
//
// arcBand draws a SOLID annular sector: the band is tiled with small quads
// (two fillTriangle each) instead of radial 1px lines from the center.
// Radial lines leave 1px black pinholes at the outer edge because a 1 deg
// step spans ~0.94px at R=54 and integer rounding opens gaps. Quad tiling
// shares exact edge vertices between neighbours, so no gaps are possible.
static void arcBand(Adafruit_ST7789& tft, float a0deg, float a1deg,
                    int rOuter, int rInner, uint16_t color) {
  float span = a1deg - a0deg;
  if (span <= 0.0f) return;
  // ~2 deg per quad: smooth at R=54 (chord error < 0.1px), few triangles.
  int n = (int)ceilf(span / 2.0f);
  if (n < 1) n = 1;
  for (int i = 0; i < n; i++) {
    float a = a0deg + span * (float)i / (float)n;
    float b = a0deg + span * (float)(i + 1) / (float)n;
    float ra = a * (float)M_PI / 180.0f;
    float rb = b * (float)M_PI / 180.0f;
    float ca = cosf(ra), sa = sinf(ra);
    float cb = cosf(rb), sb = sinf(rb);
    int x0i = GAUGE_CX + (int)(rInner * ca);
    int y0i = GAUGE_CY + (int)(rInner * sa);
    int x0o = GAUGE_CX + (int)(rOuter * ca);
    int y0o = GAUGE_CY + (int)(rOuter * sa);
    int x1i = GAUGE_CX + (int)(rInner * cb);
    int y1i = GAUGE_CY + (int)(rInner * sb);
    int x1o = GAUGE_CX + (int)(rOuter * cb);
    int y1o = GAUGE_CY + (int)(rOuter * sb);
    tft.fillTriangle(x0i, y0i, x0o, y0o, x1i, y1i, color);
    tft.fillTriangle(x0o, y0o, x1o, y1o, x1i, y1i, color);
  }
}

static void drawSpeedometerAt(Adafruit_ST7789& tft, float pctF) {
  tft.fillRect(0, GAUGE_ZONE_TOP, SCREEN_W, GAUGE_ZONE_BOT - GAUGE_ZONE_TOP, COL_BLACK);
  if (pctF < 0) pctF = 0;
  if (pctF > 100) pctF = 100;
  int pct = (int)(pctF + 0.5f);
  uint16_t gc = gaugeColor(pct);
  const int rO = GAUGE_R, rI = GAUGE_R - 12;
  // Zone backdrop: red 0-60% dim, yellow 60-90% + green 90-100% bright.
  arcBand(tft, 180.0f, 288.0f, rO, rI, dimColor(COL_RED, 1, 4));
  arcBand(tft, 288.0f, 342.0f, rO, rI, dimColor(COL_YELLOW, 1, 2));
  arcBand(tft, 342.0f, 360.0f, rO, rI, dimColor(COL_GREEN, 1, 2));
  // Bright value sweep (follows the animated needle).
  arcBand(tft, 180.0f, 180.0f + 1.8f * pctF, rO, rI, gc);
  // 11 major ticks only, white.
  for (int i = 0; i <= 10; i++) {
    float a = (180.0f + (float)i * 18.0f) * (float)M_PI / 180.0f;
    float c = cosf(a), s = sinf(a);
    tft.drawLine(GAUGE_CX + (int)((rO + 2) * c), GAUGE_CY + (int)((rO + 2) * s),
                 GAUGE_CX + (int)((rO + 8) * c), GAUGE_CY + (int)((rO + 8) * s),
                 COL_WHITE);
  }
  // Needle + hub.
  float na = (180.0f + 1.8f * pctF) * (float)M_PI / 180.0f;
  tft.drawLine(GAUGE_CX, GAUGE_CY,
               GAUGE_CX + (int)((rI - 4) * cosf(na)),
               GAUGE_CY + (int)((rI - 4) * sinf(na)), COL_WHITE);
  tft.fillCircle(GAUGE_CX, GAUGE_CY, 7, gc);
  tft.fillCircle(GAUGE_CX, GAUGE_CY, 3, COL_WHITE);
}

static void drawSpeedometer(Adafruit_ST7789& tft, const EspStatus& st) {
  drawSpeedometerAt(tft, (float)successPct(st));
}

// Needle animation state: uiDraw sets the target, uiTick eases shownPct
// towards it so the arrow sweeps instead of jumping on every poll.
static float shownPct = -1.0f; // currently displayed value (<0 = uninit)
static float animFrom = 0.0f;
static float animTo = 0.0f;
static unsigned long animT0 = 0;
static bool animating = false;
static const unsigned long ANIM_MS = 700;

static void startNeedleAnim(float target) {
  if (shownPct < 0) {
    shownPct = target;
    animating = false;
    return;
  }
  animFrom = shownPct;
  animTo = target;
  animT0 = millis();
  animating = (animFrom != animTo);
}

static uint8_t personaSize(const String& disp) {
  if (disp.length() <= 2) return 4; // PM, QA — hero glyphs
  if (disp.length() <= 7) return 3; // ENGINEER, REVIEW
  return 2;
}

static void drawPersona(Adafruit_ST7789& tft, const EspStatus& st) {
  // Persona glyph + large freshness; clears to the bottom (no footer).
  tft.fillRect(0, PERS_Y - 10, SCREEN_W, SCREEN_H - (PERS_Y - 10), COL_BLACK);
  String disp = personaDisplay(st.persona);
  uint16_t pc = personaColor(st.persona);
  tft.setTextColor(pc, COL_BLACK);
  centerText(tft, disp, PERS_Y, personaSize(disp));
  tft.setTextColor(COL_LGREY, COL_BLACK);
  centerText(tft, fmtAgo(st.ago_s), AGO_Y, 2);
}

static void drawFull(Adafruit_ST7789& tft, const EspStatus& st, uint16_t c) {
  tft.fillScreen(COL_BLACK);
  tft.setTextWrap(false);
  drawTopBar(tft, st, c);
  drawProvider(tft, st);
  shownPct = (float)successPct(st);
  animating = false;
  drawSpeedometerAt(tft, shownPct);
  drawPersona(tft, st);
}

static Snap snapOf(const EspStatus& st) {
  Snap cur;
  cur.title = titleText(st);
  cur.loopBadge = st.loop ? "ON" : "OFF";
  cur.provider = providerText(st);
  cur.pct = successPct(st);
  cur.gaugeC = gaugeColor(cur.pct);
  cur.persona = personaDisplay(st.persona);
  cur.personaC = personaColor(st.persona);
  cur.personaSize = personaSize(cur.persona);
  cur.ago = fmtAgo(st.ago_s);
  cur.color = statusColor(st.offline ? "grey" : st.status);
  cur.offline = st.offline;
  cur.valid = true;
  return cur;
}

void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  (void)errMsg; // no footer to show it on; offline state still greys the UI
  uint16_t c = statusColor(st.offline ? "grey" : st.status);
  tft.setTextWrap(false);
  Snap cur = snapOf(st);

  if (!prev.valid) {
    drawFull(tft, st, c);
    prev = cur;
    return;
  }

  // Top bar carries the loop status (ON/OFF + bar color) — no dot.
  if (cur.color != prev.color || cur.title != prev.title || cur.loopBadge != prev.loopBadge) {
    drawTopBar(tft, st, c);
  }
  if (cur.provider != prev.provider) drawProvider(tft, st);
  if (cur.pct != prev.pct || cur.gaugeC != prev.gaugeC)
    startNeedleAnim((float)cur.pct); // uiTick eases the arrow there
  if (cur.persona != prev.persona || cur.personaC != prev.personaC ||
      cur.personaSize != prev.personaSize || cur.ago != prev.ago)
    drawPersona(tft, st);

  prev = cur;
}

// --- needle animation: eased sweep after every poll --------------------------
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs) {
  (void)st;
  (void)lastPollMs;
  if (!animating || shownPct < 0) return;
  unsigned long dt = (nowMs >= animT0) ? (nowMs - animT0) : 0;
  float t = (float)dt / (float)ANIM_MS;
  if (t >= 1.0f) {
    shownPct = animTo;
    animating = false;
    drawSpeedometerAt(tft, shownPct);
    return;
  }
  // Ease-out cubic: fast start, soft landing.
  float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
  shownPct = animFrom + (animTo - animFrom) * e;
  drawSpeedometerAt(tft, shownPct);
}
