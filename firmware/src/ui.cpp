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
#define COL_GREY 0x8410
#define COL_LGREY 0xC618
#define COL_DGREY 0x7BEF
#define COL_FOOTER_BG 0x2104
#define COL_BAR_BG 0x18E3
#define COL_GLOSS 0xFFFF

// Layout (170x320 portrait, works for rotation 0 and 2)
#define DOT_CX (SCREEN_W / 2)
#define DOT_CY 72
#define DOT_R 26
#define TOP_H 26
#define BAR_Y TOP_H
#define BAR_H 4
#define LABEL_Y 110
#define LABEL_H 30
#define INFO_Y 142
#define INFO_H 12
#define LAST_CAP_Y 160
#define LAST_Y1 178
#define LAST_Y2 190
#define LAST_H 44
#define DIV_Y 214
#define STAT_Y1 222
#define STAT_Y2 238
#define STAT_Y3 254
#define FOOT_H 20
#define FOOT_Y (SCREEN_H - FOOT_H)

uint16_t statusColor(const String& s) {
  if (s == "green") return COL_GREEN;
  if (s == "yellow") return COL_YELLOW;
  if (s == "red") return COL_RED;
  return COL_GREY; // grey / offline
}

String fmtAgo(long ago_s) {
  if (ago_s < 0) return "never";
  if (ago_s < 60) return String(ago_s) + "s ago";
  if (ago_s < 3600) return String(ago_s / 60) + "m ago";
  return String(ago_s / 3600) + "h ago";
}

String fmtTokens(long n) {
  if (n >= 1000000) return String(n / 1000000.0, 1) + "M";
  if (n >= 1000) return String(n / 1000.0, 1) + "k";
  return String(n);
}

// Scale an RGB565 color by num/den (for glow / dim effects).
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
  if (title.length() > 14) title = title.substring(0, 14);
  return title;
}

static String infoText(const EspStatus& st) {
  String p = st.persona.length() ? st.persona : "-";
  if (p.length() > 12) p = p.substring(0, 12);
  return p + " " + fmtAgo(st.ago_s);
}

// --- static glow dot ------------------------------------------------------
static void drawDot(Adafruit_ST7789& tft, uint16_t c) {
  // Soft outer glow (two dim halos), solid core, glossy highlight.
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 8, dimColor(c, 1, 6));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 4, dimColor(c, 1, 3));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R, c);
  // Gloss: small white specular dot, dimmed so it reads as shine.
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 6, dimColor(COL_WHITE, 3, 4));
  tft.fillCircle(DOT_CX - 9, DOT_CY - 10, 3, COL_WHITE);
}

// Erase the animated pulse-ring zone back to black (ring orbits outside DOT_R).
static void clearPulseZone(Adafruit_ST7789& tft) {
  // Ring radii never exceed DOT_R+14. One band-clear is tiny — no visible flicker.
  int r = DOT_R + 15;
  tft.fillRect(0, DOT_CY - r, SCREEN_W, r * 2, COL_BLACK);
  // Dot itself was wiped by the band clear — redrawn by caller.
}

// --- boot -----------------------------------------------------------------
void uiBoot(Adafruit_ST7789& tft, const String& ssid) {
  tft.fillScreen(COL_BLACK);
  uint16_t c = COL_CYAN;
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R + 8, dimColor(c, 1, 6));
  tft.fillCircle(DOT_CX, DOT_CY, DOT_R, c);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextWrap(false);
  centerText(tft, "auto-pi", 120, 3);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  String sub = "connecting " + ssid;
  if (sub.length() > 28) sub = sub.substring(0, 28);
  centerText(tft, sub, 160, 1);
  centerText(tft, "flip v2 - warmup", 200, 1);
}

void uiBootStatus(Adafruit_ST7789& tft, const String& msg) {
  String m = msg;
  if (m.length() > 28) m = m.substring(0, 28);
  tft.fillRect(0, 176, SCREEN_W, 12, COL_BLACK);
  tft.setTextColor(COL_YELLOW, COL_BLACK);
  centerText(tft, m, 176, 1);
}

// --- differential data screen ---------------------------------------------
struct Snap {
  String title;
  String label;
  String info;
  String lastA;
  String lastB;
  String run, ok, fail, tok, loop, err;
  uint16_t color = 0;
  bool offline = true;
  bool valid = false;
};
static Snap prev;
static int prevBarW = -1;
static int prevRing1 = -1;
static int prevRing2 = -1;
static uint16_t prevRingColor = 0;

static void splitLast(const String& last, String& a, String& b) {
  a = last.substring(0, 28);
  b = last.length() > 28 ? last.substring(28, 56) : "";
}

static void drawTopBar(Adafruit_ST7789& tft, const String& title, uint16_t c) {
  tft.fillRect(0, 0, SCREEN_W, TOP_H, c);
  tft.setTextColor(COL_BLACK, c);
  centerText(tft, title, 6, 2);
  // Accent line under the bar + progress track.
  tft.fillRect(0, BAR_Y, SCREEN_W, BAR_H, COL_BAR_BG);
  prevBarW = -1; // force progress repaint
}

static void drawLabel(Adafruit_ST7789& tft, const String& label) {
  centerTextBand(tft, label, LABEL_Y, LABEL_H, 3, COL_WHITE, COL_BLACK);
}

static void drawInfo(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, INFO_Y - 2, SCREEN_W, INFO_H + 4, COL_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  tft.setCursor(8, INFO_Y);
  tft.print("persona:");
  tft.setTextColor(COL_CYAN, COL_BLACK);
  tft.setCursor(62, INFO_Y);
  tft.print(infoText(st));
}

static void drawLastBlock(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  tft.fillRect(0, LAST_CAP_Y - 2, SCREEN_W, LAST_H + 4, COL_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  tft.setCursor(8, LAST_CAP_Y);
  tft.print("last:");
  tft.setTextColor(COL_WHITE, COL_BLACK);
  String src = st.offline ? errMsg : st.last;
  String a, b;
  splitLast(src, a, b);
  tft.setCursor(8, LAST_Y1);
  tft.print(a);
  // Clear second line always (stale chars ghost when text shrinks).
  tft.fillRect(8, LAST_Y2, SCREEN_W - 16, 10, COL_BLACK);
  if (b.length()) {
    tft.setCursor(8, LAST_Y2);
    tft.print(b);
  }
}

static void drawStats(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, DIV_Y, SCREEN_W, FOOT_Y - DIV_Y, COL_BLACK);
  tft.drawFastHLine(0, DIV_Y, SCREEN_W, COL_DGREY);
  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setCursor(8, STAT_Y1);
  tft.print("RUN " + String(st.runs));
  tft.setTextColor(COL_GREEN, COL_BLACK);
  tft.setCursor(8, STAT_Y2);
  tft.print("OK " + String(st.ok_n));
  tft.setTextColor(COL_RED, COL_BLACK);
  tft.setCursor(8, STAT_Y3);
  tft.print("FAIL " + String(st.fail_n));
  tft.setTextColor(COL_YELLOW, COL_BLACK);
  tft.setCursor(90, STAT_Y1);
  tft.print("TOK " + fmtTokens(st.tok_today));
  tft.setTextColor(st.loop ? COL_GREEN : COL_RED, COL_BLACK);
  tft.setCursor(90, STAT_Y2);
  tft.print(st.loop ? "LOOP ON" : "LOOP OFF");
  tft.setTextColor(COL_DGREY, COL_BLACK);
  tft.setCursor(90, STAT_Y3);
  tft.print("ERR " + String(st.err));
}

static void drawFooter(Adafruit_ST7789& tft, const EspStatus& st) {
  tft.fillRect(0, FOOT_Y, SCREEN_W, FOOT_H, COL_FOOTER_BG);
  tft.setTextColor(COL_LGREY, COL_FOOTER_BG);
  centerText(tft, st.offline ? "poll failed - retry" : "poll 15s :8787", FOOT_Y + 6, 1);
}

static void drawFull(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg,
                     uint16_t c, const Snap& s) {
  (void)s;
  tft.fillScreen(COL_BLACK);
  tft.setTextWrap(false);
  drawTopBar(tft, titleText(st), c);
  drawDot(tft, c);
  prevRing1 = prevRing2 = -1;
  drawLabel(tft, statusLabel(st));
  drawInfo(tft, st);
  drawLastBlock(tft, st, errMsg);
  drawStats(tft, st);
  drawFooter(tft, st);
}

void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  uint16_t c = statusColor(st.offline ? "grey" : st.status);
  tft.setTextWrap(false);

  Snap cur;
  cur.title = titleText(st);
  cur.label = statusLabel(st);
  cur.info = infoText(st);
  splitLast(st.offline ? errMsg : st.last, cur.lastA, cur.lastB);
  cur.run = String(st.runs);
  cur.ok = String(st.ok_n);
  cur.fail = String(st.fail_n);
  cur.tok = fmtTokens(st.tok_today);
  cur.loop = st.loop ? "1" : "0";
  cur.err = String(st.err);
  cur.color = c;
  cur.offline = st.offline;
  cur.valid = true;

  if (!prev.valid) {
    drawFull(tft, st, errMsg, c, cur);
    prev = cur;
    return;
  }

  // Top bar (color or project name changed).
  if (cur.color != prev.color || cur.title != prev.title) {
    // Dot glow uses the same color — repaint dot + clear stale rings.
    // Note: the pulse-zone band (ends at LABEL_Y+3) clips the top of the
    // label, so repaint the label too.
    clearPulseZone(tft);
    drawTopBar(tft, cur.title, c);
    drawDot(tft, c);
    drawLabel(tft, cur.label);
    prevRing1 = prevRing2 = -1;
    prevRingColor = c;
  }
  if (cur.label != prev.label) drawLabel(tft, cur.label);
  if (cur.info != prev.info) drawInfo(tft, st);
  if (cur.lastA != prev.lastA || cur.lastB != prev.lastB) drawLastBlock(tft, st, errMsg);

  bool statsDirty = cur.run != prev.run || cur.ok != prev.ok || cur.fail != prev.fail ||
                    cur.tok != prev.tok || cur.loop != prev.loop || cur.err != prev.err;
  if (statsDirty) drawStats(tft, st);

  if (cur.offline != prev.offline) drawFooter(tft, st);

  prev = cur;
}

// --- animation tick (no fillScreen — tiny shapes only) ----------------------
void uiTick(Adafruit_ST7789& tft, const EspStatus& st,
            unsigned long nowMs, unsigned long lastPollMs) {
  uint16_t c = statusColor(st.offline ? "grey" : st.status);

  // Color changed outside uiDraw (e.g. first frames) — reset ring cache.
  if (c != prevRingColor) {
    if (prevRing1 >= 0) tft.drawCircle(DOT_CX, DOT_CY, prevRing1, COL_BLACK);
    if (prevRing2 >= 0) tft.drawCircle(DOT_CX, DOT_CY, prevRing2, COL_BLACK);
    prevRing1 = prevRing2 = -1;
    prevRingColor = c;
  }

  // Breathing double halo: two sine-phased rings orbiting the dot.
  float t = (float)(nowMs % 4000) / 4000.0f * 2.0f * (float)M_PI;
  int r1 = DOT_R + 6 + (int)(3.5f * (0.5f + 0.5f * sinf(t)));
  int r2 = DOT_R + 6 + (int)(3.5f * (0.5f + 0.5f * sinf(t + (float)M_PI)));
  if (r1 != prevRing1 || r2 != prevRing2 || c != prevRingColor) {
    if (prevRing1 >= 0 && prevRing1 != r1 && prevRing1 != r2)
      tft.drawCircle(DOT_CX, DOT_CY, prevRing1, COL_BLACK);
    if (prevRing2 >= 0 && prevRing2 != r1 && prevRing2 != r2)
      tft.drawCircle(DOT_CX, DOT_CY, prevRing2, COL_BLACK);
    tft.drawCircle(DOT_CX, DOT_CY, r1, dimColor(c, 1, 2));
    if (r2 != r1) tft.drawCircle(DOT_CX, DOT_CY, r2, dimColor(c, 1, 4));
    prevRing1 = r1;
    prevRing2 = r2;
    prevRingColor = c;
  }

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
