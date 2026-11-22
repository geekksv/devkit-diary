// Tambola (Housie) for a room full of phones, called and refereed by the board.
//
// The host starts a game and gets a four-letter code; everyone else joins with it and picks one
// to six tickets. The board makes the tickets (proper strips: every number once across six), draws
// the numbers with the chip's hardware random generator, calls one every few seconds, and checks
// every claim against what has actually been called, so nobody can cheat and nobody has to check
// by hand. Any screen, a laptop or a TV, can show the caller board: the number, the 1-90 grid,
// the winners, read out in English or Hindi.
//
// All the rules live in tambola.h, which is tested on a laptop. This file only connects it to
// WiFi and the web server.
//
// It runs on your home WiFi (tambola.local), or makes its own network devkit-tambola.
//
// Board: ESP32 DevKit V1, nothing attached.

#include <WiFi.h>
#include <WebServer.h>
#include <esp_random.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "tambola.h"
#include "tambola_page.h"

const char *HOST = "tambola";
const char *OWN_AP = "devkit-tambola";

WebServer web(80);
static char reply[8192];                 // the biggest reply (30 players, 6 tickets each) is ~3.3 KB

static uint32_t hwRand() { return esp_random(); }

// A request's arguments, for tambola.h. A few at once can be in use (route() reads the name and
// ticket count in one call), so they rotate through a small set of buffers.
static const char *arg(const char *key) {
  static String held[4];
  static int at = 0;
  at = (at + 1) % 4;
  held[at] = web.arg(key);
  return held[at].c_str();
}

static void handle() {
  Out o = {reply, sizeof reply, 0};
  route(web.uri().c_str(), arg, millis(), o);
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", reply);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\ntambola: starting");
  tRand = hwRand;
  memset(rooms, 0, sizeof rooms);

  const wifisetup::Config cfg = {HOST, OWN_AP, "devkit-tambola", "11", true};
  if (!wifisetup::begin(cfg, web)) {
    // On its own network, let in as many phones as the ESP32 allows (the default is four).
    WiFi.softAP(OWN_AP, nullptr, 1, 0, 10);
    Serial.println("tambola: own network allows up to 10 phones");
  }

  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", TAMBOLA_BODY); });
  for (const char *p : {"/rooms", "/create", "/join", "/state", "/tickets", "/claim", "/emoji", "/leave",
                        "/settings", "/start", "/next", "/pause", "/end", "/again"})
    web.on(p, handle);
  web.onNotFound([] {
    if (wifisetup::portal()) {             // phones joining the board's own network land on the game
      web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      web.send(302, "text/plain", "");
    } else web.send(404, "text/plain", "not here");
  });
  web.begin();
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last >= 50) {                // the calling clock
    last = now;
    tick(now);
  }
}
