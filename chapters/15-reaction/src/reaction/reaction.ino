// A reaction game with the board's own LED and BOOT button.
//
// Press BOOT (or Start on the page). After a random wait the blue LED lights: press BOOT as fast as
// you can. The page at http://react.local shows your time, your last five, and the top ten, which
// the board keeps through restarts.
//
// All the rules live in reaction.h, which is tested on a laptop. This file only connects it to the
// LED, the button, WiFi and the web server.
//
// Board: ESP32 DevKit V1, nothing attached. Default partition scheme.

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "reaction.h"
#include "reaction_page.h"

const char *HOST = "react";
const int LED = 2;                       // the blue LED on a DevKit V1
const int BOOT_BUTTON = 0;               // BOOT: reads LOW while pressed

WebServer web(80);
static char reply[2048];
static Preferences prefs;

static uint32_t hwRand() { return esp_random(); }

static const char *arg(const char *key) {
  static String held[3];
  static int at = 0;
  at = (at + 1) % 3;
  held[at] = web.arg(key);
  return held[at].c_str();
}

static void handle() {
  Out o = {reply, sizeof reply, 0};
  route(web.uri().c_str(), arg, millis(), o);
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", reply);
}

static void loadScores() {
  prefs.begin("react", true);
  if (prefs.getBytesLength("top") == sizeof g.top) {
    prefs.getBytes("top", g.top, sizeof g.top);
    g.nTop = prefs.getUChar("n", 0);
    if (g.nTop > TOP) g.nTop = 0;
  }
  prefs.end();
}

static void saveScores() {
  prefs.begin("react", false);
  prefs.putBytes("top", g.top, sizeof g.top);
  prefs.putUChar("n", g.nTop);
  prefs.end();
  g.scoresChanged = false;
}

// The button, read on loop(): a press counts once it has held steady for 15 ms. The time of the
// press is when it first went down, so the steadying does not add to anyone's time.
static void readButton(uint32_t now) {
  static bool down = false;
  static uint32_t firstLow = 0;
  const bool low = digitalRead(BOOT_BUTTON) == LOW;
  if (low && !down) {
    if (!firstLow) firstLow = now;
    else if (now - firstLow >= 15) {
      down = true;
      press(firstLow);
    }
  } else if (!low) {
    down = false;
    firstLow = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nreact: starting");
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  rRand = hwRand;
  gameInit();
  loadScores();

  // Without WiFi the game still works on the board alone: the LED and BOOT need no page.
  const wifisetup::Config cfg = {HOST, "devkit-react", "devkit-reaction", "15", true};
  wifisetup::begin(cfg, web);

  web.on("/", [] {
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html; charset=utf-8", REACTION_PAGE);
  });
  for (const char *p : {"/state", "/start", "/name", "/clear"}) web.on(p, handle);
  web.on("/favicon.ico", [] { web.send(204); });
  web.onNotFound([] {
    if (wifisetup::portal()) {           // a phone on the board's own network lands on the game
      web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      web.send(302, "text/plain", "");
    } else web.send(404, "text/plain", "not here");
  });
  web.begin();
  Serial.printf("react: ready, %u scores kept\n", g.nTop);
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  const uint32_t now = millis();
  readButton(now);
  tick(now);
  digitalWrite(LED, ledOn(now) ? HIGH : LOW);
  if (g.scoresChanged) saveScores();
  static uint32_t lastPhase = 255;
  if (g.phase != lastPhase) {
    lastPhase = g.phase;
    if (g.phase == DONE) Serial.printf("react: %s %u ms%s\n", g.player, g.lastMs, g.rank == 0 ? " (best)" : "");
    else if (g.phase == EARLY) Serial.println("react: false start");
    else if (g.phase == MISSED) Serial.println("react: missed");
  }
  delay(1);
}
