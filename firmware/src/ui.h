#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

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
void uiBoot(TFT_eSPI& tft, const String& ssid);
void uiDraw(TFT_eSPI& tft, const EspStatus& st, const String& errMsg);
