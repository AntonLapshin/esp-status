#include "ui.h"
#include "config.h"

uint16_t statusColor(const String& s) {
  if (s == "green") return 0x07E0;
  if (s == "yellow") return 0xFFE0;
  if (s == "red") return 0xF800;
  return 0x8410; // grey
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

void uiBoot(TFT_eSPI& tft, const String& ssid) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("auto-pi", SCREEN_W / 2, 90, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("connecting " + ssid, SCREEN_W / 2, 130, 2);
  tft.setTextDatum(TL_DATUM);
}

// Wrap helper: draw up to 2 lines of the "last" text.
static void drawLast(TFT_eSPI& tft, const String& last) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String a = last.substring(0, 22);
  String b = last.length() > 22 ? last.substring(22, 44) : "";
  tft.drawString(a, 8, 176, 2);
  if (b.length()) tft.drawString(b, 8, 194, 2);
}

void uiDraw(TFT_eSPI& tft, const EspStatus& st, const String& errMsg) {
  uint16_t c = statusColor(st.status);
  tft.fillScreen(TFT_BLACK);

  // Top bar: project + loop state
  tft.fillRect(0, 0, SCREEN_W, 26, c);
  tft.setTextColor(TFT_BLACK, c);
  tft.setTextDatum(TC_DATUM);
  String title = st.proj.length() ? st.proj : "auto-pi";
  if (title.length() > 20) title = title.substring(0, 20);
  tft.drawString(title, SCREEN_W / 2, 5, 2);
  tft.setTextDatum(TL_DATUM);

  // Big status dot + label
  tft.fillCircle(SCREEN_W / 2, 78, 30, c);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  String label = st.offline ? "OFFLINE" : st.status;
  label.toUpperCase();
  tft.drawString(label, SCREEN_W / 2, 116, 4);
  tft.setTextDatum(TL_DATUM);

  // Persona + ago
  tft.setTextColor(0xC618 /* light grey */, TFT_BLACK);
  tft.drawString("persona:", 8, 142, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String p = st.persona.length() ? st.persona : "-";
  if (p.length() > 14) p = p.substring(0, 14);
  tft.drawString(p + "  " + fmtAgo(st.ago_s), 66, 142, 2);

  // Last activity
  tft.setTextColor(0xC618, TFT_BLACK);
  tft.drawString("last:", 8, 160, 2);
  drawLast(tft, st.offline ? errMsg : st.last);

  // Stats grid
  tft.drawLine(0, 222, SCREEN_W, 222, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("RUN " + String(st.runs), 8, 230, 2);
  tft.setTextColor(0x07E0, TFT_BLACK);
  tft.drawString("OK " + String(st.ok_n), 8, 248, 2);
  tft.setTextColor(0xF800, TFT_BLACK);
  tft.drawString("FAIL " + String(st.fail_n), 8, 266, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("TOK " + fmtTokens(st.tok_today), 86, 230, 2);
  tft.setTextColor(st.loop ? 0x07E0 : 0xF800, TFT_BLACK);
  tft.drawString(st.loop ? "LOOP ON" : "LOOP OFF", 86, 248, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("ERR " + String(st.err), 86, 266, 2);

  // Footer
  tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 20, 0x2104);
  tft.setTextColor(0xC618, 0x2104);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(st.offline ? "poll failed - retrying" : "poll 15s :8787", SCREEN_W / 2, SCREEN_H - 17, 1);
  tft.setTextDatum(TL_DATUM);
}
