#pragma once
// The board's side of clip.h: flash storage, the index, the clock, and the two web servers.
// In a header rather than clip.ino, because Arduino writes a prototype for every function in a
// .ino, and those land above the types they use.

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_https_server.h>
#include <esp_random.h>
#include <esp_log.h>
#include <string>
#include <vector>
#include "clip.h"
#include "clip_page.h"
#include "files.h"
#include "cert.h"

static SemaphoreHandle_t lock;          // clip.h runs on two server tasks and loop()
static char reply[REPLY_MAX];
static String hosts;                    // filled into the installers: "clip.local 192.168.1.20"
static httpd_handle_t plainServer = nullptr, secureServer = nullptr;

static uint32_t hwRand() { return esp_random(); }

static uint32_t unixTime() {
  const time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;   // 0 until NTP has answered
}

// ── items on flash ──────────────────────────────────────────────────────────────────────────

static void itemPath(uint32_t id, char *p) { snprintf(p, 24, "/i/%lu", (unsigned long)id); }

struct FlashStore : Store {
  int read(uint32_t id, uint8_t *buf, int max) override {
    char p[24];
    itemPath(id, p);
    File f = LittleFS.open(p, "r");
    if (!f) return -1;
    const int n = f.read(buf, max);
    f.close();
    return n;
  }
  void remove(uint32_t id) override {
    char p[24];
    itemPath(id, p);
    LittleFS.remove(p);
  }
  void removeAll() override {
    File d = LittleFS.open("/i");
    if (d) {
      std::vector<String> names;
      for (File f = d.openNextFile(); f; f = d.openNextFile()) names.push_back(String("/i/") + f.name());
      d.close();
      for (auto &n : names) LittleFS.remove(n);
    }
  }
};
static FlashStore flashStore;

static Saved saved;                     // ~4 KB; static so it isn't on a task's stack
static uint32_t dirtySince = 0;

static void loadIndex() {
  LittleFS.mkdir("/i");
  File f = LittleFS.open("/index.bin", "r");
  bool ok = false;
  if (f && f.size() == sizeof saved && f.read((uint8_t *)&saved, sizeof saved) == sizeof saved) ok = loadFrom(saved, millis());
  if (f) f.close();
  if (!ok) {
    Serial.println("clip: no saved group; starting fresh");
    resetAll();                         // also clears any item files left without an index
  } else {
    // Files with no item in the index (a restart in the middle of a send) are removed.
    File d = LittleFS.open("/i");
    std::vector<String> orphans;
    for (File x = d.openNextFile(); x; x = d.openNextFile())
      if (!itemById((uint32_t)strtoul(x.name(), nullptr, 10))) orphans.push_back(String("/i/") + x.name());
    d.close();
    for (auto &o : orphans) LittleFS.remove(o);
    unsigned n = 0;
    for (auto &i : items) n += i.used;
    Serial.printf("clip: group loaded, %u items\n", n);
  }
}

// Written a couple of seconds after the last change, so a burst of copies is one flash write.
static void saveIndexIfDirty(uint32_t now, bool force = false) {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (!dirty) { dirtySince = 0; xSemaphoreGive(lock); return; }
  if (!dirtySince) dirtySince = now;
  if (!force && now - dirtySince < 2000) { xSemaphoreGive(lock); return; }
  saveTo(saved);
  xSemaphoreGive(lock);
  dirtySince = 0;
  File f = LittleFS.open("/index.tmp", "w");
  if (!f) return;
  const bool whole = f.write((const uint8_t *)&saved, sizeof saved) == sizeof saved;
  f.close();
  if (whole) LittleFS.rename("/index.tmp", "/index.bin");   // the old index stays until the new one is whole
}

static unsigned itemCount() { unsigned n = 0; for (auto &i : items) n += i.used; return n; }
static unsigned onlineCount() { unsigned n = 0; for (auto &d : devices) n += d.used && d.online; return n; }

// ── requests ────────────────────────────────────────────────────────────────────────────────

static std::string urlDecode(const char *s, size_t n) {
  std::string o;
  o.reserve(n);
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < n) { const char h[3] = {s[i + 1], s[i + 2], 0}; o += (char)strtol(h, nullptr, 16); i += 2; }
    else o += s[i];
  }
  return o;
}

struct Args {
  std::vector<std::pair<std::string, std::string>> kv;
  void parse(const char *q, size_t n) {
    size_t a = 0;
    while (a < n) {
      size_t e = a;
      while (e < n && q[e] != '&') e++;
      size_t eq = a;
      while (eq < e && q[eq] != '=') eq++;
      if (eq < e) kv.emplace_back(urlDecode(q + a, eq - a), urlDecode(q + eq + 1, e - eq - 1));
      a = e + 1;
    }
  }
  const char *get(const char *k) const {
    for (auto &p : kv) if (p.first == k) return p.second.c_str();
    return "";
  }
};

static esp_err_t sendJson(httpd_req_t *r, const std::string &s) {
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_send(r, s.data(), s.size());
}
static std::string jsonErr(const char *e) { return std::string("{\"ok\":false,\"err\":\"") + e + "\"}"; }

// A served file, with the board's own addresses put into the installers.
static esp_err_t sendFile(httpd_req_t *r, const char *type, const char *text, bool withHosts) {
  httpd_resp_set_type(r, type);
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  if (!withHosts) return httpd_resp_send(r, text, HTTPD_RESP_USE_STRLEN);
  String s(text);
  s.replace("%%HOSTS%%", hosts);
  return httpd_resp_send(r, s.c_str(), s.length());
}

// /put: the sealed bytes are the body. Checked first, written to a temporary file as they arrive
// (an image is bigger than the board's free memory), then given their id.
static esp_err_t doPut(httpd_req_t *r, const Args &a) {
  const uint32_t tok = (uint32_t)strtoul(a.get("t"), nullptr, 16);
  const int flags = atoi(a.get("f")), to = atoi(a.get("to"));
  const size_t n = r->content_len;
  xSemaphoreTake(lock, portMAX_DELAY);
  Device *d = deviceByToken(tok);
  const PutResult p = d ? canPut(*d, n, to, flags) : P_NOTO;
  if (d) d->seen = millis();
  xSemaphoreGive(lock);
  if (!d) return sendJson(r, jsonErr("notin"));
  if (p != P_OK) return sendJson(r, jsonErr(PUT_TEXT[p]));
  char tmp[24];
  snprintf(tmp, sizeof tmp, "/up%d.tmp", httpd_req_to_sockfd(r));
  File f = LittleFS.open(tmp, "w");
  if (!f) return sendJson(r, jsonErr("flash"));
  static uint8_t buf[2][2048];          // one per server task
  uint8_t *b = buf[r->handle == secureServer ? 1 : 0];
  size_t got = 0;
  bool ok = true;
  while (got < n) {
    const int k = httpd_req_recv(r, (char *)b, min(sizeof buf[0], n - got));
    if (k == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (k <= 0) { ok = false; break; }
    if (f.write(b, k) != (size_t)k) { ok = false; break; }
    got += k;
  }
  f.close();
  if (!ok) { LittleFS.remove(tmp); return sendJson(r, jsonErr("flash")); }
  xSemaphoreTake(lock, portMAX_DELAY);
  d = deviceByToken(tok);
  Item *it = nullptr;
  if (d) {
    // addItem may first delete older items to make room; then the file takes the new id.
    const uint32_t id = nextItemId();
    char path[24];
    itemPath(id, path);
    it = addItem(*d, n, flags, to, millis(), unixTime());
    if (it && !LittleFS.rename(tmp, path)) { removeItem(*it); it = nullptr; }
  }
  const uint32_t id = it ? it->id : 0;
  xSemaphoreGive(lock);
  if (!it) { LittleFS.remove(tmp); return sendJson(r, jsonErr(d ? "full" : "notin")); }
  return sendJson(r, "{\"ok\":true,\"id\":" + std::to_string(id) + "}");
}

static esp_err_t doBlob(httpd_req_t *r, const Args &a) {
  const uint32_t tok = (uint32_t)strtoul(a.get("t"), nullptr, 16), id = (uint32_t)strtoul(a.get("id"), nullptr, 10);
  xSemaphoreTake(lock, portMAX_DELAY);
  const int size = blobFor(tok, id, millis());
  xSemaphoreGive(lock);
  char p[24];
  itemPath(id, p);
  File f = size >= 0 ? LittleFS.open(p, "r") : File();
  if (!f) return sendJson(r, jsonErr("gone"));
  httpd_resp_set_type(r, "application/octet-stream");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  static uint8_t buf[2][2048];
  uint8_t *b = buf[r->handle == secureServer ? 1 : 0];
  bool ok = true;
  for (int k; (k = f.read(b, sizeof buf[0])) > 0;)
    if (httpd_resp_send_chunk(r, (const char *)b, k) != ESP_OK) { ok = false; break; }
  f.close();
  httpd_resp_send_chunk(r, nullptr, 0);
  if (ok) {
    xSemaphoreTake(lock, portMAX_DELAY);
    taken(tok, id);                     // a one-time item that reached another device is gone
    xSemaphoreGive(lock);
  }
  return ESP_OK;
}

static esp_err_t onRequest(httpd_req_t *r) {
  const char *q = strchr(r->uri, '?');
  const std::string path(r->uri, q ? q - r->uri : strlen(r->uri));
  if (path == "/") {                      // kept gzipped to fit in flash; every browser unpacks it
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, (const char *)CLIP_PAGE_GZ, CLIP_PAGE_GZ_LEN);
  }
  if (path == "/favicon.ico") { httpd_resp_set_status(r, "204 No Content"); return httpd_resp_send(r, nullptr, 0); }
  if (path == "/devkit_clip.py.gz") { httpd_resp_set_type(r, "application/gzip"); return httpd_resp_send(r, (const char *)CLIP_PY_GZ, CLIP_PY_GZ_LEN); }
  if (path == "/clipcrypto.py.gz") { httpd_resp_set_type(r, "application/gzip"); return httpd_resp_send(r, (const char *)CRYPTO_PY_GZ, CRYPTO_PY_GZ_LEN); }
  if (path == "/i") return sendFile(r, "text/plain; charset=utf-8", INSTALL_SH, true);
  if (path == "/u") return sendFile(r, "text/plain; charset=utf-8", UNINSTALL_SH, false);
  if (path == "/i.ps1") return sendFile(r, "text/plain; charset=utf-8", INSTALL_PS1, true);
  if (path == "/u.ps1") return sendFile(r, "text/plain; charset=utf-8", UNINSTALL_PS1, false);
  Args a;
  if (q) a.parse(q + 1, strlen(q + 1));
  if (path == "/put") return doPut(r, a);
  if (path == "/blob") return doBlob(r, a);
  const size_t n = r->content_len;
  if (n > 4096) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "too long");
  if (n) {
    std::string body(n, '\0');
    size_t got = 0;
    while (got < n) {
      const int k = httpd_req_recv(r, &body[got], n - got);
      if (k == HTTPD_SOCK_ERR_TIMEOUT) continue;
      if (k <= 0) return ESP_FAIL;
      got += k;
    }
    a.parse(body.data(), n);
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  Out o = {reply, sizeof reply, 0};
  reply[0] = 0;
  route(path.c_str(), [&](const char *k) { return a.get(k); }, millis(), unixTime(), o);
  std::string s(reply, o.n);
  xSemaphoreGive(lock);
  return sendJson(r, s);
}

static void quietTls() {
  // A phone still on the "not private" warning ends handshakes early; that's normal (chapter 12).
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);
  esp_log_level_set("esp-tls", ESP_LOG_NONE);
  esp_log_level_set("esp_https_server", ESP_LOG_NONE);
  esp_log_level_set("httpd", ESP_LOG_NONE);
}

static void startServers() {
  static const httpd_uri_t get = {"/*", HTTP_GET, onRequest, nullptr};
  static const httpd_uri_t post = {"/*", HTTP_POST, onRequest, nullptr};
  // https for phones, first, while every socket and the most memory are free.
  if (!cert::ready("clip", WiFi.localIP())) return;
  httpd_ssl_config_t sc = HTTPD_SSL_CONFIG_DEFAULT();
  sc.servercert = (const uint8_t *)cert::pem.c_str();
  sc.servercert_len = cert::pem.length() + 1;
  sc.prvtkey_pem = (const uint8_t *)cert::key.c_str();
  sc.prvtkey_len = cert::key.length() + 1;
  sc.httpd.max_open_sockets = 3;          // ~37 KB each; see chapter 12
  sc.httpd.lru_purge_enable = true;
  sc.httpd.uri_match_fn = httpd_uri_match_wildcard;
  sc.httpd.stack_size = 8192;
  // No session tickets: this core's TLS is built without server tickets, and asking for them
  // stops the whole https server from starting.
  sc.tls_handshake_timeout_ms = 5000;
  const esp_err_t e = httpd_ssl_start(&secureServer, &sc);
  if (e == ESP_OK) {
    quietTls();                           // only once it runs, so a failure above is still logged
    httpd_register_uri_handler(secureServer, &get);
    httpd_register_uri_handler(secureServer, &post);
    MDNS.addService("https", "tcp", 443);
    Serial.printf("clip: https://%s.local for phones, http://%s.local for the computer helpers\n", "clip", "clip");
  } else Serial.printf("clip: https did not start (%s)\n", esp_err_to_name(e));
  // Plain http for the helpers.
  httpd_config_t pc = HTTPD_DEFAULT_CONFIG();
  pc.server_port = 80;
  pc.ctrl_port = 32770;                   // https has 32769
  pc.max_open_sockets = 6;                // with https's 3 and each server's own 3: 15 of lwIP's 16
  pc.lru_purge_enable = true;
  pc.uri_match_fn = httpd_uri_match_wildcard;
  pc.stack_size = 8192;
  if (httpd_start(&plainServer, &pc) == ESP_OK) {
    httpd_register_uri_handler(plainServer, &get);
    httpd_register_uri_handler(plainServer, &post);
  } else Serial.println("clip: http did not start");
}
