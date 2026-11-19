// The sound of the room.
//
// The board listens to the 2.4 GHz air around it and streams what it hears to a browser, which
// turns it into music. Each WiFi network nearby becomes a held note — its channel is the pitch,
// its signal the volume — so the networks around you sound as a slow chord. On top of that, the
// board counts the raw frames flying past on its own channel, and that live activity becomes a
// shimmer that ripples when a device transmits or someone walks between the board and a router.
//
// It only ever listens. The radio is put in promiscuous mode — the mode every WiFi analyser
// uses — to *count* frames; it never injects one, never deauthenticates anyone, never connects
// to a network it was not invited to. The one thing it transmits is its own open access point,
// devkit-air, so a phone or laptop can join it and hear the result. No internet, nothing
// attached.
//
// Board: ESP32 DevKit V1. Join the WiFi "devkit-air", open http://192.168.4.1, press play.

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include "ui.h"
#include "airstats.h"
#include "air_page.h"

const char *AP_NAME = "devkit-air";
const uint8_t CAMP_CHANNEL = 1;        // the board's own channel; the live shimmer is this channel
const int BLUE_LED = 2;

const uint32_t SCAN_EVERY = 5000;      // a full look across every channel, this often
const uint32_t WINDOW_MS = 250;        // the board drains its live tally this often

const int MAX_NETS = 40;
struct Net {
  char ssid[33];
  char bssid[18];
  uint8_t ch;
  int8_t rssi;
  bool secure;
  bool hidden;
};
Net nets[MAX_NETS];
int netCount = 0;

// Written by the promiscuous callback (WiFi task, core 0), drained by loop() (core 1). The
// callback only ever adds; loop() copies and clears. A frame or two may fall across the seam
// when it clears — harmless for a stream meant to be heard, not audited.
volatile AirTally live;
AirSummary summary;     // the drained, per-second view the page reads
uint32_t lastDrain = 0, lastScan = 0, drainStart = 0;
bool scanning = false;

WebServer web(80);
DNSServer dns;

// ── the ear ──────────────────────────────────────────────────────────────────

// Called for every frame the radio overhears on its current channel. It must be tiny and must
// not allocate, print, or call into the network stack — it runs inside the WiFi driver.
static void IRAM_ATTR sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  const wifi_pkt_rx_ctrl_t &c = p->rx_ctrl;
  int t = type == WIFI_PKT_MGMT ? AIR_MGMT : type == WIFI_PKT_CTRL ? AIR_CTRL
          : type == WIFI_PKT_DATA ? AIR_DATA : AIR_MISC;
  ((AirTally *)&live)->add(c.channel, c.rssi, c.sig_len, t);
}

static void listen(bool on) {
  esp_wifi_set_promiscuous(on);
  if (on) {
    esp_wifi_set_promiscuous_rx_cb(&sniff);
    esp_wifi_set_channel(CAMP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }
}

// ── the chord: a full look across every channel ──────────────────────────────
//
// A scan hops the radio across all thirteen channels and lists every network it finds. It
// cannot run while the radio is camped sniffing, so sniffing is switched off for the ~1.5 s the
// scan takes and switched back on after. The chord is refreshed; the shimmer pauses for a
// breath.

static void startScan() {
  listen(false);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);       // async, include hidden
  scanning = true;
}

static void readScan() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  if (n >= 0) {
    netCount = 0;
    for (int i = 0; i < n && netCount < MAX_NETS; i++) {
      String s = WiFi.SSID(i);
      if (s == AP_NAME) continue;      // do not sing the board's own beacon
      Net &net = nets[netCount];
      s.toCharArray(net.ssid, sizeof(net.ssid));
      WiFi.BSSIDstr(i).toCharArray(net.bssid, sizeof(net.bssid));
      net.ch = WiFi.channel(i);
      net.rssi = WiFi.RSSI(i);
      net.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      net.hidden = s.length() == 0;
      netCount++;
    }
    Serial.printf("air: %d networks\n", netCount);
  }
  WiFi.scanDelete();
  scanning = false;
  listen(true);                        // back to sniffing our own channel
  lastScan = millis();
}

// ── the page's data ──────────────────────────────────────────────────────────

static void handleAir() {
  String o;
  o.reserve(1600);
  o += "{\"camp\":" + String(CAMP_CHANNEL);
  o += ",\"up\":" + String(millis() / 1000);
  o += ",\"here\":" + String(WiFi.softAPgetStationNum());
  o += ",\"live\":{\"rate\":" + String(summary.rate, 1);
  o += ",\"kbps\":" + String(summary.kbps, 2);
  o += ",\"busiest\":" + String(summary.busiest, 1);
  o += ",\"mgmt\":" + String(summary.mgmt) + ",\"data\":" + String(summary.data) + ",\"ctrl\":" + String(summary.ctrl);
  o += ",\"per\":[";
  for (int c = 1; c <= 13; c++) { if (c > 1) o += ','; o += String(summary.perChan[c], 1); }
  o += "]}";
  o += ",\"nets\":[";
  for (int i = 0; i < netCount; i++) {
    const Net &n = nets[i];
    if (i) o += ',';
    o += "{\"s\":\"" + uiJson(n.hidden ? "(hidden)" : n.ssid) + "\",\"b\":\"" + String(n.bssid) +
         "\",\"c\":" + String(n.ch) + ",\"r\":" + String(n.rssi) + ",\"o\":" + String(n.secure ? 1 : 0) +
         ",\"h\":" + String(n.hidden ? 1 : 0) + "}";
  }
  o += "]}";
  web.send(200, "application/json", o);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BLUE_LED, OUTPUT);
  Serial.println("\nair: starting");

  WiFi.mode(WIFI_AP_STA);              // AP to serve the page, STA so scans can hop
  WiFi.softAP(AP_NAME, nullptr, CAMP_CHANNEL);
  WiFi.setSleep(false);               // never doze: a sleeping radio hears nothing
  dns.start(53, "*", WiFi.softAPIP());

  esp_wifi_set_promiscuous_filter(nullptr);   // every class of frame
  listen(true);

  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", AIR_BODY); });   // a full page, sent as-is
  web.on("/air", handleAir);
  web.onNotFound([] {                 // every captive-portal probe lands on the page
    web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    web.send(302, "text/plain", "");
  });
  web.begin();

  drainStart = lastDrain = lastScan = millis();
  Serial.printf("air: join \"%s\", open http://%s — listening on channel %u\n", AP_NAME,
                WiFi.softAPIP().toString().c_str(), CAMP_CHANNEL);
}

void loop() {
  dns.processNextRequest();
  web.handleClient();

  const uint32_t now = millis();

  // Drain the live tally into per-second rates.
  if (now - lastDrain >= WINDOW_MS) {
    AirTally snap = *(AirTally *)&live;
    ((AirTally *)&live)->clear();
    summarize(snap, now - drainStart, summary);
    drainStart = now;
    lastDrain = now;
    digitalWrite(BLUE_LED, summary.rate > 1 ? HIGH : LOW);   // a faint flicker with the traffic
  }

  // The periodic full scan, and reading it back when it finishes.
  if (scanning) readScan();
  else if (now - lastScan >= SCAN_EVERY) startScan();
}
