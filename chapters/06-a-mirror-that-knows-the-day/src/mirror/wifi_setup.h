#pragma once
//
// Getting a board onto someone's WiFi without their password ever being in the code.
//
// The firmware on the flasher is public and everyone's network is different, so the name and
// password are handed to the board after it is flashed, and kept in its flash (NVS), which
// survives restarts and re-flashing. Two ways in:
//
//   1. Over the USB cable, from the flasher page, right after flashing — Improv Serial
//      (improv.h). The password goes browser → cable → board and nowhere else.
//   2. With no computer: a board that has no saved network, or cannot reach it, opens its own
//      network, <apName>, with a page to pick one. Join it from a phone; the page opens itself.
//
// Shared by every project that needs a real network; like ui.h, each sketch folder carries an
// identical copy.

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "improv.h"
#include "ui.h"

namespace wifisetup {

struct Config {
  const char *host;        // mDNS name: <host>.local
  const char *apName;      // the setup network
  const char *firmware;    // for Improv "device info"
  const char *version;
};

enum Mode { CONNECTING, ONLINE, PORTAL };

static Config cfg;
static Mode mode = CONNECTING;
static Preferences prefs;
static WebServer *web = nullptr;
static DNSServer dns;
static improv::Reader reader;
static uint32_t restartAt = 0;           // non-zero: restart the board at this millis()
static uint32_t lostAt = 0;              // when an ONLINE board lost its network
static char lastError[80] = "";

inline bool online() { return mode == ONLINE && WiFi.status() == WL_CONNECTED; }
inline bool portal() { return mode == PORTAL; }

inline String url() { return String("http://") + WiFi.localIP().toString() + "/"; }

inline void forget() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

static void save(const char *ssid, const char *pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.remove("err");
  prefs.end();
}

static void noteError(const char *e) {
  prefs.begin("wifi", false);
  prefs.putString("err", e);
  prefs.end();
}

// Joins a network and waits for an address. The board's own setup network, if it is up, stays
// up while it tries (AP + STA).
static bool join(const char *ssid, const char *pass, uint32_t timeoutMs) {
  WiFi.mode(mode == PORTAL ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(cfg.host);
  WiFi.begin(ssid, pass);
  const uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    const wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return true;
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) {
      if (millis() - t0 > 4000) break;   // give a slow AP a few seconds before believing it
    }
    delay(150);
  }
  snprintf(lastError, sizeof(lastError), "%s",
           WiFi.status() == WL_NO_SSID_AVAIL ? "the network was not found" :
           "it did not accept the password, or did not answer");
  WiFi.disconnect(false, false);
  return false;
}

// ── Improv over the serial line ──────────────────────────────────────────────

static void send(uint8_t type, const uint8_t *data, size_t len) {
  uint8_t pkt[10 + improv::MAX_DATA];
  const size_t n = improv::build(type, data, len, pkt);
  Serial.write(pkt, n);
  Serial.write('\n');                    // keeps the human-readable log on its own lines
}

static void sendState(uint8_t s) { send(improv::CURRENT_STATE, &s, 1); }
static void sendError(uint8_t e) { send(improv::ERROR_STATE, &e, 1); }

static void sendResult(uint8_t cmd, const char *const *strings, int count) {
  uint8_t data[improv::MAX_DATA];
  const size_t n = improv::buildResult(cmd, strings, count, data);
  send(improv::RPC_RESULT, data, n);
}

static void sendUrl(uint8_t cmd) {
  const String u = url();
  const char *s[] = {u.c_str()};
  sendResult(cmd, s, 1);
}

static void handleRpc(const uint8_t *d, size_t len) {
  if (len < 2) { sendError(improv::BAD_PACKET); return; }
  switch (d[0]) {
    case improv::GET_STATE:
      sendState(online() ? improv::PROVISIONED : improv::READY);
      if (online()) sendUrl(improv::GET_STATE);
      break;
    case improv::GET_INFO: {
      const char *s[] = {cfg.firmware, cfg.version, "ESP32", cfg.host};
      sendResult(improv::GET_INFO, s, 4);
      break;
    }
    case improv::GET_NETWORKS: {
      const int n = WiFi.scanNetworks();
      for (int i = 0; i < n && i < 20; i++) {
        const String ssid = WiFi.SSID(i);
        if (!ssid.length()) continue;
        const String rssi = String(WiFi.RSSI(i));
        const char *s[] = {ssid.c_str(), rssi.c_str(), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"};
        sendResult(improv::GET_NETWORKS, s, 3);
      }
      sendResult(improv::GET_NETWORKS, nullptr, 0);   // an empty result ends the list
      WiFi.scanDelete();
      break;
    }
    case improv::WIFI_SETTINGS: {
      char ssid[33], pass[65];
      if (!improv::parseWifi(d, len, ssid, sizeof(ssid), pass, sizeof(pass))) { sendError(improv::BAD_PACKET); return; }
      sendState(improv::PROVISIONING);
      Serial.printf("wifi: joining \"%s\"\n", ssid);
      if (join(ssid, pass, 20000)) {
        save(ssid, pass);
        sendState(improv::PROVISIONED);
        sendUrl(improv::WIFI_SETTINGS);
        Serial.printf("wifi: joined, %s — restarting into the project\n", url().c_str());
        restartAt = millis() + 1500;     // come back up cleanly, online, with the project running
      } else {
        Serial.printf("wifi: could not join — %s\n", lastError);
        sendError(improv::NO_CONNECT);
        sendState(improv::READY);
      }
      break;
    }
    default:
      sendError(improv::UNKNOWN_RPC);
  }
}

static void readSerial() {
  while (Serial.available()) {
    if (reader.feed((uint8_t)Serial.read())) {
      if (reader.type == improv::RPC) handleRpc(reader.data, reader.len);
    }
  }
}

// ── the setup network, for a phone ───────────────────────────────────────────

static const char PORTAL_BODY[] PROGMEM = R"HTML(
<style>
.net{display:flex;align-items:center;gap:10px;width:100%;text-align:left;margin:0 0 8px;
  background:var(--card2);color:var(--ink);border:1px solid var(--line);font-weight:600}
.net span{flex:1}.net small{color:var(--mute);font-weight:500}
.err{color:var(--red);font-size:13px;min-height:18px}
</style>
<header><span class="dot"></span><div><h1>Connect to your WiFi</h1>
<p class="sub">so the board can reach the internet</p></div></header>
<div class="card"><p class="label">networks the board can see</p><div id="nets"><p class="sub">looking…</p></div>
<button class="ghost" onclick="scan()" style="margin-top:6px">look again</button></div>
<div class="card"><form id="f" autocomplete="off">
<p class="label">network</p><input id="s" maxlength="32" required>
<p class="label" style="margin-top:12px">password</p><input id="p" type="password" maxlength="64">
<button style="margin-top:14px">save and connect</button><p class="err" id="e"></p></form></div>
<div class="card" id="done" style="display:none"><p class="label">saved</p>
<p>The board is restarting to join <b id="dn"></b>. Put your phone back on that network, then open
<b>http://%HOST%.local</b> in a minute.</p>
<p class="sub">If it cannot join, this setup network appears again and says why.</p></div>
<script>
const $=i=>document.getElementById(i);
async function scan(){$('nets').innerHTML='<p class="sub">looking…</p>';
 try{const l=await(await fetch('/setup/scan')).json();$('nets').innerHTML='';
  if(l.err)$('e').textContent='Last try: could not join — '+l.err+'.';
  for(const n of l.nets){const b=document.createElement('button');b.type='button';b.className='net';
   const a=document.createElement('span');a.textContent=n.s;const r=document.createElement('small');
   r.textContent=(n.r>-60?'strong':n.r>-72?'good':'weak')+(n.o?' · open':'');b.append(a,r);
   b.onclick=()=>{$('s').value=n.s;$('p').focus()};$('nets').appendChild(b)}
  if(!l.nets.length)$('nets').innerHTML='<p class="sub">none found — type the name below</p>'}
 catch(e){$('nets').innerHTML='<p class="sub">could not scan</p>'}}
$('f').onsubmit=async e=>{e.preventDefault();$('e').textContent='';
 const b='s='+encodeURIComponent($('s').value)+'&p='+encodeURIComponent($('p').value);
 try{const d=await(await fetch('/setup/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})).json();
  if(d.ok){$('f').parentNode.style.display='none';$('dn').textContent=$('s').value;$('done').style.display='block'}}
 catch(x){$('e').textContent='The board did not answer.'}};
scan();
</script>
)HTML";

static void portalPage() {
  String body = FPSTR(PORTAL_BODY);
  body.replace("%HOST%", cfg.host);
  uiSend(*web, "WiFi setup", body.c_str());
}

static void startPortal() {
  mode = PORTAL;
  WiFi.mode(WIFI_AP_STA);                // STA too, so scans and a test join work
  WiFi.softAP(cfg.apName);
  dns.start(53, "*", WiFi.softAPIP());
  web->on("/setup/scan", [] {
    const int n = WiFi.scanNetworks();
    String o = "{\"err\":\"" + uiJson(lastError) + "\",\"nets\":[";
    int shown = 0;
    for (int i = 0; i < n && shown < 20; i++) {
      if (!WiFi.SSID(i).length()) continue;
      if (shown++) o += ',';
      o += "{\"s\":\"" + uiJson(WiFi.SSID(i).c_str()) + "\",\"r\":" + String(WiFi.RSSI(i)) +
           ",\"o\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "1" : "0") + "}";
    }
    o += "]}";
    WiFi.scanDelete();
    web->send(200, "application/json", o);
  });
  web->on("/setup/save", HTTP_POST, [] {
    const String s = web->arg("s"), p = web->arg("p");
    if (!s.length() || s.length() > 32 || p.length() > 64) {
      web->send(200, "application/json", "{\"ok\":false}");
      return;
    }
    // Saved first, tested after the restart. Testing it now would move the board's radio to
    // the home network's channel, and the phone showing this page would drop off mid-answer.
    save(s.c_str(), p.c_str());
    web->send(200, "application/json", "{\"ok\":true}");
    restartAt = millis() + 2500;
  });
  web->on("/", portalPage);
  web->onNotFound([] {                   // every portal probe lands on the setup page
    if (web->hostHeader() != WiFi.softAPIP().toString()) {
      web->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      web->send(302, "text/plain", "");
    } else portalPage();
  });
  web->begin();
  Serial.printf("wifi: setup network \"%s\" is up — join it and open http://%s\n", cfg.apName,
                WiFi.softAPIP().toString().c_str());
}

// ── the outside ──────────────────────────────────────────────────────────────

// Joins the saved network, or opens the setup network. Returns true when online; then the
// sketch registers its own pages on `server` and calls server.begin().
inline bool begin(const Config &c, WebServer &server) {
  cfg = c;
  web = &server;
  WiFi.persistent(false);                // NVS is ours; do not let the WiFi driver keep a copy too
  prefs.begin("wifi", true);
  const String ssid = prefs.getString("ssid", ""), pass = prefs.getString("pass", "");
  prefs.getString("err", "").toCharArray(lastError, sizeof(lastError));
  prefs.end();

  if (ssid.length()) {
    Serial.printf("wifi: joining \"%s\"\n", ssid.c_str());
    if (join(ssid.c_str(), pass.c_str(), 20000)) {
      mode = ONLINE;
      WiFi.setAutoReconnect(true);
      if (MDNS.begin(cfg.host)) MDNS.addService("http", "tcp", 80);
      Serial.printf("wifi: online — open http://%s.local or %s\n", cfg.host, url().c_str());
      noteError("");
      return true;
    }
    Serial.printf("wifi: could not join \"%s\" — %s\n", ssid.c_str(), lastError);
    noteError(lastError);
  } else {
    Serial.println("wifi: no network saved yet");
    lastError[0] = 0;
  }
  startPortal();
  return false;
}

// Call from loop(): Improv on the serial line, the setup network, reconnects, restarts.
inline void loop() {
  readSerial();
  if (mode == PORTAL) {
    dns.processNextRequest();
    web->handleClient();
  } else if (mode == ONLINE) {
    if (WiFi.status() == WL_CONNECTED) lostAt = 0;
    else if (!lostAt) { lostAt = millis(); Serial.println("wifi: lost the network, reconnecting"); }
    else if (millis() - lostAt > 60000) { WiFi.reconnect(); lostAt = millis(); }
  }
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
}

}  // namespace wifisetup
