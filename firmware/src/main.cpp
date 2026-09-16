#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "config.h"
#include "ui.h"

// IdeaSpark 1.9" ST7789 (fixed on-board): MOSI=23 SCLK=18 CS=15 DC=2 RST=4 BL=32
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define TFT_MOSI 23
#define TFT_SCLK 18

#define TICK_MS 40

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
EspStatus current;
String lastErr = "boot";
unsigned long lastPoll = 0;
unsigned long lastTick = 0;

// --- Persona-run + LLM-turn outcome bars -------------------------------------
// The server sends `last10PersonaStatus` (whole persona runs) and
// `last10LlmStatus` (individual LLM turns), both up to 10 booleans newest
// first; the device stores them oldest-first so bar 0 is the oldest (left)
// and the last bar is the newest (right). Fewer than 10 recorded -> solid
// grey bars on the left (right-aligned data).

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(250);
    dots = (dots + 1) % 4;
    String msg = "connecting " + String(WIFI_SSID) + String("...").substring(0, dots);
    uiBootStatus(tft, msg);
  }
}

// Newest-first wire array -> oldest-first memory (bar 0 = oldest).
static uint8_t parseBars(JsonArray arr, bool* out) {
  if (arr.isNull()) return 0;
  size_t n = arr.size();
  if (n > 10) n = 10;
  for (size_t i = 0; i < n; i++) {
    out[i] = arr[n - 1 - i].as<bool>();
  }
  return (uint8_t)n;
}

bool fetchStatus(EspStatus& out, String& errMsg) {
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(SERVER_URL)) { errMsg = "http.begin fail"; return false; }
  int code = http.GET();
  if (code != 200) { errMsg = "http " + String(code); http.end(); return false; }
  String body = http.getString();
  http.end();
  if (body.length() == 0 || body.length() > 4096) { errMsg = "bad body"; return false; }

  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) { errMsg = "json " + String(e.c_str()); return false; }

  out.ok = doc["ok"] | false;
  out.proj = String((const char*)(doc["proj"] | ""));
  out.loop = doc["loop"] | false;
  out.stuck = doc["stuck"] | false;
  out.persona = String((const char*)(doc["persona"] | "-"));
  out.model = String((const char*)(doc["model"] | "-"));
  out.lastAction = String((const char*)(doc["lastAction"] | "-"));
  out.lastActionAgoS = doc["lastActionAgoS"] | -1;
  // v10 persona-run bars (+ fallback: v9 backends send persona outcomes in
  // `last10LlmStatus` with no persona fields — mirror them so the PERSONA
  // row still shows history during rollout).
  JsonArray parr = doc["last10PersonaStatus"];
  if (!parr.isNull()) {
    out.personaCount = parseBars(parr, out.personaStatus);
  } else {
    JsonArray legacy = doc["last10LlmStatus"];
    out.personaCount = parseBars(legacy, out.personaStatus);
  }
  if (!doc["lastPersonaCallFinished"].isNull()) {
    out.lastPersonaCallFinished = doc["lastPersonaCallFinished"] | -1;
  } else {
    out.lastPersonaCallFinished = doc["lastLlmCallFinished"] | -1;
  }
  // v10 true per-turn LLM bars (empty until the upgraded backend records
  // llm.jsonl turns; on a v9 backend this mirrors the persona array above).
  JsonArray larr = doc["last10LlmStatus"];
  out.llmCount = parseBars(larr, out.llmStatus);
  out.lastLlmCallFinished = doc["lastLlmCallFinished"] | -1;
  out.offline = false;
  return true;
}

static void doPoll() {
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
    Serial.printf("ok %s %s%s %s %s pers%lds llm%lds act:%s %lds pbars:%d lbars:%d\n",
                  current.proj.c_str(), current.loop ? "ON" : "OFF",
                  current.stuck ? " STUCK" : "",
                  current.persona.c_str(), current.model.c_str(),
                  current.lastPersonaCallFinished,
                  current.lastLlmCallFinished,
                  current.lastAction.c_str(), current.lastActionAgoS,
                  current.personaCount, current.llmCount);
  } else {
    current.offline = true;
    lastErr = err.substring(0, 22);
    uiDraw(tft, current, lastErr);
    Serial.println("poll fail: " + err);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LCD_BL_PIN, OUTPUT);
  // 50% brightness (was full HIGH): PWM duty 128/255 to dim the panel.
  analogWrite(LCD_BL_PIN, 128);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(SCREEN_W, SCREEN_H);
  tft.setRotation(DISPLAY_ROTATION); // 2 = flipped 180 deg (vertical flip)
  tft.invertDisplay(true);
  tft.fillScreen(ST77XX_BLACK);
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
  lastPoll = millis();
  lastTick = millis();
  // First data frame immediately (uiDraw does the one full paint;
  // later polls only patch dirty rects, so no 15s full-screen wipe).
  doPoll();
  lastPoll = millis();
}

void loop() {
  unsigned long now = millis();

  // Poll every POLL_MS (non-blocking — animation keeps running).
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    doPoll();
  }

  // Animation frame: breathing halo + poll countdown bar. Tiny shapes only.
  if (now - lastTick >= TICK_MS) {
    lastTick = now;
    uiTick(tft, current, now, lastPoll);
  }

  delay(5);
}
