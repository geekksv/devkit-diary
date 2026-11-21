// Who's on my WiFi.
//
// A network scanner in the spirit of Angry IP Scanner and Advanced IP Scanner, living on the board:
// it asks every address on your network who is there (ARP, so even phones and PCs that ignore
// ping show up), then learns what it can about each — who made it, what it calls itself, how fast
// it answers, what it is — and keeps watching. A phone opens http://scan.local to see the list,
// name things, check a device's open ports, wake a sleeping PC, watch a latency graph, export the
// list, and see anything new that joins.
//
// And one extra: computers that run the small inbox helper (one line to install, served by the
// board) can be sent a message from the list, and reply.
//
// It needs to be on your home WiFi to see it; its own network devkit-scan is only for setting
// that up.
//
// Board: ESP32 DevKit V1, nothing attached.

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "net.h"
#include "probe.h"
#include "page.h"
#include "files.h"

const char *HOST = "scan";
const char *OWN_AP = "devkit-scan";
const uint32_t INTERNET = 0x01010101;   // 1.1.1.1, for "is the internet slow, or my WiFi?"

WebServer web(80);
Preferences prefs;
SemaphoreHandle_t lock;                 // the device table is shared by the web (loop) and the scanner task
portMUX_TYPE ringMux = portMUX_INITIALIZER_UNLOCKED;
Net net;
Ring ring;
uint32_t watchIp = 0;                   // what the latency graph pings; 0 = the router
uint32_t tableVer = 1;                  // bumps whenever the device list changes, so the phone refetches it

// The scanner task's state, shown on the phone.
volatile bool scanning = false, scanAsked = true;
volatile int scanPct = 0;
volatile uint32_t lastScanMs = 0, portsAsk = 0, portsBusy = 0;
int everyMin = 5;                       // a full scan this often; a quick ARP-only pass in between
bool alertComputers = true;             // tell the inbox helpers when something new joins
bool dirty = false;

// ── time: the network's (NTP), or the phone's until then ─────────────────────────────────────

uint32_t phoneEpoch = 0, phoneAt = 0;
bool timeFixed = false;

static uint32_t epochNow() {
  const time_t t = time(nullptr);
  if (t > 1700000000) return (uint32_t)t;
  if (phoneEpoch) return phoneEpoch + (millis() - phoneAt) / 1000;
  return millis() / 1000;                 // until we know: seconds since boot, corrected below
}

// Devices first seen before the real time was known carry "seconds since boot"; move them on.
static void fixTimes() {
  if (timeFixed) return;
  const time_t t = time(nullptr);
  if (t <= 1700000000 && !phoneEpoch) return;
  const uint32_t real = epochNow(), boot = millis() / 1000, delta = real - boot;
  for (int i = 0; i < net.n; i++) {
    Device &d = net.dev[i];
    if (d.first < 1000000000) d.first += delta;
    if (d.last < 1000000000) d.last += delta;
    if (d.portsAt && d.portsAt < 1000000000) d.portsAt += delta;
  }
  timeFixed = true;
}

// ── keeping what it learned ──────────────────────────────────────────────────────────────────

const uint32_t FILE_MAGIC = 0x53434E31;  // "SCN1"

static void loadDevices() {
  File f = LittleFS.open("/devices.bin", "r");
  if (!f) return;
  uint32_t magic = 0, count = 0, size = 0;
  f.read((uint8_t *)&magic, 4);
  f.read((uint8_t *)&count, 4);
  f.read((uint8_t *)&size, 4);
  if (magic == FILE_MAGIC && size == sizeof(Device) && count <= MAX_DEV) {
    net.n = f.read((uint8_t *)net.dev, count * sizeof(Device)) / sizeof(Device);
    for (int i = 0; i < net.n; i++) { net.dev[i].online = 0; net.dev[i].rtt = -1; net.dev[i].helperMs = 0; }
    net.baselined = net.n > 0;
  }
  f.close();
  Serial.printf("scan: remembered %d devices\n", net.n);
}

static void saveDevices() {
  File f = LittleFS.open("/devices.tmp", "w");
  if (!f) return;
  const uint32_t head[3] = {FILE_MAGIC, (uint32_t)net.n, sizeof(Device)};
  f.write((const uint8_t *)head, sizeof head);
  f.write((const uint8_t *)net.dev, net.n * sizeof(Device));
  f.close();
  LittleFS.rename("/devices.tmp", "/devices.bin");
  dirty = false;
}

static void loadSettings() {
  prefs.begin("scan", true);
  everyMin = prefs.getInt("every", 5);
  alertComputers = prefs.getBool("alert", true);
  watchIp = prefs.getUInt("watch", 0);
  prefs.end();
}
static void saveSettings() {
  prefs.begin("scan", false);
  prefs.putInt("every", everyMin);
  prefs.putBool("alert", alertComputers);
  prefs.putUInt("watch", watchIp);
  prefs.end();
}

// ── the network we're on ─────────────────────────────────────────────────────────────────────

static uint32_t u32(const IPAddress &a) { return (uint32_t)a[0] << 24 | a[1] << 16 | a[2] << 8 | a[3]; }
static uint32_t myIp() { return u32(WiFi.localIP()); }
static uint32_t gatewayIp() { return u32(WiFi.gatewayIP()); }
static uint32_t maskBits() { return u32(WiFi.subnetMask()); }
// The addresses swept: the whole subnet, or for anything bigger than a /24, the /24 we're in.
static void sweepRange(uint32_t &first, uint32_t &last) {
  uint32_t mask = maskBits();
  if (mask < 0xFFFFFF00) mask = 0xFFFFFF00;
  first = (myIp() & mask) + 1;
  last = (myIp() | ~mask) - 1;
}
static uint32_t broadcastIp() { return myIp() | ~maskBits(); }

// ── messages ─────────────────────────────────────────────────────────────────────────────────

const int MAX_HELPERS = 16, MAX_MSGS = 30;
Helper helpers[MAX_HELPERS];
Msg msgs[MAX_MSGS];
int msgHead = 0, msgCount = 0;
uint16_t msgSeq = 0;

static Helper *helperAt(uint32_t ip) {
  for (auto &h : helpers) if (h.ip == ip) return &h;
  return nullptr;
}
static bool helperOnline(const Helper &h) { return h.ip && millis() - h.lastMs < 15000; }

static Msg &newMsg() {
  Msg &m = msgs[msgHead];
  msgHead = (msgHead + 1) % MAX_MSGS;
  if (msgCount < MAX_MSGS) msgCount++;
  memset(&m, 0, sizeof m);
  m.id = ++msgSeq;
  m.t = epochNow();
  return m;
}
static void queueMsg(uint32_t ip, const char *who, const char *text) {
  Msg &m = newMsg();
  m.ip = ip;
  m.out = 1;
  strncpy(m.who, who, sizeof m.who - 1);
  strncpy(m.text, text, sizeof m.text - 1);
}

// ── the scanner task ─────────────────────────────────────────────────────────────────────────

static void lockTable() { xSemaphoreTake(lock, portMAX_DELAY); }
static void unlockTable() { xSemaphoreGive(lock); }

static void onPing(int ms) {
  portENTER_CRITICAL(&ringMux);
  ring.add(ms);
  portEXIT_CRITICAL(&ringMux);
}
static void startWatch() {
  portENTER_CRITICAL(&ringMux);
  ring.clear();
  portEXIT_CRITICAL(&ringMux);
  probe::watch(watchIp ? watchIp : gatewayIp(), onPing);
}

static void guessAll() {
  const uint32_t me = myIp(), gw = gatewayIp();
  for (int i = 0; i < net.n; i++) {
    Device &d = net.dev[i];
    if (!d.kindSet) d.kind = guessKind(d, d.ip == gw, d.ip == me && d.online);
  }
}

// Checks the ports of one device, if the phone asked. Called between the scan's steps too, so a
// tap on "Check ports" never waits for a whole scan.
static void servePorts() {
  const uint32_t ip = portsAsk;
  if (!ip) return;
  portsAsk = 0;
  portsBusy = ip;
  const uint32_t open = probe::ports(ip);
  static char page[3072];
  char title[40] = "";
  for (uint16_t p : {80, 8080}) {
    if (!(open >> portIndex(p) & 1)) continue;
    char u[40], ips[16];
    ipStr(ip, ips);
    snprintf(u, sizeof u, "http://%s:%u/", ips, p);
    if (probe::fetch(u, page, sizeof page) && htmlTitle(page, title, sizeof title)) break;
  }
  lockTable();
  if (Device *d = byIp(net, ip)) {
    d->ports = open;
    d->portsAt = epochNow();
    if (title[0]) strncpy(d->title, title, sizeof d->title - 1);
    if (!d->kindSet) d->kind = guessKind(*d, ip == gatewayIp(), ip == myIp());
    dirty = true;
    tableVer++;
  }
  unlockTable();
  portsBusy = 0;
}

static void announceNew(const Device &d) {
  char ips[16], mac[18], line[200];
  ipStr(d.ip, ips);
  macStr(d.mac, mac);
  const char *maker = d.maker == 0xff ? (macRandom(d.mac) ? "a private address" : "an unknown maker") : makerName(d.maker);
  snprintf(line, sizeof line, "Something new joined your WiFi: %s (%s, %s).", d.name[0] ? d.name : ips, maker, mac);
  Serial.printf("scan: new device %s %s\n", ips, mac);
  if (!alertComputers) return;
  for (auto &h : helpers) if (helperOnline(h)) queueMsg(h.ip, "Scanner", line);
}

// One scan. full: also ping everything, ask names, and ask UPnP; otherwise just who's there.
static void runScan(bool full) {
  scanning = true;
  scanPct = 0;
  uint32_t first, last;
  sweepRange(first, last);
  const uint32_t me = myIp();
  static uint32_t answered[256];
  int nAns = 0;

  // The board itself never appears in its own ARP table.
  uint8_t myMac[6];
  WiFi.macAddress(myMac);
  lockTable();
  if (Device *d = seen(net, myMac, me, epochNow())) {
    setName(*d, HOST, N_HELPER);
    d->rtt = 0;
    d->isNew = 0;
  }
  unlockTable();
  answered[nAns++] = me;

  // 1. ARP, eight addresses at a time
  const int total = last - first + 1;
  uint32_t batch[8];
  int done = 0;
  for (uint32_t ip = first; ip <= last;) {
    int n = 0;
    while (n < 8 && ip <= last) { if (ip != me) batch[n++] = ip; ip++; }
    probe::ArpSeen got;
    probe::arp(batch, n, 350, got);
    lockTable();
    for (int i = 0; i < got.n; i++) {
      bool fresh = false;
      Device *d = seen(net, got.mac[i], got.ip[i], epochNow(), &fresh);
      if (!d) continue;
      bool dup = false;
      for (int j = 0; j < nAns && !dup; j++) dup = answered[j] == got.ip[i];
      if (!dup && nAns < 256) answered[nAns++] = got.ip[i];
      if (fresh) { tableVer++; if (d->isNew) announceNew(*d); }
    }
    unlockTable();
    done += n;
    scanPct = done * (full ? 55 : 100) / total;
    servePorts();
    if (!wifisetup::online()) { scanning = false; return; }
  }
  lockTable();
  endSweep(net, answered, nAns);
  tableVer++;
  unlockTable();

  if (full) {
    // 2. ping each one that's here
    static uint32_t here[MAX_DEV];
    int nh = 0;
    lockTable();
    for (int i = 0; i < net.n; i++) if (net.dev[i].online && net.dev[i].ip != me) here[nh++] = net.dev[i].ip;
    unlockTable();
    for (int i = 0; i < nh; i++) {
      const int ms = probe::pingOnce(here[i]);
      lockTable();
      if (Device *d = byIp(net, here[i])) d->rtt = ms;
      unlockTable();
      scanPct = 55 + 20 * (i + 1) / (nh ? nh : 1);
      if (i % 6 == 5) servePorts();
    }
    tableVer++;

    // 3. names, for anything without a good one
    static uint32_t ask[MAX_DEV];
    int na = 0;
    lockTable();
    for (int i = 0; i < net.n; i++)
      if (net.dev[i].online && net.dev[i].ip != me && net.dev[i].nameSrc < N_BONJOUR) ask[na++] = net.dev[i].ip;
    unlockTable();
    const uint32_t dns = u32(WiFi.dnsIP());
    for (int at = 0; at < na; at += 8) {
      probe::Named got[24];
      const int g = probe::names(ask + at, na - at < 8 ? na - at : 8, dns, got, 24);
      lockTable();
      for (int i = 0; i < g; i++) if (Device *d = byIp(net, got[i].ip)) setName(*d, got[i].name, got[i].src);
      unlockTable();
      scanPct = 75 + 10 * (at + 8 > na ? na : at + 8) / (na ? na : 1);
      servePorts();
    }
    tableVer++;

    // 3b. Bonjour: printers, Macs, TVs and speakers announce what they are
    static Found found[32];
    const int nf = probe::browse(found, 32);
    lockTable();
    for (int i = 0; i < nf; i++) if (Device *d = byIp(net, found[i].ip)) applyFound(*d, found[i]);
    tableVer++;
    unlockTable();
    servePorts();

    // 4. UPnP descriptions, for whatever answers and hasn't described itself yet
    static probe::Upnp up[24];
    const int nu = probe::ssdp(up, 24);
    static char xml[4096];
    for (int i = 0; i < nu; i++) {
      lockTable();
      Device *d = byIp(net, up[i].ip);
      const bool want = d && !d->model[0];
      unlockTable();
      if (!want || !probe::fetch(up[i].location, xml, sizeof xml)) continue;
      char fr[40] = "", mk[40] = "", md[40] = "", desc[40];
      xmlTag(xml, "friendlyName", fr, sizeof fr);
      xmlTag(xml, "manufacturer", mk, sizeof mk);
      xmlTag(xml, "modelName", md, sizeof md);
      describeModel(mk, fr, md, desc, sizeof desc);
      lockTable();
      if ((d = byIp(net, up[i].ip)) && desc[0]) {
        strncpy(d->model, desc, sizeof d->model - 1);
        if (fr[0]) setName(*d, fr, N_UPNP);
      }
      unlockTable();
      scanPct = 85 + 15 * (i + 1) / nu;
      servePorts();
    }
  }
  lockTable();
  for (auto &h : helpers)                 // a computer's inbox helper knows its own name best
    if (h.ip && h.name[0]) if (Device *d = byIp(net, h.ip)) setName(*d, h.name, N_HELPER);
  guessAll();
  dirty = true;
  tableVer++;
  unlockTable();
  lastScanMs = millis();
  scanPct = 100;
  scanning = false;
  Serial.printf("scan: %s scan done, %d devices here\n", full ? "full" : "quick", nAns);
}

static void scannerTask(void *) {
  uint32_t lastFull = 0;
  uint32_t watchingGw = 0;
  for (;;) {
    if (!wifisetup::online()) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    if (!watchingGw || (!watchIp && watchingGw != gatewayIp())) { startWatch(); watchingGw = gatewayIp(); }
    servePorts();
    const uint32_t now = millis();
    const bool fullDue = !lastFull || now - lastFull >= (uint32_t)everyMin * 60000;
    const bool quickDue = now - lastScanMs >= 60000;
    if (scanAsked || fullDue) {
      scanAsked = false;
      runScan(true);
      lastFull = millis();
    } else if (quickDue) {
      runScan(false);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ── the phone's side ─────────────────────────────────────────────────────────────────────────

static String js(const char *s) { return uiJson(s); }

static void sendState() {
  if (web.hasArg("t") && !phoneEpoch) {
    const uint32_t t = strtoul(web.arg("t").c_str(), nullptr, 10);
    if (t > 1700000000) { phoneEpoch = t; phoneAt = millis(); }
  }
  const uint32_t want = strtoul(web.arg("v").c_str(), nullptr, 10);
  char a[16], b[16];
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "application/json", "");
  String o;
  o.reserve(2048);
  ipStr(myIp(), a);
  ipStr(gatewayIp(), b);
  o = "{\"ssid\":\"" + js(WiFi.SSID().c_str()) + "\",\"me\":\"" + a + "\",\"gw\":\"" + b + "\",\"now\":" +
      String(epochNow()) + ",\"rssi\":" + String(WiFi.RSSI());
  uint32_t first, last;
  sweepRange(first, last);
  ipStr(first, a);
  ipStr(last, b);
  o += ",\"range\":\"" + String(a) + " – " + b + "\"";
  o += ",\"scan\":{\"busy\":" + String(scanning ? 1 : 0) + ",\"pct\":" + String(scanPct) + ",\"ago\":" +
       String(lastScanMs ? (millis() - lastScanMs) / 1000 : -1) + ",\"ports\":\"";
  if (portsAsk || portsBusy) { ipStr(portsAsk ? portsAsk : portsBusy, a); o += a; }
  o += "\"},\"every\":" + String(everyMin) + ",\"alert\":" + String(alertComputers ? 1 : 0);

  // the latency graph
  ipStr(watchIp ? watchIp : gatewayIp(), a);
  o += ",\"lat\":{\"ip\":\"" + String(a) + "\",\"what\":\"" +
       (watchIp == 0 ? "router" : watchIp == INTERNET ? "internet" : "device") + "\",\"s\":[";
  static int16_t snap[RING];
  int count;
  Stats st, minute;
  portENTER_CRITICAL(&ringMux);
  count = ring.count;
  for (int i = 0; i < count; i++) snap[i] = ring.at(i);
  st = stats(ring, RING);
  minute = stats(ring, 60);
  portEXIT_CRITICAL(&ringMux);
  for (int i = count > 120 ? count - 120 : 0; i < count; i++) { o += snap[i]; if (i < count - 1) o += ','; }
  auto stat = [](const Stats &s) {
    return "{\"min\":" + String(s.min) + ",\"avg\":" + String(s.avg) + ",\"max\":" + String(s.max) +
           ",\"jit\":" + String(s.jitter) + ",\"loss\":" + String(s.lossPct) + ",\"n\":" + String(s.count) + "}";
  };
  o += "],\"m1\":" + stat(minute) + ",\"m5\":" + stat(st) + "}";

  // computers that can take a message, and the conversation
  o += ",\"helpers\":[";
  bool firstH = true;
  for (auto &h : helpers) {
    if (!h.ip) continue;
    ipStr(h.ip, a);
    o += String(firstH ? "" : ",") + "{\"ip\":\"" + a + "\",\"n\":\"" + js(h.name) + "\",\"os\":\"" + js(h.os) +
         "\",\"on\":" + (helperOnline(h) ? "1" : "0") + "}";
    firstH = false;
  }
  o += "],\"msgs\":[";
  for (int i = 0; i < msgCount; i++) {
    const Msg &m = msgs[(msgHead - msgCount + i + MAX_MSGS) % MAX_MSGS];
    ipStr(m.ip, a);
    o += String(i ? "," : "") + "{\"id\":" + m.id + ",\"ip\":\"" + a + "\",\"out\":" + m.out + ",\"st\":" + m.state +
         ",\"t\":" + m.t + ",\"who\":\"" + js(m.who) + "\",\"x\":\"" + js(m.text) + "\"}";
  }
  o += "]";
  web.sendContent(o);

  // the devices, only when they changed since the phone's copy
  lockTable();
  const uint32_t ver = tableVer;
  if (want != ver) {
    o = ",\"v\":" + String(ver) + ",\"devs\":[";
    char mac[18];
    for (int i = 0; i < net.n; i++) {
      const Device &d = net.dev[i];
      ipStr(d.ip, a);
      macStr(d.mac, mac);
      const Helper *h = helperAt(d.ip);
      o += String(i ? "," : "") + "{\"m\":\"" + mac + "\",\"ip\":\"" + a + "\",\"n\":\"" + js(d.name) + "\",\"src\":\"" +
           NAME_SRC[d.nameSrc] + "\",\"nk\":\"" + js(d.nick) + "\",\"md\":\"" + js(d.model) + "\",\"ti\":\"" +
           js(d.title) + "\",\"mk\":\"" + (d.maker == 0xff ? "" : makerName(d.maker)) + "\",\"rnd\":" +
           (macRandom(d.mac) ? 1 : 0) + ",\"k\":\"" + KIND_NAMES[d.kind < K_KINDS ? d.kind : 0] + "\",\"ks\":" +
           d.kindSet + ",\"nw\":" + d.isNew + ",\"on\":" + d.online + ",\"r\":" + d.rtt + ",\"p\":" + d.ports +
           ",\"sv\":" + d.svcs + ",\"pa\":" + d.portsAt + ",\"f\":" + d.first + ",\"l\":" + d.last + ",\"h\":" +
           (h ? (helperOnline(*h) ? 2 : 1) : 0) + "}";
      if (o.length() > 1500) { web.sendContent(o); o = ""; }
    }
    o += "]";
  }
  unlockTable();
  o += ",\"ports\":[";
  for (int i = 0; i < PORT_COUNT; i++) o += String(i ? "," : "") + "[" + PORTS[i].port + ",\"" + PORTS[i].label + "\"]";
  o += "],\"svcs\":[";
  for (int i = 0; i < SERVICE_COUNT; i++) o += String(i ? ",\"" : "\"") + SERVICE_LABELS[i] + "\"";
  o += "]}";
  web.sendContent(o);
  web.sendContent("");
}

static void ok(bool good = true) { web.send(200, "application/json", good ? "{\"ok\":true}" : "{\"ok\":false}"); }

static bool argMac(uint8_t *mac) { return macParse(web.arg("m").c_str(), mac); }

static void handleName() {
  uint8_t mac[6];
  if (!argMac(mac)) return ok(false);
  lockTable();
  Device *d = byMac(net, mac);
  if (d) {
    if (web.hasArg("n")) {
      String n = web.arg("n");
      n.trim();
      strncpy(d->nick, n.c_str(), sizeof d->nick - 1);
      d->nick[sizeof d->nick - 1] = 0;
    }
    if (web.hasArg("k")) {
      const String k = web.arg("k");
      d->kindSet = 0;
      for (int i = 0; i < K_KINDS; i++) if (k == KIND_NAMES[i]) { d->kind = i; d->kindSet = 1; }
      if (!d->kindSet) d->kind = guessKind(*d, d->ip == gatewayIp(), d->ip == myIp());
    }
    d->isNew = 0;
    tableVer++;
    dirty = true;
  }
  unlockTable();
  ok(d != nullptr);
}

static void handleSeen() {
  uint8_t mac[6];
  const bool all = web.arg("m") == "all";
  if (!all && !argMac(mac)) return ok(false);
  lockTable();
  for (int i = 0; i < net.n; i++) if (all || !memcmp(net.dev[i].mac, mac, 6)) net.dev[i].isNew = 0;
  tableVer++;
  dirty = true;
  unlockTable();
  ok();
}

static void handleForget() {
  uint8_t mac[6];
  if (!argMac(mac)) return ok(false);
  lockTable();
  for (int i = 0; i < net.n; i++)
    if (!memcmp(net.dev[i].mac, mac, 6)) { net.dev[i] = net.dev[--net.n]; break; }
  tableVer++;
  dirty = true;
  unlockTable();
  ok();
}

static void handleWake() {
  uint8_t mac[6];
  if (!argMac(mac)) return ok(false);
  const bool sent = probe::wake(mac, broadcastIp());
  char m[18];
  macStr(mac, m);
  Serial.printf("scan: wake-on-LAN to %s %s\n", m, sent ? "sent" : "failed");
  ok(sent);
}

static void handlePorts() {
  const uint32_t ip = ipParse(web.arg("ip").c_str());
  if (!ip) return ok(false);
  portsAsk = ip;
  ok();
}

static void handleWatch() {
  const String w = web.arg("w");
  watchIp = w == "router" ? 0 : w == "internet" ? INTERNET : ipParse(w.c_str());
  saveSettings();
  if (wifisetup::online()) startWatch();
  ok();
}

static void handleSettings() {
  if (web.hasArg("every")) everyMin = constrain(web.arg("every").toInt(), 1, 60);
  if (web.hasArg("alert")) alertComputers = web.arg("alert") == "1";
  saveSettings();
  ok();
}

static void handleSend() {
  String text = web.arg("t");
  text.trim();
  if (!text.length() || text.length() > 190) return ok(false);
  const String to = web.arg("to");
  int sent = 0;
  for (auto &h : helpers) {
    if (!h.ip) continue;
    char a[16];
    ipStr(h.ip, a);
    if (to == "all" ? helperOnline(h) : to == a) { queueMsg(h.ip, "Phone", text.c_str()); sent++; }
  }
  ok(sent > 0);
}

// The inbox helper on a computer checks in here every couple of seconds and collects its messages.
static void handleInbox() {
  const uint32_t ip = u32(web.client().remoteIP());
  Helper *h = helperAt(ip);
  if (!h) {
    h = &helpers[0];                       // a free slot, or else the one heard from longest ago
    for (auto &x : helpers) {
      if (!x.ip) { h = &x; break; }
      if (x.lastMs < h->lastMs) h = &x;
    }
    memset(h, 0, sizeof *h);
    h->ip = ip;
    tableVer++;
  }
  const bool wasOn = helperOnline(*h);
  h->lastMs = millis();
  strncpy(h->name, web.arg("n").c_str(), sizeof h->name - 1);
  strncpy(h->os, web.arg("o").c_str(), sizeof h->os - 1);
  if (!wasOn) {
    lockTable();
    if (Device *d = byIp(net, ip)) setName(*d, h->name, N_HELPER);
    tableVer++;
    unlockTable();
  }
  String o = "{\"m\":[";
  bool firstM = true;
  for (int i = 0; i < msgCount; i++) {
    Msg &m = msgs[(msgHead - msgCount + i + MAX_MSGS) % MAX_MSGS];
    if (m.ip != ip || !m.out || m.state) continue;
    o += String(firstM ? "" : ",") + "{\"id\":" + m.id + ",\"t\":\"" + js(m.text) + "\",\"f\":\"" + js(m.who) + "\"}";
    m.state = 1;
    firstM = false;
  }
  o += "]}";
  web.send(200, "application/json", o);
}

static void handleReply() {
  const uint32_t ip = u32(web.client().remoteIP());
  String text = web.arg("t");
  text.trim();
  if (!text.length()) return ok(false);
  const Helper *h = helperAt(ip);
  Msg &m = newMsg();
  m.ip = ip;
  m.out = 0;
  m.state = 1;
  strncpy(m.who, h && h->name[0] ? h->name : "a computer", sizeof m.who - 1);
  strncpy(m.text, text.c_str(), sizeof m.text - 1);
  ok();
}

static void handleExport() {
  web.sendHeader("Content-Disposition", "attachment; filename=\"devices.csv\"");
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "text/csv; charset=utf-8", "");
  web.sendContent("Name,Your name,IP,MAC,Maker,Kind,Here now,Ping ms,Open ports,Model,Web title,First seen (UTC),Last seen (UTC)\r\n");
  lockTable();
  for (int i = 0; i < net.n; i++) {
    const Device &d = net.dev[i];
    char ip[16], mac[18], f[3][100], portsTxt[160] = "", when[2][24];
    ipStr(d.ip, ip);
    macStr(d.mac, mac);
    for (int p = 0; p < PORT_COUNT; p++)
      if (d.ports >> p & 1) {
        const size_t l = strlen(portsTxt);
        snprintf(portsTxt + l, sizeof portsTxt - l, "%s%u", l ? " " : "", PORTS[p].port);
      }
    const uint32_t ts[2] = {d.first, d.last};
    for (int k = 0; k < 2; k++) {
      const time_t t = ts[k];
      struct tm tmv;
      gmtime_r(&t, &tmv);
      strftime(when[k], sizeof when[k], "%Y-%m-%d %H:%M", &tmv);
    }
    String row;
    csvField(d.name, f[0], sizeof f[0]);  row += f[0]; row += ',';
    csvField(d.nick, f[0], sizeof f[0]);  row += f[0]; row += ',';
    row += ip; row += ','; row += mac; row += ',';
    row += d.maker == 0xff ? (macRandom(d.mac) ? "(private address)" : "") : makerName(d.maker); row += ',';
    row += KIND_NAMES[d.kind < K_KINDS ? d.kind : 0]; row += ',';
    row += d.online ? "yes" : "no"; row += ',';
    if (d.rtt >= 0) row += d.rtt;
    row += ','; row += portsTxt; row += ',';
    csvField(d.model, f[1], sizeof f[1]); row += f[1]; row += ',';
    csvField(d.title, f[2], sizeof f[2]); row += f[2]; row += ',';
    row += when[0]; row += ','; row += when[1]; row += "\r\n";
    web.sendContent(row);
  }
  unlockTable();
  web.sendContent("");
}

static void sendScript(const char *body, const char *type) {
  String s(body);
  s.replace("%%HOSTS%%", String(HOST) + ".local " + WiFi.localIP().toString());
  web.send(200, type, s);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nscan: starting");
  lock = xSemaphoreCreateMutex();
  memset(&net, 0, sizeof net);
  ring.clear();
  loadSettings();
  if (LittleFS.begin(true)) loadDevices();
  else Serial.println("scan: no storage — devices won't be remembered across restarts");

  const wifisetup::Config cfg = {HOST, OWN_AP, "devkit-scan", "10", false};
  if (!wifisetup::begin(cfg, web)) return;     // setting up WiFi: the setup page is all there is

  configTime(0, 0, "pool.ntp.org", "time.google.com");
  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", PAGE_BODY); });
  web.on("/state", sendState);
  web.on("/scan", HTTP_POST, [] { scanAsked = true; ok(); });
  web.on("/ports", HTTP_POST, handlePorts);
  web.on("/name", HTTP_POST, handleName);
  web.on("/seen", HTTP_POST, handleSeen);
  web.on("/forget", HTTP_POST, handleForget);
  web.on("/wake", HTTP_POST, handleWake);
  web.on("/watch", HTTP_POST, handleWatch);
  web.on("/settings", HTTP_POST, handleSettings);
  web.on("/send", HTTP_POST, handleSend);
  web.on("/inbox", handleInbox);
  web.on("/reply", HTTP_POST, handleReply);
  web.on("/export.csv", handleExport);
  web.on("/i", [] { sendScript(INSTALL_SH, "text/plain; charset=utf-8"); });
  web.on("/i.ps1", [] { sendScript(INSTALL_PS1, "text/plain; charset=utf-8"); });
  web.on("/u", [] { sendScript(UNINSTALL_SH, "text/plain; charset=utf-8"); });
  web.on("/u.ps1", [] { sendScript(UNINSTALL_PS1, "text/plain; charset=utf-8"); });
  web.on("/devkit_inbox.py", [] { web.send_P(200, "text/plain; charset=utf-8", INBOX_PY); });
  web.onNotFound([] { web.send(404, "text/plain", "not here"); });
  web.begin();
  MDNS.addService("http", "tcp", 80);

  // The scanner gets its own task, so a sweep never makes the page wait.
  xTaskCreatePinnedToCore(scannerTask, "scanner", 8192, nullptr, 1, nullptr, 0);
}

void loop() {
  wifisetup::loop();
  if (!wifisetup::online()) return;
  web.handleClient();
  static uint32_t lastSave = 0;
  if (!timeFixed) { lockTable(); fixTimes(); unlockTable(); }
  if (dirty && millis() - lastSave > 60000) {
    lastSave = millis();
    lockTable();
    saveDevices();
    unlockTable();
  }
  delay(2);
}
