// A pet that lives on the WiFi.
//
// The board is the pet's home and its brain: its hunger, happiness and energy run here, and the
// board alone decides which computer it is on. Every computer running the small helper checks in
// about once a second; the pet walks around the bottom of that screen, over whatever is open, and
// when it walks off the edge the board sends it to the next computer. A phone opens the pet's
// page to adopt it, feed it, play, put it to bed, or call it to a particular screen.
//
// The helper and its one-line installer are served by the board itself, so the command to add a
// computer always matches this firmware: curl -fsSL http://pet.local/i | sh
//
// It runs on your home WiFi (pet.local), or makes its own network devkit-pet as a fallback.
//
// Board: ESP32 DevKit V1, nothing attached.

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "pet.h"
#include "pet_page.h"
#include "files.h"

const char *HOST = "pet";
const char *OWN_AP = "devkit-pet";
const uint32_t TICK_MS = 1000;          // the pet's life advances once a second
const uint32_t SAVE_MS = 300000;        // and is written to flash every 5 minutes (and on care)

WebServer web(80);
Preferences prefs;
PetWorld world;
float clockHour = -1;                   // local time of day, as last told by a computer or phone
uint32_t clockAt = 0;
bool dirty = false;
int petSize = 120;                      // how tall it stands on a computer screen, px (70..240)

// ── time of day: taken from whoever checks in, so there is no timezone to set ──────────────────

static void heardHour(const String &h) {
  if (!h.length()) return;
  const float v = h.toFloat();
  if (v >= 0 && v < 24) { clockHour = v; clockAt = millis(); }
}

static float hourNow() {
  if (clockHour < 0) return -1;
  float h = clockHour + (millis() - clockAt) / 3600000.0f;
  while (h >= 24) h -= 24;
  return h;
}

// ── keeping the pet across a power cut ───────────────────────────────────────────────────────

static void load() {
  prefs.begin("pet", true);
  PetLife &p = world.pet;
  p.adopted = prefs.getBool("adopted", false);
  p.kind = prefs.getUChar("kind", PET_CAT);
  if (p.kind >= PET_KINDS) p.kind = PET_CAT;
  prefs.getString("name", p.name, sizeof p.name);
  p.hunger = prefs.getFloat("hunger", 80);
  p.happy = prefs.getFloat("happy", 80);
  p.energy = prefs.getFloat("energy", 80);
  p.asleep = prefs.getBool("asleep", false);
  petSize = constrain(prefs.getInt("size", 120), 70, 240);
  prefs.end();
}

static void save() {
  const PetLife &p = world.pet;
  prefs.begin("pet", false);
  prefs.putBool("adopted", p.adopted);
  prefs.putUChar("kind", p.kind);
  prefs.putString("name", p.name);
  prefs.putFloat("hunger", p.hunger);
  prefs.putFloat("happy", p.happy);
  prefs.putFloat("energy", p.energy);
  prefs.putBool("asleep", p.asleep);
  prefs.putInt("size", petSize);
  prefs.end();
  dirty = false;
}

// ── the JSON a screen draws ──────────────────────────────────────────────────────────────────

static const char *EV_NAMES[] = {"", "feed", "pet", "play", "sleep", "wake", "poke", "adopt"};

static String petJson() {
  const PetLife &p = world.pet;
  if (!p.adopted) return "null";
  String o = "{\"k\":" + String(p.kind) + ",\"n\":\"" + uiJson(p.name) + "\",\"mood\":\"" + petMood(p) + "\"";
  o += ",\"sleep\":" + String(p.asleep ? 1 : 0);
  o += ",\"hun\":" + String((int)(p.hunger + 0.5f)) + ",\"hap\":" + String((int)(p.happy + 0.5f));
  o += ",\"en\":" + String((int)(p.energy + 0.5f)) + ",\"sz\":" + String(petSize) + "}";
  return o;
}

static String eventJson() {
  return ",\"ev\":" + String(world.evId) + ",\"evk\":\"" + EV_NAMES[world.evKind < 8 ? world.evKind : 0] + "\"";
}

// the phone page: the pet, and every computer it could be on
static void sendState() {
  const uint32_t now = millis();
  String o = "{\"ok\":true,\"pet\":" + petJson() + eventJson() + ",\"devs\":[";
  bool first = true;
  for (int i = 0; i < PET_MAX_DEV; i++) {
    if (!world.dev[i].used) continue;
    const bool on = petOnline(world, i, now);
    if (!on && now - world.dev[i].seen > 24UL * 3600000) continue;   // forget after a day away
    if (!first) o += ',';
    first = false;
    o += "{\"i\":" + String(i) + ",\"n\":\"" + uiJson(world.dev[i].name) + "\",\"on\":" + String(on ? 1 : 0)
       + ",\"here\":" + String(world.at == i ? 1 : 0) + "}";
  }
  o += "],\"board\":\"" + (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString() + "\"}";
  web.send(200, "application/json", o);
}

static void fail(const char *why) {
  web.send(200, "application/json", String("{\"ok\":false,\"err\":\"") + why + "\"}");
}

static int sideRand() { return esp_random() & 1; }

static void cleanText(const String &in, char *out, size_t cap) {
  size_t n = 0;
  for (size_t i = 0; i < in.length() && n + 1 < cap; i++) {
    const char c = in[i];
    if (c >= 32 && c != '"' && c != '\\' && c != '<' && c != '>') out[n++] = c;
  }
  out[n] = 0;
}

// ── a computer checks in ─────────────────────────────────────────────────────────────────────

static void handleBeat() {
  char id[PET_ID + 1], name[PET_DEV_NAME + 1];
  cleanText(web.arg("id"), id, sizeof id);
  cleanText(web.arg("n"), name, sizeof name);
  if (strlen(id) != PET_ID) return fail("id");
  heardHour(web.arg("h"));
  const uint32_t now = millis();
  const int me = petBeat(world, id, name[0] ? name : "computer", now, sideRand());
  if (me < 0) return fail("full");
  String o = "{\"ok\":true,\"here\":" + String(world.at == me ? 1 : 0);
  o += ",\"arr\":" + String(world.arrival) + ",\"side\":" + String(world.side);
  o += ",\"others\":" + String(petOnlineCount(world, now) - 1);
  o += ",\"pet\":" + petJson() + eventJson() + "}";
  web.send(200, "application/json", o);
}

static int deviceOf(const String &id) {
  for (int i = 0; i < PET_MAX_DEV; i++)
    if (world.dev[i].used && id == world.dev[i].id) return i;
  return -1;
}

// it walked off this computer's screen
static void handleLeave() {
  const int me = deviceOf(web.arg("id"));
  if (me < 0) return fail("who");
  const int to = petLeave(world, me, web.arg("s").toInt() ? 1 : 0, millis());
  Serial.printf("pet: walked off %s -> %s\n", world.dev[me].name, to >= 0 ? world.dev[to].name : "-");
  web.send(200, "application/json", "{\"ok\":true}");
}

// clicked on a computer screen
static void handlePoke() {
  if (deviceOf(web.arg("id")) < 0) return fail("who");
  petAct(world, "poke");
  dirty = true;
  web.send(200, "application/json", "{\"ok\":true}");
}

// ── the phone ────────────────────────────────────────────────────────────────────────────────

static void handleAdopt() {
  char name[PET_NAME + 1];
  cleanText(web.arg("n"), name, sizeof name);
  if (!name[0]) return fail("name");
  petAdopt(world, (uint8_t)constrain(web.arg("k").toInt(), 0, PET_KINDS - 1), name);
  world.at = -1;
  petSweep(world, millis(), sideRand());      // straight onto a computer, if one is online
  save();
  sendState();
}

static void handleAct() {
  static const char *ACT_ERRS[] = {"", "asleep", "full", "tired", "nopet", "unknown"};
  const String a = web.arg("a");
  if (a == "poke") return fail("unknown");
  const int r = petAct(world, a.c_str());
  if (r != ACT_OK) return fail(ACT_ERRS[r]);
  save();
  sendState();
}

static void handleCall() {
  if (!petCall(world, web.arg("i").toInt(), millis(), sideRand())) return fail("offline");
  sendState();
}

// the size slider on the phone: every computer resizes the pet within a second
static void handleSize() {
  petSize = constrain(web.arg("v").toInt(), 70, 240);
  save();
  sendState();
}

static void handleRelease() {
  world.pet.adopted = false;
  world.at = -1;
  save();
  sendState();
}

// ── the installer and the helper, straight from the board ────────────────────────────────────

// The installer is told every address this board answers on, so it works whichever one the
// computer can reach: pet.local first, the plain IP if this network won't resolve names.
static void sendScript(const char *body, const char *type) {
  String s(body);
  const String ip = (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString();
  s.replace("%%HOSTS%%", String(HOST) + ".local " + ip);
  web.send(200, type, s);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\npet: starting");
  memset(&world, 0, sizeof world);
  world.at = -1;
  load();
  Serial.printf("pet: %s\n", world.pet.adopted ? world.pet.name : "(none adopted yet)");

  const wifisetup::Config cfg = {HOST, OWN_AP, "devkit-pet", "9", true};
  wifisetup::begin(cfg, web);

  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", PET_BODY); });
  web.on("/state", sendState);
  web.on("/adopt", HTTP_POST, handleAdopt);
  web.on("/act", HTTP_POST, handleAct);
  web.on("/call", HTTP_POST, handleCall);
  web.on("/release", HTTP_POST, handleRelease);
  web.on("/size", HTTP_POST, handleSize);
  web.on("/beat", handleBeat);
  web.on("/leave", HTTP_POST, handleLeave);
  web.on("/poke", HTTP_POST, handlePoke);
  web.on("/i", [] { sendScript(INSTALL_SH, "text/plain; charset=utf-8"); });
  web.on("/i.ps1", [] { sendScript(INSTALL_PS1, "text/plain; charset=utf-8"); });
  web.on("/u", [] { sendScript(UNINSTALL_SH, "text/plain; charset=utf-8"); });
  web.on("/u.ps1", [] { sendScript(UNINSTALL_PS1, "text/plain; charset=utf-8"); });
  web.on("/devkit_pet.py", [] { web.send_P(200, "text/plain; charset=utf-8", HELPER_PY); });
  web.onNotFound([] {
    const String home = (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString();
    web.sendHeader("Location", String("http://") + home + "/", true);
    web.send(302, "text/plain", "");
  });
  web.begin();

  Serial.printf("pet: open http://%s.local or http://%s\n", HOST,
                (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
  Serial.printf("pet: add a computer with  curl -fsSL http://%s.local/i | sh\n", HOST);
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  static uint32_t lastTick = 0, lastSave = 0;
  const uint32_t now = millis();
  if (now - lastTick >= TICK_MS) {
    const float dt = (now - lastTick) / 1000.0f;
    lastTick = now;
    if (world.pet.adopted) {
      const bool wasAsleep = world.pet.asleep;
      petTick(world, dt > 5 ? 1 : dt, hourNow());
      if (wasAsleep != world.pet.asleep) {
        Serial.printf("pet: %s\n", world.pet.asleep ? "fell asleep" : "woke up");
        dirty = true;
      }
    }
    petSweep(world, now, sideRand());
  }
  if (now - lastSave >= SAVE_MS) {
    lastSave = now;
    if (world.pet.adopted || dirty) save();
  }
}
