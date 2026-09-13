#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "ui.h"

TFT_eSPI tft;
EspStatus current;
String lastErr = "boot";
unsigned long lastPoll = 0;
bool firstDraw = true;

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(250);
  }
}

bool fetchStatus(EspStatus& out, String& errMsg) {
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(SERVER_URL)) { errMsg = "http.begin fail"; return false; }
  int code = http.GET();
  if (code != 200) { errMsg = "http " + String(code); http.end(); return false; }
  String body = http.getString();
  http.end();
  if (body.length() == 0 || body.length() > 2048) { errMsg = "bad body"; return false; }

  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) { errMsg = "json " + String(e.c_str()); return false; }

  out.ok = doc["ok"] | false;
  out.status = String((const char*)(doc["status"] | "grey"));
  out.loop = doc["loop"] | false;
  out.persona = String((const char*)(doc["persona"] | "-"));
  out.last = String((const char*)(doc["last"] | "-"));
  out.ago_s = doc["ago_s"] | -1;
  out.runs = doc["runs"] | 0;
  out.ok_n = doc["ok_n"] | 0;
  out.fail_n = doc["fail_n"] | 0;
  out.tok_today = doc["tok_today"] | 0;
  out.err = doc["err"] | 0;
  out.proj = String((const char*)(doc["proj"] | ""));
  out.offline = false;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(0); // portrait 170x320
  tft.fillScreen(TFT_BLACK);
  uiBoot(tft, WIFI_SSID);

  Serial.println("\nesp-status boot");
  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed");
    current.offline = true;
    lastErr = "wifi fail";
    uiDraw(tft, current, lastErr);
  }
}

void loop() {
  unsigned long now = millis();
  if (!firstDraw && now - lastPoll < POLL_MS) { delay(200); return; }
  firstDraw = false;
  lastPoll = now;

  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (WiFi.status() != WL_CONNECTED) {
    current.offline = true;
    lastErr = "wifi lost";
    uiDraw(tft, current, lastErr);
    Serial.println("wifi lost");
    return;
  }

  EspStatus next = current;
  String err;
  if (fetchStatus(next, err)) {
    current = next;
    uiDraw(tft, current, "");
    Serial.printf("ok %s %s %lds\n", current.status.c_str(), current.persona.c_str(), current.ago_s);
  } else {
    current.offline = true;
    lastErr = err.substring(0, 22);
    uiDraw(tft, current, lastErr);
    Serial.println("poll fail: " + err);
  }
}
