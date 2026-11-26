// No ads on this WiFi: an ad blocker for every phone and laptop, with nothing installed on them.
//
// The board joins your home WiFi and makes a second network, "NoAds". Devices that join NoAds get
// the internet through the board, and the board answers their DNS questions ("where is
// ads.example.com?"). For the ~180,000 ad and tracker names on the blocklist it answers
// "nowhere", so the ad never loads. Everything else is asked of a real DNS server and passed back.
//
// It also works without NoAds: point your router's DNS setting at the board's home address, and
// every device on your home WiFi is covered (README, "Router mode").
//
// The decisions live in filter.h, the list in blocklist.h and builder.h, all tested on a laptop.
// This file moves packets, keeps the list in flash, updates it weekly, and serves the dashboard.
//
// Board: ESP32 DevKit V1, nothing attached. Partition scheme: "No OTA (2MB APP/2MB SPIFFS)", for
// room to keep the list and build the next one.

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <new>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "builder.h"
#include "noads_page.h"

const char *HOST = "noads";

// The lists on offer. HaGeZi's lists are kept by hand to block ads and trackers without breaking
// sites; StevenBlack's is the classic hosts list, stricter on Google's ad links.
struct ListChoice { const char *id, *name, *url; };
static const ListChoice LISTS[] = {
    {"normal", "Normal (HaGeZi Multi, ~185,000 names)",
     "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/multi-onlydomains.txt"},
    {"light", "Light (HaGeZi Light, ~85,000 names)",
     "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/light-onlydomains.txt"},
    {"classic", "Classic (StevenBlack hosts, ~80,000 names)",
     "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts"},
};

extern const uint8_t crtBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t crtBundleEnd[] asm("_binary_x509_crt_bundle_end");

WebServer web(80);
static Preferences prefs;
static SemaphoreHandle_t lock;           // stats, cache, your lists, settings
static SemaphoreHandle_t listLock;       // the list file and its index

// ── settings ─────────────────────────────────────────────────────────────────
static char apName[33] = "NoAds", apPass[65] = "";
static char listId[12] = "normal";
static IPAddress upstream[2] = {IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8)};
static NameList allowList, blockList;

// ── state ────────────────────────────────────────────────────────────────────
static Stats stats;
static Cache *cache;                     // on the heap: ~25 KB, too much for static memory
static uint32_t pausedUntil = 0;
static volatile bool routing = false;    // NAPT on: NoAds devices reach the internet through the board         // millis(); 0: not paused
static volatile bool updating = false;
static char updateNote[96] = "";
static char failDetail[160] = "";        // why the last download failed, for the dashboard

// ── the list in flash ────────────────────────────────────────────────────────

// Anything an interrupted update left behind (a power cut, a crash): half-made files take flash
// the next update needs.
static void removeLeftovers() {
  File root = LittleFS.open("/");
  String doomed[80];
  int n = 0;
  for (File f = root.openNextFile(); f && n < 80; f = root.openNextFile()) {
    const String name = String("/") + f.name();
    if (name.endsWith(".tmp") || name == "/list.new") doomed[n++] = name;
  }
  root.close();
  for (int i = 0; i < n; i++) LittleFS.remove(doomed[i]);
  if (n) Serial.printf("noads: removed %d leftover files\n", n);
}
static File listFile;
struct FlashReader {
  int operator()(uint32_t off, uint8_t *buf, size_t n) const {
    if (!listFile || !listFile.seek(off)) return -1;
    return listFile.read(buf, n);
  }
};
static Blocklist<FlashReader> list;
static char listWhy[96] = "";            // why the list isn't loaded, for the dashboard

static void openList() {
  xSemaphoreTake(listLock, portMAX_DELAY);
  if (listFile) listFile.close();
  list.ok = false;
  listFile = LittleFS.open("/list.bin", "r");
  listWhy[0] = 0;
  if (!listFile) snprintf(listWhy, sizeof listWhy, "no /list.bin");
  if (listFile && !list.load(listFile.size()))
    snprintf(listWhy, sizeof listWhy, "/list.bin of %u bytes did not load (header count %lu)", (unsigned)listFile.size(),
             (unsigned long)list.h.count);
  if (listFile && list.ok)
    Serial.printf("noads: blocklist of %lu names ready\n", (unsigned long)list.h.count);
  else {
    if (listFile) listFile.close();
    Serial.println("noads: no blocklist yet");
  }
  xSemaphoreGive(listLock);
}

static bool listHas(uint64_t fp) {
  xSemaphoreTake(listLock, portMAX_DELAY);
  const bool r = list.has(fp);
  xSemaphoreGive(listLock);
  return r;
}

// LittleFS for the builder.
struct LfsFS {
  bool append(const char *p, const uint8_t *d, size_t n) {
    File f = LittleFS.open(p, "a");
    if (!f) { Serial.printf("noads: could not open %s to add to it\n", p); return false; }
    const size_t w = f.write(d, n);
    f.close();
    if (w != n) Serial.printf("noads: wrote %u of %u bytes to %s\n", (unsigned)w, (unsigned)n, p);
    return w == n;
  }
  int read(const char *p, uint32_t off, uint8_t *b, size_t n) {
    File f = LittleFS.open(p, "r");
    if (!f) return -1;
    f.seek(off);
    const int got = f.read(b, n);
    f.close();
    return got;
  }
  template <class F> bool scan(const char *p, F fn) {
    File f = LittleFS.open(p, "r");
    if (!f) return false;
    static uint8_t b[1536];                // a multiple of 6: fingerprints never straddle two reads
    int n;
    while ((n = f.read(b, sizeof b)) > 0) fn(b, n);
    f.close();
    return true;
  }
  void remove(const char *p) {
    if (LittleFS.exists(p)) LittleFS.remove(p);
  }
};

static const ListChoice &currentList() {
  for (auto &l : LISTS)
    if (!strcmp(l.id, listId)) return l;
  return LISTS[0];
}

// ── settings in flash ────────────────────────────────────────────────────────
static void saveNames(const char *key, const NameList &l) {
  String s;
  for (int i = 0; i < l.n; i++) { if (i) s += '\n'; s += l.names[i]; }
  prefs.putString(key, s);
}
static void loadNames(const char *key, NameList &l) {
  memset(&l, 0, sizeof l);
  const String s = prefs.getString(key, "");
  int from = 0;
  while (from < (int)s.length()) {
    int e = s.indexOf('\n', from);
    if (e < 0) e = s.length();
    l.add(s.substring(from, e).c_str());
    from = e + 1;
  }
}
static void loadSettings() {
  prefs.begin("noads", false);
  prefs.getString("ap", "NoAds").toCharArray(apName, sizeof apName);
  String pass = prefs.getString("pass", "");
  if (pass.length() < 8) {               // first start: a password of its own, shown on the dashboard
    char p[16];
    snprintf(p, sizeof p, "noads-%04lu", (unsigned long)(esp_random() % 10000));
    pass = p;
    prefs.putString("pass", pass);
  }
  pass.toCharArray(apPass, sizeof apPass);
  prefs.getString("list", "normal").toCharArray(listId, sizeof listId);
  for (int i = 0; i < 2; i++) {
    IPAddress a;
    if (a.fromString(prefs.getString(i ? "dns2" : "dns1", ""))) upstream[i] = a;
  }
  loadNames("allow", allowList);
  loadNames("block", blockList);
}

// ── the DNS server (its own task) ────────────────────────────────────────────
static const int IN_FLIGHT = 16, QUERY_KEEP = 256;   // a home rarely has more than a few waiting
struct Flight {
  bool used;
  uint16_t upId, clientId;
  sockaddr_in client;
  Question q;
  uint8_t query[QUERY_KEEP];
  uint16_t queryLen;
  uint32_t sentAt;
  uint8_t tries;
};
static Flight *flights;                  // on the heap too
static uint8_t pkt[DNS_MAX], ans[DNS_MAX];

static void dnsTask(void *) {
  auto sendUp = [](int up, Flight &p) {
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_port = htons(53);
    to.sin_addr.s_addr = (uint32_t)upstream[p.tries % 2];
    sendto(up, p.query, p.queryLen, 0, (sockaddr *)&to, sizeof to);
    p.sentAt = millis();
    p.tries++;
  };
  // "Server failure", for a question the board can't get answered: the device then retries.
  auto failAnswer = [](int s, const uint8_t *query, const Question &q, const sockaddr_in &to) {
    memcpy(ans, query, q.end);
    wr16(ans + 2, 0x8182);
    wr16(ans + 4, 1);
    wr16(ans + 6, 0);
    wr16(ans + 8, 0);
    wr16(ans + 10, 0);
    sendto(s, ans, q.end, 0, (sockaddr *)&to, sizeof to);
  };
  int s = socket(AF_INET, SOCK_DGRAM, 0), up = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(53);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (s < 0 || up < 0 || bind(s, (sockaddr *)&a, sizeof a) < 0) {
    Serial.println("noads: DNS could not start");
    vTaskDelete(nullptr);
  }
  Serial.println("noads: DNS answering on port 53");
  for (;;) {
    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(s, &fs);
    FD_SET(up, &fs);
    timeval tv = {0, 100000};
    const int ready = select((s > up ? s : up) + 1, &fs, nullptr, nullptr, &tv);
    const uint32_t now = millis();

    if (ready > 0 && FD_ISSET(s, &fs)) {                 // a question from a device
      sockaddr_in from;
      socklen_t fl = sizeof from;
      const int n = recvfrom(s, pkt, sizeof pkt, 0, (sockaddr *)&from, &fl);
      Question q;
      if (n > 0 && parseQuery(pkt, n, q)) {
        xSemaphoreTake(lock, portMAX_DELAY);
        const bool paused = pausedUntil && (int32_t)(pausedUntil - now) > 0;
        const Verdict v = decide(q.name, allowList, blockList, listHas, paused);   // one ~3 KB flash read at most
        xSemaphoreGive(lock);
        const bool blocked = v == BLOCK_LIST || v == BLOCK_YOURS || v == BLOCK_SPECIAL;
        if (blocked) {
          const int m = blockedAnswer(pkt, q, ans, v == BLOCK_SPECIAL);
          sendto(s, ans, m, 0, (sockaddr *)&from, sizeof from);
          xSemaphoreTake(lock, portMAX_DELAY);
          record(stats, q.name, from.sin_addr.s_addr, v, false, now);
          xSemaphoreGive(lock);
        } else {
          xSemaphoreTake(lock, portMAX_DELAY);
          const int m = (cache ? cache->get(q, now, ans) : 0);
          if (m) record(stats, q.name, from.sin_addr.s_addr, v, true, now);
          xSemaphoreGive(lock);
          if (m) sendto(s, ans, m, 0, (sockaddr *)&from, sizeof from);
          else {
            Flight *p = nullptr;
            for (int fi = 0; fi < IN_FLIGHT; fi++)
              if (!flights[fi].used) { p = &flights[fi]; break; }
            if (!p || n > QUERY_KEEP) failAnswer(s, pkt, q, from);   // too busy: say so at once
            else {
              p->used = true;
              p->client = from;
              p->clientId = q.id;
              p->q = q;
              p->upId = (uint16_t)esp_random();          // a fresh, unguessable id for the upstream leg
              memcpy(p->query, pkt, n);
              wr16(p->query, p->upId);
              p->queryLen = n;
              p->tries = 0;
              sendUp(up, *p);
            }
          }
        }
      }
    }

    if (ready > 0 && FD_ISSET(up, &fs)) {                // an answer from the real DNS server
      sockaddr_in from;
      socklen_t fl = sizeof from;
      const int n = recvfrom(up, ans, sizeof ans, 0, (sockaddr *)&from, &fl);
      const bool known = from.sin_addr.s_addr == (uint32_t)upstream[0] || from.sin_addr.s_addr == (uint32_t)upstream[1];
      if (n >= 12 && known && ntohs(from.sin_port) == 53) {
        const uint16_t id = rd16(ans);
        for (int fi = 0; fi < IN_FLIGHT; fi++) {
          Flight &p = flights[fi];
          if (!p.used || p.upId != id) continue;
          wr16(ans, p.clientId);
          sendto(s, ans, n, 0, (sockaddr *)&p.client, sizeof p.client);
          xSemaphoreTake(lock, portMAX_DELAY);
          if (cache) cache->put(p.q, ans, n, now);
          record(stats, p.q.name, p.client.sin_addr.s_addr, ALLOW, false, now);
          xSemaphoreGive(lock);
          p.used = false;
          break;
        }
      }
    }

    for (int fi = 0; fi < IN_FLIGHT; fi++) {             // slow answers: the other server, then give up
      Flight &p = flights[fi];
      if (!p.used || now - p.sentAt < 1500) continue;
      if (p.tries < 2) sendUp(up, p);
      else {
        failAnswer(s, p.query, p.q, p.client);
        xSemaphoreTake(lock, portMAX_DELAY);
        stats.failed++;
        xSemaphoreGive(lock);
        p.used = false;
      }
    }
  }
}

// ── updating the list ────────────────────────────────────────────────────────
// A secure download needs ~50 KB, and with NoAds routing, DNS and the dashboard running the board
// was left with blocks of 6 KB, so downloads broke off. So updates run in a restart of their own:
// home WiFi only, nothing else, all the memory. NoAds is gone for ~3 minutes; phones fall back to
// the home WiFi by themselves. The result is kept in flash for the dashboard after the next start.
static void runUpdate() {
  {
    updating = true;
    const ListChoice &lc = currentList();
    Serial.printf("noads: updating the list from %s\n", lc.url);
    snprintf(updateNote, sizeof updateNote, "Downloading...");

    NetworkClientSecure tls;
    tls.setCACertBundle(crtBundleStart, crtBundleEnd - crtBundleStart);
    HTTPClient http;
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    long result = -1;
    Serial.printf("noads: %lu bytes free, largest block %lu, before downloading\n", (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMaxAllocHeap());
    char memNote[64];
    snprintf(memNote, sizeof memNote, "start: %lu free, largest %lu", (unsigned long)ESP.getFreeHeap(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    const int code = http.begin(tls, lc.url) ? http.GET() : -1;
    snprintf(failDetail, sizeof failDetail, "%s; connected: %lu free, largest %lu", memNote,
             (unsigned long)ESP.getFreeHeap(), (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (code == 200) {
      const int size = http.getSize();
      // Room: the old list stays in use while the new one is built, if both fit. If not, the old
      // one goes first (blocking from the list stops for the minute or so it takes).
      const size_t old = LittleFS.exists("/list.bin") ? LittleFS.open("/list.bin", "r").size() : 0;
      const size_t estimate = size > 0 ? (size_t)size / 20 * FP_BYTES * 12 / 10 : 1200000;
      if (LittleFS.totalBytes() - LittleFS.usedBytes() < estimate * 2 + 65536 && old) {
        Serial.println("noads: not enough room for two lists; the old one goes first");
        xSemaphoreTake(listLock, portMAX_DELAY);
        if (listFile) listFile.close();
        list.ok = false;
        LittleFS.remove("/list.bin");
        xSemaphoreGive(listLock);
      }
      removeLeftovers();
      // Reserved once, at start: 13 KB in one piece is not always to be had later on, with the
      // secure connection open and memory in pieces.
      static LfsFS fs;
      static Builder<LfsFS> builder(fs);
      Builder<LfsFS> *b = &builder;
      b->begin();
      NetworkClient *st = http.getStreamPtr();
      static uint8_t buf[1460];
      uint32_t got = 0, quiet = millis();
      while (http.connected() && (size < 0 || got < (uint32_t)size) && millis() - quiet < 20000) {
        const int avail = st->available();
        if (avail <= 0) { delay(2); continue; }
        const int r = st->read(buf, avail < (int)sizeof buf ? avail : sizeof buf);
        if (r <= 0) continue;
        b->feed(buf, r);
        got += r;
        quiet = millis();
        if (b->failed) break;
        snprintf(updateNote, sizeof updateNote, "Downloading... %lu KB", (unsigned long)(got / 1024));
      }
      http.end();
      if (size > 0 && got < (uint32_t)size) Serial.println("noads: the download stopped short");
      else {
        snprintf(updateNote, sizeof updateNote, "Sorting %lu names...", (unsigned long)b->names);
        result = b->finish((uint32_t)time(nullptr));
      }
      if (result < 0) b->removeBuckets();
      if (result < 0 && b->failed) {
        snprintf(updateNote, sizeof updateNote, "Building the list failed");
        snprintf(failDetail, sizeof failDetail, "%s / flash %lu of %lu KB used / largest block %lu", b->why,
                 (unsigned long)(LittleFS.usedBytes() / 1024), (unsigned long)(LittleFS.totalBytes() / 1024),
                 (unsigned long)ESP.getMaxAllocHeap());
      }
    } else {
      char tlsErr[64] = "";
      tls.lastError(tlsErr, sizeof tlsErr);
      snprintf(failDetail, sizeof failDetail, "%s; error %d: %.60s", memNote, code, tlsErr);
      Serial.printf("noads: download failed: %s\n", failDetail);
      http.end();
    }
    if (result > 0) {
      xSemaphoreTake(listLock, portMAX_DELAY);
      if (listFile) listFile.close();
      list.ok = false;
      LittleFS.remove("/list.bin");
      bool moved = false;
      for (int i = 0; i < 5 && !(moved = LittleFS.rename("/list.new", "/list.bin")); i++) delay(200);
      if (!moved) Serial.println("noads: could not rename /list.new");
      xSemaphoreGive(listLock);
      openList();
      snprintf(updateNote, sizeof updateNote, "Updated: %ld names", result);
    } else {
      openList();                                // the old list, if it's still there
      if (!strstr(updateNote, "Building"))
        snprintf(updateNote, sizeof updateNote, code == 200 ? "The last update stopped short"
                 : code < 0 ? "The last update could not reach the list"
                 : "The list's server answered %d", code);
    }
    Serial.printf("noads: %s\n", updateNote);
    prefs.putString("note", updateNote);
    prefs.putString("detail", result > 0 ? "" : failDetail);
    prefs.putUInt("tried", (uint32_t)time(nullptr));
    updating = false;
  }
}

static void updateTaskOnce(void *done) {
  runUpdate();
  xSemaphoreGive((SemaphoreHandle_t)done);
  vTaskDelete(nullptr);
}

// Asks for an update: the board restarts into update mode a moment later.
static uint32_t restartAt = 0;
static void requestUpdate() {
  prefs.putBool("update", true);
  snprintf(updateNote, sizeof updateNote, "Restarting to update the list: back in about 3 minutes");
  restartAt = millis() + 1500;
}

// ── the dashboard ────────────────────────────────────────────────────────────
static char reply[12288];

static const char *arg(const char *key) {
  static String held[3];
  static int at = 0;
  at = (at + 1) % 3;
  held[at] = web.arg(key);
  return held[at].c_str();
}

static void sendReply(const Out &o) {
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", reply);
  (void)o;
}

static void stateJson(Out &o) {
  const uint32_t now = millis();
  const bool paused = pausedUntil && (int32_t)(pausedUntil - now) > 0;
  o.add("{\"ok\":true,\"paused\":%lu,\"updating\":%s,\"note\":", paused ? (unsigned long)((pausedUntil - now) / 1000) : 0UL,
        updating ? "true" : "false");
  o.str(updateNote);
  o.add(",\"detail\":");
  o.str(failDetail);
  o.add(",\"listWhy\":");
  o.str(listWhy);
  o.add(",\"list\":{\"id\":\"%s\",\"count\":%lu,\"built\":%lu},\"lists\":[", listId, list.ok ? (unsigned long)list.h.count : 0UL,
        list.ok ? (unsigned long)list.h.built : 0UL);
  for (size_t i = 0; i < sizeof LISTS / sizeof LISTS[0]; i++) {
    o.add(i ? ",[\"%s\"," : "[\"%s\",", LISTS[i].id);
    o.str(LISTS[i].name);
    o.add("]");
  }
  o.add("],\"ap\":");
  o.str(apName);
  o.add(",\"pass\":");
  o.str(apPass);
  o.add(",\"apIp\":\"%s\",\"homeIp\":\"%s\",\"home\":", WiFi.softAPIP().toString().c_str(), WiFi.localIP().toString().c_str());
  o.str(WiFi.SSID().c_str());
  o.add(",\"routing\":%s", routing ? "true" : "false");
  o.add(",\"devices\":%d,\"dns\":[\"%s\",\"%s\"],\"allow\":[", WiFi.softAPgetStationNum(), upstream[0].toString().c_str(),
        upstream[1].toString().c_str());
  for (int i = 0; i < allowList.n; i++) { if (i) o.add(","); o.str(allowList.names[i]); }
  o.add("],\"block\":[");
  for (int i = 0; i < blockList.n; i++) { if (i) o.add(","); o.str(blockList.names[i]); }
  o.add("],\"flash\":[%lu,%lu],\"free\":%lu,\"largest\":%lu,\"minFree\":%lu,\"up\":%lu,\"reset\":%d,",
        (unsigned long)LittleFS.usedBytes(), (unsigned long)LittleFS.totalBytes(), (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getMaxAllocHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)(now / 1000),
        (int)esp_reset_reason());
  statsJson(o, stats, now);
  o.add("}");
}

static void handle() {
  const String path = web.uri();
  Out o = {reply, sizeof reply, 0};
  auto fail = [&](const char *why) { o.add("{\"ok\":false,\"err\":\"%s\"}", why); };
  xSemaphoreTake(lock, portMAX_DELAY);
  if (path == "/state") stateJson(o);
  else if (path == "/pause") {
    const int mins = atoi(arg("min"));
    pausedUntil = mins > 0 ? millis() + (uint32_t)(mins > 720 ? 720 : mins) * 60000 : 0;
    stateJson(o);
  } else if (path == "/allow" || path == "/block") {
    NameList &l = path == "/allow" ? allowList : blockList;
    const bool add = strcmp(arg("do"), "remove") != 0;
    if (add ? !l.add(arg("name")) : !l.remove(arg("name"))) fail(add ? (l.n >= USER_NAMES ? "full" : "name") : "gone");
    else {
      saveNames(path == "/allow" ? "allow" : "block", l);
      if (cache) cache->clear();
      stateJson(o);
    }
  } else if (path == "/lookup") {
    char nm[NAME_MAX_LEN + 2];
    if (!normalizeName(arg("name"), nm, sizeof nm)) fail("name");
    else {
      const bool paused = pausedUntil && (int32_t)(pausedUntil - millis()) > 0;
      const Verdict v = decide(nm, allowList, blockList, listHas, paused);
      o.add("{\"ok\":true,\"name\":");
      o.str(nm);
      o.add(",\"verdict\":%d}", v);
    }
  } else if (path == "/settings") {
    bool changed = false;
    const char *ap = arg("ap"), *pass = arg("pass"), *lid = arg("list");
    if (*ap) {
      if (strlen(ap) > 32) { fail("ap"); goto done; }
      strcpy(apName, ap);
      prefs.putString("ap", apName);
      changed = true;
    }
    if (*pass) {
      if (strlen(pass) < 8 || strlen(pass) > 63) { fail("pass"); goto done; }
      strcpy(apPass, pass);
      prefs.putString("pass", apPass);
      changed = true;
    }
    if (*lid) {
      bool known = false;
      for (auto &l : LISTS) known |= !strcmp(l.id, lid);
      if (!known) { fail("list"); goto done; }
      if (strcmp(lid, listId)) {
        strcpy(listId, lid);
        prefs.putString("list", listId);
        requestUpdate();
      }
    }
    const char *tz = arg("tz");                  // the browser's offset from UTC, for the quiet hour
    if (*tz) prefs.putInt("tz", atoi(tz));
    for (int i = 0; i < 2; i++) {
      const char *d = arg(i ? "dns2" : "dns1");
      IPAddress a;
      if (*d && a.fromString(d)) { upstream[i] = a; prefs.putString(i ? "dns2" : "dns1", d); }
    }
    stateJson(o);
    if (changed) {
      // the new name or password takes effect after the reply is out
      xSemaphoreGive(lock);
      sendReply(o);
      delay(300);
      ESP.restart();                           // NoAds comes back with the new name or password
      return;
    }
  } else if (path == "/update") {
    if (restartAt) fail("busy");
    else {
      requestUpdate();
      stateJson(o);
    }
  } else fail("what");
done:
  xSemaphoreGive(lock);
  sendReply(o);
}

// ── the networks ─────────────────────────────────────────────────────────────

// NoAds is made the way the ESP32 core's WiFiExtender example makes it: the network first, then
// the home WiFi joined, and routing (NAPT) switched on each time the home connection gets an
// address. Made the other way round (home WiFi first, then NoAds) the board passed nothing on:
// phones could reach the board itself but not the router (tested 2026-11-26).
static void startNoAds() {
  Network.onEvent([](arduino_event_id_t e, arduino_event_info_t) {
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      routing = WiFi.AP.enableNAPT(true);
      Serial.printf("noads: routing %s\n", routing ? "on" : "did not start");
    } else if (e == ARDUINO_EVENT_WIFI_STA_LOST_IP || e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      if (routing) WiFi.AP.enableNAPT(false);
      routing = false;
    }
  });
  WiFi.AP.begin();
  // The board is NoAds' gateway and its DNS server: phones get both from it automatically.
  WiFi.AP.config(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0),
                 IPAddress(192, 168, 4, 2), IPAddress(192, 168, 4, 1));
  if (!WiFi.AP.create(apName, apPass, 1, 0, 8)) { Serial.println("noads: NoAds network did not start"); return; }
  WiFi.AP.waitStatusBits(ESP_NETIF_STARTED_BIT, 1000);
  Serial.printf("noads: network \"%s\" is up\n", apName);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nnoads: starting");
  lock = xSemaphoreCreateMutex();
  listLock = xSemaphoreCreateMutex();
  memset(&stats, 0, sizeof stats);
  if (!LittleFS.begin(true)) Serial.println("noads: flash files could not be mounted");
  removeLeftovers();
  loadSettings();

  // Update mode: asked for before the restart, or there is no list at all yet. With no list, one
  // try at start; if that fails, loop() asks again every 10 minutes (never a loop of restarts).
  openList();
  const bool wanted = prefs.getBool("update", false);
  const bool updateMode = wanted || (!list.ok && !prefs.getBool("noRetry", false));
  if (!updateMode) startNoAds();             // before the home WiFi: see startNoAds()

  // false: the board's own setup network is only for choosing your WiFi; NoAds needs it to work.
  const wifisetup::Config cfg = {HOST, "devkit-noads", "devkit-noads", "14", false};
  if (!wifisetup::begin(cfg, web)) return;   // the setup page runs from loop() until WiFi is chosen
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  if (updateMode) {
    prefs.putBool("update", false);
    prefs.putBool("noRetry", true);
    for (int i = 0; i < 50 && time(nullptr) < 1700000000; i++) delay(100);   // the clock, for "built"
    Serial.println("noads: update mode: home WiFi only, all memory for the download");
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(updateTaskOnce, "update", 12288, done, 1, nullptr, 1);
    xSemaphoreTake(done, portMAX_DELAY);
    delay(300);
    ESP.restart();
  }
  if (list.ok) prefs.putBool("noRetry", false);
  prefs.getString("note", "").toCharArray(updateNote, sizeof updateNote);
  prefs.getString("detail", "").toCharArray(failDetail, sizeof failDetail);
  cache = new Cache();
  cache->clear();
  flights = new Flight[IN_FLIGHT]();

  web.on("/", [] {
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html; charset=utf-8", NOADS_PAGE);
  });
  for (const char *p : {"/state", "/pause", "/allow", "/block", "/lookup", "/settings", "/update"}) web.on(p, handle);
  web.on("/favicon.ico", [] { web.send(204); });
  web.onNotFound([] { web.send(404, "text/plain", "not here"); });
  web.begin();

  xTaskCreatePinnedToCore(dnsTask, "dns", 6144, nullptr, 3, nullptr, 1);
  Serial.printf("noads: dashboard at http://%s.local or http://%s (home WiFi), http://192.168.4.1 (on NoAds)\n", HOST,
                WiFi.localIP().toString().c_str());
}

void loop() {
  wifisetup::loop();
  if (!wifisetup::online() && wifisetup::portal()) { delay(5); return; }
  web.handleClient();
  const uint32_t now = millis();
  if (restartAt && (int32_t)(now - restartAt) >= 0) ESP.restart();
  // When to update: with no list, every 10 minutes until one comes; with a list over 7 days old,
  // in the quiet hours (3 to 5 in the morning, by the time of the browser that last opened the
  // dashboard), at most once a day.
  static uint32_t lastCheck = 0;
  if (now - lastCheck > 60000) {
    lastCheck = now;
    const time_t t = time(nullptr);
    const bool clockOk = t > 1700000000;
    const uint32_t tried = prefs.getUInt("tried", 0);
    if (!restartAt) {
      if (!list.ok && now > 600000) requestUpdate();
      else if (list.ok && clockOk && (uint32_t)t - list.h.built > 7 * 86400 && (uint32_t)t - tried > 20 * 3600) {
        const int localHour = (int)(((int64_t)t + (int64_t)prefs.getInt("tz", 0) * 60) / 3600 % 24);
        if (localHour >= 3 && localHour < 5) requestUpdate();
      }
    }
    Serial.printf("noads: %lu lookups, %lu blocked, %d devices on NoAds, %lu bytes free (largest %lu)\n",
                  (unsigned long)stats.total, (unsigned long)stats.blocked, WiFi.softAPgetStationNum(),
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  }
  delay(2);
}
