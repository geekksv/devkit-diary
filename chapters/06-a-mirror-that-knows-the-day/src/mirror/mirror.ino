// A mirror that knows the day.
//
// The board joins your home WiFi, fetches the weather, the air, a few headlines, "on this day"
// and the next public holiday from free services that need no account or key, and serves a
// mirror page to any screen on the same network — an old tablet behind two-way glass, a phone,
// a laptop. White on black by day, black on white at night, switched at your city's sunrise and
// sunset.
//
// Nothing is attached. The BOOT button cycles the mirror's views; held for five seconds it
// forgets the WiFi. The WiFi name and password are never in this code: the flasher hands them
// over the USB cable after flashing, or a phone does it through the setup network
// (wifi_setup.h).
//
// Board: ESP32 DevKit V1, nothing attached.
// After setup, open http://mirror.local (or the address on the serial line) on any screen.

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <time.h>
#include "ui.h"
#include "parse.h"
#include "improv.h"
#include "wifi_setup.h"
#include "net.h"
#include "mirror_page.h"
#include "settings_page.h"

const char *HOST = "mirror";                    // http://mirror.local
const char *SETUP_AP = "devkit-mirror-setup";
const int KEY_PIN = 0;                          // BOOT
const int BLUE_LED = 2;

// How often each thing is fetched. Free services ask for restraint, and none of these change
// faster than this anyway.
const uint32_t WEATHER_EVERY = 15 * 60 * 1000UL;
const uint32_t AIR_EVERY = 60 * 60 * 1000UL;
const uint32_t NEWS_EVERY = 15 * 60 * 1000UL;
const uint32_t HOLIDAY_EVERY = 12 * 60 * 60 * 1000UL;
const uint32_t RETRY = 60 * 1000UL;             // after a failure

struct Feed { const char *label, *url; };
static const Feed FEEDS[] = {
  {"BBC World",        "https://feeds.bbci.co.uk/news/world/rss.xml"},
  {"BBC Technology",   "https://feeds.bbci.co.uk/news/technology/rss.xml"},
  {"The Hindu",        "https://www.thehindu.com/news/national/feeder/default.rss"},
  {"NDTV",             "https://feeds.feedburner.com/ndtvnews-top-stories"},
  {"Times of India",   "https://timesofindia.indiatimes.com/rssfeedstopstories.cms"},
  {"NPR",              "https://feeds.npr.org/1001/rss.xml"},
  {"Al Jazeera",       "https://www.aljazeera.com/xml/rss/all.xml"},
  {"The Guardian",     "https://www.theguardian.com/world/rss"},
};
const int FEED_COUNT = sizeof(FEEDS) / sizeof(FEEDS[0]);

// ── settings, kept in flash ──────────────────────────────────────────────────

struct Settings {
  char name[24];
  char msg[121];               // a line of your own under the greeting; empty = none
  bool hasPlace;
  Place place;
  char units;                  // 'c' or 'f'
  bool clock24;
  uint8_t theme;               // 0 by the sun, 1 always black, 2 always white
  int8_t feed;                 // index into FEEDS, or -1 for `custom`
  char custom[200];
};

// ── what the mirror shows ────────────────────────────────────────────────────

struct Data {
  Weather wx;
  uint32_t wxAt;               // millis() of the last good fetch, 0 = never
  int aqi = -1;
  int nTitles;
  char titles[HEADLINES][HEADLINE_LEN];
  char feedLabel[40];
  int nEvents;
  Event events[EVENTS];
  int eventsFor;               // month*100 + day the events are for
  char holName[64], holDate[12];
  char err[80];                // the last thing that went wrong, shown small on the mirror
};

Settings S;
Data D;
// Two cores touch these: the fetcher on one, the web server on the other. `lock` keeps a page
// from reading half-written data; `netLock` keeps two HTTPS connections (each ~40 KB of RAM)
// from being open at once.
SemaphoreHandle_t lock, netLock;
volatile bool dirty = true;    // settings changed: fetch everything again now
volatile uint8_t view = 0;     // the view the BOOT button has chosen
WebServer web(80);

static void loadSettings() {
  memset(&S, 0, sizeof(S));
  Preferences p;
  p.begin("mirror", true);
  p.getString("name", "").toCharArray(S.name, sizeof(S.name));
  p.getString("msg", "").toCharArray(S.msg, sizeof(S.msg));
  S.hasPlace = p.getBytes("place", &S.place, sizeof(Place)) == sizeof(Place);
  S.units = p.getChar("units", 'c');
  S.clock24 = p.getBool("clock24", true);
  S.theme = p.getUChar("theme", 0);
  S.feed = p.getChar("feed", 0);
  p.getString("custom", "").toCharArray(S.custom, sizeof(S.custom));
  p.end();
  if (S.feed >= FEED_COUNT) S.feed = 0;
}

static void saveSettings() {
  Preferences p;
  p.begin("mirror", false);
  p.putString("name", S.name);
  p.putString("msg", S.msg);
  if (S.hasPlace) p.putBytes("place", &S.place, sizeof(Place));
  p.putChar("units", S.units);
  p.putBool("clock24", S.clock24);
  p.putUChar("theme", S.theme);
  p.putChar("feed", S.feed);
  p.putString("custom", S.custom);
  p.end();
}

// ── fetching ─────────────────────────────────────────────────────────────────

static void setErr(const char *what, int code) {
  xSemaphoreTake(lock, portMAX_DELAY);
  snprintf(D.err, sizeof(D.err), "%s: %s", what, code > 0 ? String(code).c_str() : HTTPClient::errorToString(code).c_str());
  xSemaphoreGive(lock);
  Serial.printf("fetch: %s failed (%d)\n", what, code);
}

static String coords(const Settings &s) {
  return "latitude=" + String(s.place.lat, 4) + "&longitude=" + String(s.place.lon, 4);
}

static bool fetchWeather(const Settings &s) {
  String body;
  const String url = "https://api.open-meteo.com/v1/forecast?" + coords(s) +
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
    "&timezone=auto&forecast_days=6&timeformat=unixtime" +
    String(s.units == 'f' ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");
  const int code = getSmall(url, body);
  Weather w;
  if (code != 200 || !parseWeather(body.c_str(), w)) { setErr("weather", code == 200 ? -101 : code); return false; }
  xSemaphoreTake(lock, portMAX_DELAY);
  D.wx = w;
  D.wxAt = millis();
  D.err[0] = 0;
  xSemaphoreGive(lock);
  Serial.printf("fetch: weather %.1f°, code %d, %d days\n", w.temp, w.code, w.days);
  return true;
}

static bool fetchAir(const Settings &s) {
  String body;
  const int code = getSmall("https://air-quality-api.open-meteo.com/v1/air-quality?" + coords(s) +
                            "&current=us_aqi&timezone=auto", body);
  const int aqi = code == 200 ? parseAqi(body.c_str()) : -1;
  if (aqi < 0) { setErr("air quality", code); return false; }
  xSemaphoreTake(lock, portMAX_DELAY);
  D.aqi = aqi;
  xSemaphoreGive(lock);
  Serial.printf("fetch: air quality %d\n", aqi);
  return true;
}

static bool fetchNews(const Settings &s) {
  const char *url = s.feed >= 0 ? FEEDS[s.feed].url : s.custom;
  if (!url[0]) return true;
  static char t[HEADLINES][HEADLINE_LEN];      // static: 1.3 KB is too much for the task's stack
  int n = 0;
  const int code = getStream(url, 250000, [&](Source &src) { n = parseRss(src, t, HEADLINES); });
  if (code != 200 || !n) { setErr("headlines", code == 200 ? -102 : code); return false; }
  xSemaphoreTake(lock, portMAX_DELAY);
  memcpy(D.titles, t, sizeof(t));
  D.nTitles = n;
  strncpy(D.feedLabel, s.feed >= 0 ? FEEDS[s.feed].label : "your feed", sizeof(D.feedLabel) - 1);
  xSemaphoreGive(lock);
  Serial.printf("fetch: %d headlines\n", n);
  return true;
}

static bool fetchOnThisDay(int month, int day) {
  static Event ev[EVENTS];
  int n = 0;
  char url[96];
  snprintf(url, sizeof(url), "https://api.wikimedia.org/feed/v1/wikipedia/en/onthisday/selected/%02d/%02d", month, day);
  const int code = getStream(url, 200000, [&](Source &src) { n = parseOnThisDay(src, ev, EVENTS); });
  if (code != 200 || !n) { setErr("on this day", code == 200 ? -103 : code); return false; }
  xSemaphoreTake(lock, portMAX_DELAY);
  memcpy(D.events, ev, sizeof(ev));
  D.nEvents = n;
  D.eventsFor = month * 100 + day;
  xSemaphoreGive(lock);
  Serial.printf("fetch: %d events for %02d/%02d\n", n, month, day);
  return true;
}

static bool fetchHoliday(const Settings &s) {
  if (!s.place.cc[0]) return true;
  String body;
  const int code = getSmall(String("https://date.nager.at/api/v3/NextPublicHolidays/") + s.place.cc, body);
  char name[64], date[12];
  if (code == 204 || (code == 200 && !parseHoliday(body.c_str(), name, sizeof(name), date, sizeof(date)))) {
    name[0] = date[0] = 0;                    // a country Nager.Date does not cover: show none
  } else if (code != 200) { setErr("holidays", code); return false; }
  xSemaphoreTake(lock, portMAX_DELAY);
  strncpy(D.holName, name, sizeof(D.holName));
  strncpy(D.holDate, date, sizeof(D.holDate));
  xSemaphoreGive(lock);
  return true;
}

// Runs on the other core, so a slow server never freezes the mirror page.
static void fetcher(void *) {
  uint32_t nextWx = 0, nextAir = 0, nextNews = 0, nextHol = 0, nextOtd = 0;
  for (;;) {
    delay(500);
    if (!wifisetup::online()) continue;
    if (dirty) { dirty = false; nextWx = nextAir = nextNews = nextHol = nextOtd = millis(); }
    Settings s;
    int32_t offset;
    xSemaphoreTake(lock, portMAX_DELAY);
    s = S;
    offset = D.wx.ok ? D.wx.offset : 0;
    xSemaphoreGive(lock);
    const uint32_t now = millis();
    const auto due = [&](uint32_t t) { return (int32_t)(now - t) >= 0; };

    xSemaphoreTake(netLock, portMAX_DELAY);
    if (s.hasPlace && due(nextWx)) nextWx = now + (fetchWeather(s) ? WEATHER_EVERY : RETRY);
    if (s.hasPlace && due(nextAir)) nextAir = now + (fetchAir(s) ? AIR_EVERY : RETRY);
    if (due(nextNews)) nextNews = now + (fetchNews(s) ? NEWS_EVERY : RETRY);
    if (s.hasPlace && due(nextHol)) nextHol = now + (fetchHoliday(s) ? HOLIDAY_EVERY : RETRY);
    // "On this day" follows the city's own date, so it changes at local midnight.
    const time_t t = time(nullptr);
    if (t > 1700000000 && due(nextOtd)) {
      const time_t lt = t + offset;
      struct tm d;
      gmtime_r(&lt, &d);
      const int key = (d.tm_mon + 1) * 100 + d.tm_mday;
      if (key != D.eventsFor) nextOtd = now + (fetchOnThisDay(d.tm_mon + 1, d.tm_mday) ? 60000 : RETRY);
      else nextOtd = now + 60000;
    }
    xSemaphoreGive(netLock);
  }
}

// ── pages ────────────────────────────────────────────────────────────────────

// Everything the mirror needs, about 2–3 KB. The page draws it; the board never sends pixels.
static void handleData() {
  String o;
  o.reserve(3600);
  xSemaphoreTake(lock, portMAX_DELAY);
  o += "{\"now\":" + String((uint32_t)time(nullptr));
  o += ",\"off\":" + String(D.wx.ok ? D.wx.offset : 0);
  o += ",\"name\":\"" + uiJson(S.name) + "\"";
  o += ",\"msg\":\"" + uiJson(S.msg) + "\"";
  o += ",\"place\":" + String(S.hasPlace ? "true" : "false");
  o += ",\"city\":\"" + uiJson(S.place.name) + "\"";
  o += ",\"units\":\"" + String(S.units) + "\"";
  o += ",\"clock24\":" + String(S.clock24 ? "true" : "false");
  o += ",\"theme\":" + String(S.theme);
  o += ",\"view\":" + String(view);
  o += ",\"host\":\"" + String(HOST) + "\"";
  if (D.wx.ok) {
    const Weather &w = D.wx;
    o += ",\"wx\":{\"t\":" + String(w.temp, 1) + ",\"f\":" + String(w.feels, 1) + ",\"c\":" + String(w.code) +
         ",\"h\":" + String(w.humidity) + ",\"w\":" + String(w.wind, 1) + ",\"d\":" + String(w.isDay) + "}";
    o += ",\"days\":[";
    for (int i = 0; i < w.days; i++) {
      if (i) o += ',';
      o += "{\"t\":" + String(w.dayStart[i]) + ",\"c\":" + String(w.dayCode[i]) + ",\"hi\":" + String(w.hi[i], 1) +
           ",\"lo\":" + String(w.lo[i], 1) + ",\"r\":" + String(w.sunrise[i]) + ",\"s\":" + String(w.sunset[i]) + "}";
    }
    o += "]";
  } else o += ",\"wx\":null,\"days\":[]";
  o += ",\"aqi\":" + String(D.aqi);
  o += ",\"news\":{\"src\":\"" + uiJson(D.feedLabel) + "\",\"t\":[";
  for (int i = 0; i < D.nTitles; i++) { if (i) o += ','; o += "\"" + uiJson(D.titles[i]) + "\""; }
  o += "]},\"otd\":[";
  for (int i = 0; i < D.nEvents; i++) {
    if (i) o += ',';
    o += "{\"y\":" + String(D.events[i].year) + ",\"t\":\"" + uiJson(D.events[i].text) + "\"}";
  }
  o += "]";
  if (D.holName[0]) o += ",\"hol\":{\"n\":\"" + uiJson(D.holName) + "\",\"d\":\"" + uiJson(D.holDate) + "\"}";
  o += ",\"age\":{\"wx\":" + String(D.wxAt ? (long)((millis() - D.wxAt) / 1000) : -1L) + "}";
  o += ",\"err\":\"" + uiJson(D.err) + "\"}";
  xSemaphoreGive(lock);
  web.send(200, "application/json", o);
}

static void handleSettingsGet() {
  xSemaphoreTake(lock, portMAX_DELAY);
  String o = "{\"name\":\"" + uiJson(S.name) + "\",\"msg\":\"" + uiJson(S.msg) + "\",\"units\":\"" + String(S.units) + "\",\"clock24\":" +
             String(S.clock24 ? "true" : "false") + ",\"theme\":" + String(S.theme) + ",\"feed\":" + String(S.feed) +
             ",\"custom\":\"" + uiJson(S.custom) + "\",\"place\":";
  if (S.hasPlace)
    o += "{\"name\":\"" + uiJson(S.place.name) + "\",\"region\":\"" + uiJson(S.place.region) + "\",\"country\":\"" +
         uiJson(S.place.country) + "\",\"cc\":\"" + uiJson(S.place.cc) + "\",\"lat\":" + String(S.place.lat, 4) +
         ",\"lon\":" + String(S.place.lon, 4) + "}";
  else o += "null";
  xSemaphoreGive(lock);
  o += ",\"feeds\":[";
  for (int i = 0; i < FEED_COUNT; i++) { if (i) o += ','; o += "\"" + String(FEEDS[i].label) + "\""; }
  o += "],\"ssid\":\"" + uiJson(WiFi.SSID().c_str()) + "\",\"rssi\":" + String(WiFi.RSSI()) +
       ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  web.send(200, "application/json", o);
}

// GET /api/places?q=<city>  -> up to five matches, for the settings page to choose from
static void handlePlaces() {
  String q = web.arg("q");
  q.trim();
  if (!q.length() || q.length() > 60) { web.send(200, "application/json", "{\"ok\":true,\"places\":[]}"); return; }
  String enc;
  for (char c : q) {                        // percent-encode for the query string
    if (isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_') enc += c;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", (uint8_t)c); enc += b; }
  }
  String body;
  int code = -1;
  if (xSemaphoreTake(netLock, pdMS_TO_TICKS(20000))) {     // the fetcher may be mid-download
    code = getSmall("https://geocoding-api.open-meteo.com/v1/search?count=5&language=en&format=json&name=" + enc, body);
    xSemaphoreGive(netLock);
  }
  if (code != 200) { web.send(200, "application/json", "{\"ok\":false}"); return; }
  Place p[5];
  const int n = parsePlaces(body.c_str(), p, 5);
  String o = "{\"ok\":true,\"places\":[";
  for (int i = 0; i < n; i++) {
    if (i) o += ',';
    o += "{\"name\":\"" + uiJson(p[i].name) + "\",\"region\":\"" + uiJson(p[i].region) + "\",\"country\":\"" +
         uiJson(p[i].country) + "\",\"cc\":\"" + uiJson(p[i].cc) + "\",\"lat\":" + String(p[i].lat, 4) +
         ",\"lon\":" + String(p[i].lon, 4) + "}";
  }
  o += "]}";
  web.send(200, "application/json", o);
}

static void handleSettingsPost() {
  xSemaphoreTake(lock, portMAX_DELAY);
  web.arg("name").substring(0, sizeof(S.name) - 1).toCharArray(S.name, sizeof(S.name));
  String msg = web.arg("msg");
  msg.trim();
  msg.replace("\r", " ");
  msg.replace("\n", " ");
  msg.substring(0, sizeof(S.msg) - 1).toCharArray(S.msg, sizeof(S.msg));
  S.units = web.arg("units") == "f" ? 'f' : 'c';
  S.clock24 = web.arg("clock24") != "0";
  S.theme = constrain(web.arg("theme").toInt(), 0, 2);
  S.feed = constrain(web.arg("feed").toInt(), -1, FEED_COUNT - 1);
  web.arg("custom").substring(0, sizeof(S.custom) - 1).toCharArray(S.custom, sizeof(S.custom));
  if (S.feed < 0 && !String(S.custom).startsWith("https://") && !String(S.custom).startsWith("http://")) S.feed = 0;
  if (web.hasArg("lat") && web.hasArg("lon") && web.arg("pn").length()) {
    Place &p = S.place;
    memset(&p, 0, sizeof(p));
    web.arg("pn").toCharArray(p.name, sizeof(p.name));
    web.arg("pr").toCharArray(p.region, sizeof(p.region));
    web.arg("pc").toCharArray(p.country, sizeof(p.country));
    web.arg("pcc").toCharArray(p.cc, sizeof(p.cc));
    p.lat = web.arg("lat").toDouble();
    p.lon = web.arg("lon").toDouble();
    S.hasPlace = true;
    D.wx.ok = false;                        // the old city's weather is not this city's
    D.holName[0] = 0;
  }
  saveSettings();
  xSemaphoreGive(lock);
  dirty = true;
  Serial.printf("settings: saved (%s)\n", S.hasPlace ? S.place.name : "no city yet");
  web.send(200, "application/json", "{\"ok\":true}");
}

static void handleForgetWifi() {
  web.send(200, "application/json", "{\"ok\":true}");
  delay(300);
  wifisetup::forget();
  ESP.restart();
}

// ── the BOOT button ──────────────────────────────────────────────────────────
// A short press cycles the views (all · weather · headlines · clock); five seconds held forgets
// the WiFi. Debounced: a press is a steady LOW for 30 ms.

static void readButton() {
  static int last = HIGH, stable = HIGH;
  static uint32_t changed = 0, downAt = 0;
  static bool longDone = false;
  const int r = digitalRead(KEY_PIN);
  if (r != last) { last = r; changed = millis(); }
  if (millis() - changed > 30 && r != stable) {
    stable = r;
    if (stable == LOW) { downAt = millis(); longDone = false; }
    else if (!longDone && millis() - downAt < 1500) {
      view = (view + 1) % 4;
      Serial.printf("button: view %d\n", view);
    }
  }
  if (stable == LOW && !longDone && millis() - downAt > 5000) {
    longDone = true;
    Serial.println("button: held 5 s — forgetting the WiFi");
    for (int i = 0; i < 10; i++) { digitalWrite(BLUE_LED, i % 2); delay(80); }
    wifisetup::forget();
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED, OUTPUT);
  Serial.println("\nmirror: starting");
  lock = xSemaphoreCreateMutex();
  netLock = xSemaphoreCreateMutex();
  loadSettings();

  const wifisetup::Config cfg = {HOST, SETUP_AP, "devkit-mirror", "6"};
  if (!wifisetup::begin(cfg, web)) return;    // the setup network is up instead; loop() serves it

  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", MIRROR_HTML); });
  web.on("/settings", [] { uiSend(web, "Mirror settings", SETTINGS_BODY); });
  web.on("/api/data", handleData);
  web.on("/api/settings", HTTP_GET, handleSettingsGet);
  web.on("/api/settings", HTTP_POST, handleSettingsPost);
  web.on("/api/places", handlePlaces);
  web.on("/api/forget-wifi", HTTP_POST, handleForgetWifi);
  web.onNotFound([] { web.sendHeader("Location", "/", true); web.send(302, "text/plain", ""); });
  web.begin();

  // The fetcher gets core 0, beside the WiFi stack; the web server stays on loop()'s core 1.
  xTaskCreatePinnedToCore(fetcher, "fetcher", 12288, nullptr, 1, nullptr, 0);
  Serial.printf("mirror: open http://%s.local or %s on any screen; settings at /settings\n", HOST,
                wifisetup::url().c_str());
  Serial.printf("mirror: free heap %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  if (!S.hasPlace) Serial.println("mirror: no city yet — choose one at /settings");
}

void loop() {
  wifisetup::loop();
  readButton();
  if (wifisetup::portal()) {
    digitalWrite(BLUE_LED, (millis() / 1000) % 2);    // slow blink: waiting to be set up
    return;
  }
  web.handleClient();
}
