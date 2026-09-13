#include "ui.h"
#include "config.h"

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

// Centered text helper (Adafruit GFX has no drawString/datum).
static void centerText(Adafruit_ST7789& tft, const String& s, int y, uint8_t size) {
  tft.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - (int)w) / 2, y);
  tft.print(s);
}

void uiBoot(Adafruit_ST7789& tft, const String& ssid) {
  tft.fillScreen(COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextWrap(false);
  centerText(tft, "auto-pi", 90, 3);
  tft.setTextColor(COL_DGREY, COL_BLACK);
  String sub = "connecting " + ssid;
  if (sub.length() > 28) sub = sub.substring(0, 28);
  centerText(tft, sub, 135, 1);
}

static void drawLast(Adafruit_ST7789& tft, const String& last) {
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextSize(1);
  String a = last.substring(0, 28);
  String b = last.length() > 28 ? last.substring(28, 56) : "";
  tft.setCursor(8, 178);
  tft.print(a);
  if (b.length()) {
    tft.setCursor(8, 190);
    tft.print(b);
  }
}

void uiDraw(Adafruit_ST7789& tft, const EspStatus& st, const String& errMsg) {
  uint16_t c = statusColor(st.offline ? "grey" : st.status);
  tft.fillScreen(COL_BLACK);
  tft.setTextWrap(false);

  // Top bar: project name
  tft.fillRect(0, 0, SCREEN_W, 26, c);
  tft.setTextColor(COL_BLACK, c);
  String title = st.proj.length() ? st.proj : "auto-pi";
  if (title.length() > 14) title = title.substring(0, 14);
  centerText(tft, title, 6, 2);

  // Big status dot + label
  tft.fillCircle(SCREEN_W / 2, 72, 28, c);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  String label = st.offline ? "OFFLINE" : st.status;
  label.toUpperCase();
  centerText(tft, label, 110, 3);

  // Persona + ago (size 1 fits 28 chars)
  tft.setTextSize(1);
  tft.setTextColor(COL_LGREY, COL_BLACK);
  tft.setCursor(8, 142);
  tft.print("persona:");
  tft.setTextColor(COL_CYAN, COL_BLACK);
  String p = st.persona.length() ? st.persona : "-";
  if (p.length() > 12) p = p.substring(0, 12);
  tft.setCursor(62, 142);
  tft.print(p + " " + fmtAgo(st.ago_s));

  // Last activity
  tft.setTextColor(COL_LGREY, COL_BLACK);
  tft.setCursor(8, 160);
  tft.print("last:");
  drawLast(tft, st.offline ? errMsg : st.last);

  // Stats grid
  tft.drawFastHLine(0, 214, SCREEN_W, COL_DGREY);
  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setCursor(8, 222);
  tft.print("RUN " + String(st.runs));
  tft.setTextColor(COL_GREEN, COL_BLACK);
  tft.setCursor(8, 238);
  tft.print("OK " + String(st.ok_n));
  tft.setTextColor(COL_RED, COL_BLACK);
  tft.setCursor(8, 254);
  tft.print("FAIL " + String(st.fail_n));
  tft.setTextColor(COL_YELLOW, COL_BLACK);
  tft.setCursor(90, 222);
  tft.print("TOK " + fmtTokens(st.tok_today));
  tft.setTextColor(st.loop ? COL_GREEN : COL_RED, COL_BLACK);
  tft.setCursor(90, 238);
  tft.print(st.loop ? "LOOP ON" : "LOOP OFF");
  tft.setTextColor(COL_DGREY, COL_BLACK);
  tft.setCursor(90, 254);
  tft.print("ERR " + String(st.err));

  // Footer
  tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 20, COL_FOOTER_BG);
  tft.setTextColor(COL_LGREY, COL_FOOTER_BG);
  centerText(tft, st.offline ? "poll failed - retry" : "poll 15s :8787", SCREEN_H - 14, 1);
}
