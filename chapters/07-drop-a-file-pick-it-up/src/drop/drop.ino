// Drop a file, pick it up on the other device.
//
// Open the board's page on a laptop and on a phone. Each sees the other in a list of devices
// here. Pick a file on one, choose the other, it accepts — and the file goes across, through
// the board, with no cable, no cloud, no account and no app.
//
// The board never keeps a file. It is a pipe: it takes a few kilobytes from the sender, hands
// them to the receiver, and takes the next few — so a file can be far bigger than the board's
// 4 MB of flash, and nothing is left behind afterwards. Only short text notes are kept, and
// they clear themselves after a day. An optional PIN keeps all of it to people who know it.
//
// On your home WiFi it answers at http://drop.local. With no WiFi set up, or none in reach, it
// makes its own network, devkit-drop, and works there instead.
//
// Board: ESP32 DevKit V1, nothing attached.

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <esp_random.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "pipe.h"
#include "drop_page.h"

const char *HOST = "drop";                 // http://drop.local
const char *OWN_AP = "devkit-drop";
const int KEY_PIN = 0;                     // BOOT
const int BLUE_LED = 2;

const int MAX_NOTES = 60;
const size_t TEXT_MAX = 8192;              // a pasted note, a link, a code block
const int MAX_TOKENS = 16;                 // devices that have entered the PIN since boot
const int MAX_DEVICES = 8;
const uint32_t DEVICE_GONE_MS = 8000;      // a page that has not polled for this long has left
const uint32_t OFFER_MS = 60000;           // an offer nobody accepts is withdrawn after a minute
const uint32_t CLOCK_OK = 1700000000;      // any time before this is "not set yet"

// ── text notes: the only thing kept ──────────────────────────────────────────

struct Note {
  char id[9];
  uint32_t at;          // Unix time it arrived; 0 if the board did not know the time yet
  uint16_t size;
  char from[24];
};

Note notes[MAX_NOTES];
int noteCount = 0;

struct Config {
  char pin[9];          // empty = no PIN
  int hours;            // notes clear after this many hours; 0 = never
} C;

// ── who is here, and what is being sent ──────────────────────────────────────

struct Device {
  char id[9];
  char name[24];
  uint32_t seen;
};
Device devices[MAX_DEVICES];

// One transfer at a time — the board has one radio, and the file crosses it twice.
struct Offer {
  bool active;
  char id[9];
  char from[9], to[9];          // device ids
  char fromName[24];
  char name[96];                // the file's name
  char mime[48];
  uint64_t size;                // 64-bit: files may be bigger than 4 GB
  uint32_t at;                  // millis() it was offered
  enum State { OFFERED, ACCEPTED, SENDING, DONE, DECLINED, FAILED } state;
  char why[48];                 // why it failed, for the sender
} offer;

struct Token { uint32_t value, seen; };
Token tokens[MAX_TOKENS];
int badTries = 0;
uint32_t lockedUntil = 0;
uint32_t ver = 1;               // bumps whenever the list of notes, devices or the offer changes

WebServer web(80);

// ── settings ─────────────────────────────────────────────────────────────────

static void loadConfig() {
  Preferences p;
  p.begin("drop", true);
  p.getString("pin", "").toCharArray(C.pin, sizeof(C.pin));
  C.hours = p.getInt("hours", 24);
  p.end();
}

static void saveConfig() {
  Preferences p;
  p.begin("drop", false);
  p.putString("pin", C.pin);
  p.putInt("hours", C.hours);
  p.end();
}

// ── the clock ────────────────────────────────────────────────────────────────
// Notes expire, so the board needs the time. On home WiFi it asks NTP. On its own network there
// is no internet — but every phone that opens the page knows the time exactly, so the page sends
// it along and the board sets its clock from the first one that does.

static bool clockOk() { return time(nullptr) > CLOCK_OK; }

static void clockFromPage() {
  if (clockOk() || !web.hasArg("now")) return;
  const uint64_t ms = strtoull(web.arg("now").c_str(), NULL, 10);
  if (ms / 1000 < CLOCK_OK) return;
  timeval tv = {(time_t)(ms / 1000), (suseconds_t)((ms % 1000) * 1000)};
  settimeofday(&tv, NULL);
  Serial.println("clock: set from a browser");
}

// ── notes on flash: /n/<id> is the text, /a/<id> one line — when, and from whom ──

static String textPath(const char *id) { return String("/n/") + id; }
static String infoPath(const char *id) { return String("/a/") + id; }

static void writeInfo(const Note &n) {
  File f = LittleFS.open(infoPath(n.id), "w");
  if (f) { f.printf("%u\n%s\n", (unsigned)n.at, n.from); f.close(); }
}

static void loadNotes() {
  noteCount = 0;
  LittleFS.mkdir("/n");
  LittleFS.mkdir("/a");
  File dir = LittleFS.open("/n");
  for (File t = dir.openNextFile(); t && noteCount < MAX_NOTES; t = dir.openNextFile()) {
    Note &n = notes[noteCount];
    memset(&n, 0, sizeof(n));
    strncpy(n.id, t.name(), 8);
    n.size = t.size();
    t.close();
    File a = LittleFS.open(infoPath(n.id), "r");
    if (a) {
      n.at = strtoul(a.readStringUntil('\n').c_str(), NULL, 10);
      a.readStringUntil('\n').toCharArray(n.from, sizeof(n.from));
      a.close();
    }
    noteCount++;
  }
  Serial.printf("notes: %d kept\n", noteCount);
}

static int findNote(const String &id) {
  for (int i = 0; i < noteCount; i++) if (id == notes[i].id) return i;
  return -1;
}

static void removeNote(int i) {
  LittleFS.remove(textPath(notes[i].id));
  LittleFS.remove(infoPath(notes[i].id));
  notes[i] = notes[--noteCount];
  ver++;
}

static void randomId(char *out) { snprintf(out, 9, "%08lx", (unsigned long)esp_random()); }

// Once a minute: start the clock on notes that arrived before the time was known, and delete the
// ones that have run out.
static void sweepNotes() {
  if (!clockOk()) return;
  const uint32_t now = time(nullptr);
  for (int i = noteCount - 1; i >= 0; i--) {
    if (!notes[i].at) { notes[i].at = now; writeInfo(notes[i]); continue; }
    if (C.hours > 0 && now - notes[i].at > (uint32_t)C.hours * 3600) removeNote(i);
  }
}

// ── devices ──────────────────────────────────────────────────────────────────

static Device *device(const String &id) {
  if (id.length() != 8) return nullptr;
  for (auto &d : devices) if (d.seen && id == d.id) return &d;
  return nullptr;
}

// Every poll is a heartbeat: the page says who it is, and the board keeps it in the list.
static Device *heartbeat() {
  const String id = web.arg("d");
  if (id.length() != 8) return nullptr;
  Device *d = device(id);
  if (!d) {
    int slot = 0;
    for (int i = 1; i < MAX_DEVICES; i++) if (devices[i].seen < devices[slot].seen) slot = i;
    d = &devices[slot];
    memset(d, 0, sizeof(*d));
    id.toCharArray(d->id, sizeof(d->id));
    ver++;
  }
  String name = web.arg("dn");
  name.trim();
  if (name.length() && name != d->name) { name.substring(0, sizeof(d->name) - 1).toCharArray(d->name, sizeof(d->name)); ver++; }
  d->seen = millis() | 1;
  return d;
}

static void sweepDevices() {
  for (auto &d : devices)
    if (d.seen && millis() - d.seen > DEVICE_GONE_MS) { d.seen = 0; ver++; }
  if (offer.active && (offer.state == Offer::OFFERED || offer.state == Offer::ACCEPTED) &&
      millis() - offer.at > OFFER_MS) {
    offer.state = Offer::FAILED;
    strcpy(offer.why, "nobody accepted it in time");
    ver++;
  }
  // A finished, declined or failed offer is cleared once both sides have had time to see it.
  if (offer.active && offer.state >= Offer::DONE && millis() - offer.at > OFFER_MS + 15000) {
    offer.active = false;
    ver++;
  }
}

// ── the PIN ──────────────────────────────────────────────────────────────────
// The right PIN gets a device a token, kept by its browser and sent with every request —
// including to the pipe. Five wrong PINs lock the door for a minute: four digits are only
// 10,000 guesses, and that should take a week, not a minute.

static bool tokenOk(const String &k) {
  if (!C.pin[0]) return true;
  const uint32_t t = strtoul(k.c_str(), NULL, 16);
  if (!t) return false;
  for (auto &x : tokens) if (x.value == t) { x.seen = millis(); return true; }
  return false;
}
static bool authorized() { return tokenOk(web.arg("k")); }

static uint32_t newToken() {
  int slot = 0;
  for (int i = 1; i < MAX_TOKENS; i++)
    if (!tokens[i].value || tokens[i].seen < tokens[slot].seen) slot = i;
  uint32_t t;
  do t = esp_random(); while (!t);
  tokens[slot] = {t, millis()};
  return t;
}

static void json(const String &s) { web.send(200, "application/json", s); }
static void fail(const char *err) { json(String("{\"ok\":false,\"err\":\"") + err + "\"}"); }

// ── endpoints on port 80 ─────────────────────────────────────────────────────

static const char *STATE_NAME[] = {"offered", "accepted", "sending", "done", "declined", "failed"};

// GET /api/state?v=<ver>&k=<token>&d=<device id>&dn=<device name>&now=<ms>
static void handleState() {
  clockFromPage();
  const bool own = wifisetup::portal();
  const bool ok = authorized();
  const Device *me = ok ? heartbeat() : nullptr;
  String o = "{\"ok\":" + String(ok ? "true" : "false");
  o += ",\"pin\":" + String(C.pin[0] ? "true" : "false");
  o += ",\"locked\":" + String(millis() < lockedUntil ? (long)((lockedUntil - millis()) / 1000) : 0L);
  o += ",\"mode\":\"" + String(own ? "own" : "home") + "\"";
  o += ",\"net\":\"" + uiJson(own ? OWN_AP : WiFi.SSID().c_str()) + "\"";
  o += ",\"host\":\"" + String(HOST) + "\",\"ip\":\"" + (own ? WiFi.softAPIP() : WiFi.localIP()).toString() + "\"";
  o += ",\"pipe\":" + String(PIPE_PORT);
  if (ok) {
    o += ",\"ver\":" + String(ver) + ",\"hours\":" + String(C.hours);
    o += ",\"now\":" + String(clockOk() ? (uint32_t)time(nullptr) : 0);
    // The offer is always sent: the sender's progress and the receiver's prompt depend on it.
    if (offer.active && me && (!strcmp(offer.from, me->id) || !strcmp(offer.to, me->id))) {
      o += ",\"offer\":{\"id\":\"" + String(offer.id) + "\",\"role\":\"" + (strcmp(offer.from, me->id) ? "to" : "from") +
           "\",\"state\":\"" + STATE_NAME[offer.state] + "\",\"name\":\"" + uiJson(offer.name) + "\",\"size\":" +
           u64(offer.size) + ",\"sent\":" + u64(pipeSent()) + ",\"fromName\":\"" + uiJson(offer.fromName) +
           "\",\"why\":\"" + uiJson(offer.why) + "\"}";
    }
    o += ",\"busy\":" + String(offer.active && offer.state < Offer::DONE ? "true" : "false");
    if (web.arg("v") != String(ver)) {
      o += ",\"devices\":[";
      bool first = true;
      for (auto &d : devices) {
        if (!d.seen || (me && !strcmp(d.id, me->id))) continue;
        if (!first) o += ',';
        first = false;
        o += "{\"id\":\"" + String(d.id) + "\",\"n\":\"" + uiJson(d.name[0] ? d.name : "a device") + "\"}";
      }
      o += "],\"notes\":[";
      for (int i = 0; i < noteCount; i++) {
        if (i) o += ',';
        File f = LittleFS.open(textPath(notes[i].id), "r");
        String t = f ? f.readString() : String("");
        if (f) f.close();
        o += "{\"id\":\"" + String(notes[i].id) + "\",\"at\":" + String(notes[i].at) + ",\"from\":\"" +
             uiJson(notes[i].from) + "\",\"x\":\"" + uiJson(t.c_str()) + "\"}";
      }
      o += "]";
    }
  }
  o += "}";
  json(o);
}

// POST /api/login  pin=…
static void handleLogin() {
  if (!C.pin[0]) { json("{\"ok\":true,\"k\":\"\"}"); return; }
  if (millis() < lockedUntil) { fail("locked"); return; }
  if (web.arg("pin") != C.pin) {
    if (++badTries >= 5) { badTries = 0; lockedUntil = millis() + 60000; Serial.println("pin: 5 wrong tries, locked 60 s"); }
    fail("wrong");
    return;
  }
  badTries = 0;
  json("{\"ok\":true,\"k\":\"" + String(newToken(), HEX) + "\"}");
}

// POST /api/offer  d=<from>&to=<device>&name=…&size=…&type=…  — "I'd like to send this to them"
static void handleOffer() {
  if (!authorized()) { fail("pin"); return; }
  Device *from = heartbeat();
  Device *to = device(web.arg("to"));
  if (!from || !to) { fail("gone"); return; }
  if (offer.active && offer.state < Offer::DONE) { fail("busy"); return; }
  memset(&offer, 0, sizeof(offer));
  offer.active = true;
  randomId(offer.id);
  strcpy(offer.from, from->id);
  strcpy(offer.to, to->id);
  strncpy(offer.fromName, from->name[0] ? from->name : "a device", sizeof(offer.fromName) - 1);
  web.arg("name").substring(0, sizeof(offer.name) - 1).toCharArray(offer.name, sizeof(offer.name));
  if (!offer.name[0]) strcpy(offer.name, "file");
  String m = web.arg("type");
  (m.length() ? m : String("application/octet-stream")).substring(0, sizeof(offer.mime) - 1).toCharArray(offer.mime, sizeof(offer.mime));
  offer.size = strtoull(web.arg("size").c_str(), NULL, 10);
  offer.at = millis();
  offer.state = Offer::OFFERED;
  ver++;
  Serial.printf("pipe: %s offers \"%s\" (%s bytes) to %s\n", offer.fromName, offer.name, u64(offer.size).c_str(),
                to->name);
  json("{\"ok\":true,\"id\":\"" + String(offer.id) + "\"}");
}

// POST /api/answer  d=<me>&id=<offer>&yes=0  — the receiver declines (accepting is the download)
static void handleAnswer() {
  if (!authorized()) { fail("pin"); return; }
  Device *me = heartbeat();
  if (!me || !offer.active || web.arg("id") != offer.id || strcmp(offer.to, me->id) || offer.state != Offer::OFFERED) {
    fail("gone");
    return;
  }
  offer.state = web.arg("yes") == "1" ? Offer::ACCEPTED : Offer::DECLINED;
  offer.at = millis();
  ver++;
  json("{\"ok\":true}");
}

// POST /api/cancel  d=<me>  — either side calls it off
static void handleCancel() {
  if (!authorized()) { fail("pin"); return; }
  Device *me = heartbeat();
  if (me && offer.active && (!strcmp(offer.from, me->id) || !strcmp(offer.to, me->id)) && offer.state < Offer::DONE) {
    pipeAbort();
    offer.state = Offer::FAILED;
    strcpy(offer.why, "cancelled");
    offer.at = millis();
    ver++;
  }
  json("{\"ok\":true}");
}

// POST /api/text  x=…&d=…  — a note, a link, a password: anything you would otherwise email yourself
static void handleText() {
  clockFromPage();
  if (!authorized()) { fail("pin"); return; }
  Device *me = heartbeat();
  String x = web.arg("x");
  if (!x.length()) { fail("empty"); return; }
  if (x.length() > TEXT_MAX) { fail("long"); return; }
  if (noteCount >= MAX_NOTES) { fail("many"); return; }
  Note n = {};
  randomId(n.id);
  n.at = clockOk() ? time(nullptr) : 0;
  n.size = x.length();
  strncpy(n.from, me && me->name[0] ? me->name : "a device", sizeof(n.from) - 1);
  File f = LittleFS.open(textPath(n.id), "w");
  if (!f || f.print(x) != x.length()) { if (f) f.close(); LittleFS.remove(textPath(n.id)); fail("write"); return; }
  f.close();
  writeInfo(n);
  notes[noteCount++] = n;
  ver++;
  json("{\"ok\":true}");
}

// POST /api/delete  id=…
static void handleDelete() {
  if (!authorized()) { fail("pin"); return; }
  const int i = findNote(web.arg("id"));
  if (i >= 0) removeNote(i);
  json("{\"ok\":true}");
}

// POST /api/settings  hours=…  [pin=new&old=current]  [clearpin=1&old=current]
static void handleSettings() {
  if (!authorized()) { fail("pin"); return; }
  if (web.hasArg("hours")) C.hours = constrain(web.arg("hours").toInt(), 0, 24 * 30);
  const bool changing = web.hasArg("pin") || web.arg("clearpin") == "1";
  if (changing && C.pin[0] && web.arg("old") != C.pin) { fail("old"); return; }
  String k = "";
  if (web.arg("clearpin") == "1") { C.pin[0] = 0; memset(tokens, 0, sizeof(tokens)); }
  else if (web.hasArg("pin")) {
    const String p = web.arg("pin");
    bool digits = p.length() >= 4 && p.length() <= 8;
    for (char c : p) digits = digits && isdigit((unsigned char)c);
    if (!digits) { fail("format"); return; }
    p.toCharArray(C.pin, sizeof(C.pin));
    memset(tokens, 0, sizeof(tokens));          // every other device must enter the new PIN…
    k = String(newToken(), HEX);                // …but not the one that just set it
  }
  saveConfig();
  ver++;
  json("{\"ok\":true,\"k\":\"" + k + "\"}");
}

static void handleForgetWifi() {
  if (!authorized()) { fail("pin"); return; }
  json("{\"ok\":true}");
  delay(300);
  wifisetup::forget();
  ESP.restart();
}

// ── the pipe's side of an offer (called from pipe.h) ─────────────────────────

// A connection on the pipe port names an offer and a role; this says whether to let it in.
// The receiver's download *is* its acceptance — the Accept button is the download link itself,
// because browsers only allow a download that starts straight from a tap — so a receiver may
// connect while the offer is still open; the sender only once it has been accepted.
bool pipeAllowed(const String &offerId, bool sender, const String &k) {
  if (!tokenOk(k) || !offer.active || offerId != offer.id) return false;
  if (sender) return offer.state == Offer::ACCEPTED;
  return offer.state == Offer::OFFERED || offer.state == Offer::ACCEPTED;
}
void pipeReceiverReady() {
  if (offer.state == Offer::OFFERED) { offer.state = Offer::ACCEPTED; offer.at = millis(); ver++; }
}
void pipeDescribe(String &name, String &mime, uint64_t &size) { name = offer.name; mime = offer.mime; size = offer.size; }
void pipeStarted() { offer.state = Offer::SENDING; ver++; }
void pipeFinished(bool ok, const char *why) {
  offer.state = ok ? Offer::DONE : Offer::FAILED;
  strncpy(offer.why, why ? why : "", sizeof(offer.why) - 1);
  offer.at = millis();
  ver++;
}

// ── the BOOT button ──────────────────────────────────────────────────────────
// Held five seconds: forget the PIN and the WiFi — the way back in for someone locked out. The
// notes stay. A short press prints the address on the serial line.

static void readButton() {
  static int last = HIGH, stable = HIGH;
  static uint32_t changed = 0, downAt = 0;
  static bool done = false;
  const int r = digitalRead(KEY_PIN);
  if (r != last) { last = r; changed = millis(); }
  if (millis() - changed > 30 && r != stable) {
    stable = r;
    if (stable == LOW) { downAt = millis(); done = false; }
    else if (!done) Serial.printf("drop: http://%s.local · %s\n", HOST,
                                  (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
  }
  if (stable == LOW && !done && millis() - downAt > 5000) {
    done = true;
    Serial.println("button: held 5 s — clearing the PIN and the WiFi; notes are kept");
    for (int i = 0; i < 10; i++) { digitalWrite(BLUE_LED, i % 2); delay(80); }
    C.pin[0] = 0;
    saveConfig();
    wifisetup::forget();
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED, OUTPUT);
  Serial.println("\ndrop: starting");
  loadConfig();
  if (!LittleFS.begin(true)) Serial.println("notes: could not mount the filesystem");
  loadNotes();

  const wifisetup::Config cfg = {HOST, OWN_AP, "devkit-drop", "7", true};
  const bool online = wifisetup::begin(cfg, web);
  if (online) configTime(0, 0, "pool.ntp.org", "time.google.com");

  web.on("/", [] { uiSend(web, "Drop", DROP_BODY); });
  web.on("/api/state", handleState);
  web.on("/api/login", HTTP_POST, handleLogin);
  web.on("/api/offer", HTTP_POST, handleOffer);
  web.on("/api/answer", HTTP_POST, handleAnswer);
  web.on("/api/cancel", HTTP_POST, handleCancel);
  web.on("/api/text", HTTP_POST, handleText);
  web.on("/api/delete", HTTP_POST, handleDelete);
  web.on("/api/settings", HTTP_POST, handleSettings);
  web.on("/api/forget-wifi", HTTP_POST, handleForgetWifi);
  web.onNotFound([] {
    // On its own network every phone's "is there internet?" probe lands here; send it to the
    // drop page — chapter 3's redirect.
    const String home = (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString();
    web.sendHeader("Location", String("http://") + home + "/", true);
    web.send(302, "text/plain", "");
  });
  web.begin();
  pipeBegin();

  Serial.printf("drop: open http://%s.local or http://%s\n", HOST,
                (online ? WiFi.localIP() : WiFi.softAPIP()).toString().c_str());
  Serial.printf("drop: free heap %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  pipeLoop();                        // moves a few kilobytes of any transfer, then returns
  readButton();
  static uint32_t lastSweep = 0;
  if (millis() - lastSweep > 1000) {
    lastSweep = millis();
    sweepDevices();
    static int ticks = 0;
    if (++ticks % 60 == 0) sweepNotes();
  }
  digitalWrite(BLUE_LED, pipeBusy() ? HIGH : LOW);
}
