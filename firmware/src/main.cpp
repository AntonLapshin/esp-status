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

// --- Rolling last-10 provider-call window -----------------------------------
// The gauge shows strictly the last 10 provider calls actually observed by
// this device. The server sends cumulative succ/total counters; individual
// outcomes are inferred from counter deltas between polls and kept in a ring
// buffer. Nothing is ever backfilled from cumulative totals — on boot,
// counter reset, or provider switch the window starts empty and fills up as
// new calls are observed.
namespace {
constexpr int kWinN = 10;
bool winHist[kWinN];
int winCount = 0;  // valid entries (0..10)
int winIdx = 0;    // next write position
long prevRawSucc = -1, prevRawTotal = -1;
String prevRawProv = "";

void winPush(bool ok) {
  winHist[winIdx] = ok;
  winIdx = (winIdx + 1) % kWinN;
  if (winCount < kWinN) winCount++;
}

// Strict last-10: drop everything, wait for fresh deltas. No proportional
// seeding from cumulative counters — that would mix lifetime history into
// a gauge that must reflect only the most recent calls.
void winClear() {
  winCount = 0;
  winIdx = 0;
}
}  // namespace

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
  out.proj = String((const char*)(doc["proj"] | ""));
  out.loop = doc["loop"] | false;
  // v3 contract is binary GREEN/RED; map legacy yellow/grey to red.
  String s = String((const char*)(doc["status"] | "red"));
  out.status = (s == "green") ? "green" : "red";
  out.provider = String((const char*)(doc["provider"] | "-"));
  long rawSucc = doc["succ"] | 0;
  long rawTotal = doc["total"] | 0;
  // Fold cumulative counters into the strict last-10 window; gauge reads
  // out.succ/total. Resets clear the window — no backfill from totals.
  bool reset = (prevRawTotal < 0) || (out.provider != prevRawProv) ||
               (rawTotal < prevRawTotal) || (rawSucc < 0) || (rawSucc > rawTotal) ||
               (rawSucc < prevRawSucc);
  if (reset) {
    winClear();
  } else {
    long dSucc = rawSucc - prevRawSucc;
    long dTotal = rawTotal - prevRawTotal;
    if (dTotal > 0) {
      if (dSucc < 0) dSucc = 0;
      if (dSucc > dTotal) dSucc = dTotal;
      long nSucc = dSucc, nFail = dTotal - dSucc;
      // Keep only the most recent 10 of the batch (fails treated as newest).
      if (nSucc + nFail > kWinN) {
        if (nFail >= kWinN) { nSucc = 0; nFail = kWinN; }
        else { nSucc = kWinN - nFail; }
      }
      for (long i = 0; i < nSucc; i++) winPush(true);
      for (long i = 0; i < nFail; i++) winPush(false);
    }
  }
  prevRawSucc = rawSucc;
  prevRawTotal = rawTotal;
  prevRawProv = out.provider;
  long wSucc = 0;
  for (int i = 0; i < winCount; i++) if (winHist[i]) wSucc++;
  out.succ = wSucc;
  out.total = winCount;
  out.persona = String((const char*)(doc["persona"] | "-"));
  out.ago_s = doc["ago_s"] | -1;
  // Legacy pre-v3 fields (still parsed for compat / serial log).
  out.last = String((const char*)(doc["last"] | "-"));
  out.runs = doc["runs"] | 0;
  out.ok_n = doc["ok_n"] | 0;
  out.fail_n = doc["fail_n"] | 0;
  out.tok_today = doc["tok_today"] | 0;
  out.err = doc["err"] | 0;
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
    Serial.printf("ok %s %s %s %ld/%ld %s %lds\n", current.status.c_str(),
                  current.proj.c_str(), current.provider.c_str(),
                  current.succ, current.total,
                  current.persona.c_str(), current.ago_s);
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
