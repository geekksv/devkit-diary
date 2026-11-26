// Talk across languages: a room where everyone speaks and reads in their own language.
//
// Phones listen with their own speech recognition, translate (straight from the phone, through
// Google or MyMemory) and speak with their own voices. The board keeps the room: who is in it
// and in which language, every message, the translations phones share so nobody asks twice,
// and the "speaking now" lines for live captions. All of that is in talk.h, tested on a laptop.
//
// This file connects it to two web servers:
//   - https://talk.local (port 443), because phones only allow the microphone on https pages.
//     The board makes its own certificate on its first boot (cert.h).
//   - http://talk.local (port 80), for screens that only read: the caption screen on a TV, or
//     a phone that types. No warning, and lighter on the board's memory.
// Both serve the same page and answer through the same route(), one request at a time.
//
// Home WiFi with internet is needed (phones need it to listen and translate). With no saved
// network, the board opens devkit-talk to set one up.
//
// Board: ESP32 DevKit V1, nothing attached. Default partition scheme.

// ESP-IDF's https server first: it defines HTTP_ANY too, and Arduino's WebServer needs its own.
#include <esp_https_server.h>
#undef HTTP_ANY
#include <WiFi.h>
#include <WebServer.h>
#include <esp_random.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <vector>
#include <atomic>
#include <string>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "cert.h"
#include "talk.h"
#include "talk_page.h"
#include "serve.h"

const char *HOST = "talk";
// Each open https connection holds ~70 KB (measured in chapter 13 with /health): the TLS library
// in this ESP32 core has fixed 16 KB buffers each way, which a sketch cannot change. Two fit; a
// third failed to set up (-0x7F00) while the server still counted a free slot. At two, a third
// phone closes the least recently used connection and gets in. Connections quiet for a few
// seconds close too, so a phone in someone's pocket does not hold memory.
const int SECURE_SOCKETS = 2;
const uint32_t SECURE_IDLE_MS = 4000;     // the page polls every 0.8-2 s on screen
static std::atomic<uint32_t> secureRequests{0};   // counted on the https task, read on loop()

WebServer web(80);
static httpd_handle_t secure = nullptr;

// When each secure connection last did something, by socket number.
static std::atomic<uint32_t> secureLastUse[64];
static void noteSecureUse(int fd) { secureLastUse[fd & 63] = millis() | 1; }
static esp_err_t onSecureOpen(httpd_handle_t, int fd) { noteSecureUse(fd); return ESP_OK; }
static void closeQuietSecure() {
  if (!secure) return;
  size_t n = 8;
  int fds[8];
  if (httpd_get_client_list(secure, &n, fds) != ESP_OK) return;
  const uint32_t now = millis();
  for (size_t i = 0; i < n; i++) {
    const uint32_t t = secureLastUse[fds[i] & 63];
    if (t && now - t > SECURE_IDLE_MS) { secureLastUse[fds[i] & 63] = 0; httpd_sess_trigger_close(secure, fds[i]); }
  }
}

// Refused secure connections, for /health: noMemory must stay 0.
static std::atomic<uint32_t> tlsOpened{0}, tlsNoMemory{0}, tlsAbandoned{0};
static void onHttpsEvent(void *, esp_event_base_t, int32_t id, void *data) {
  if (id == HTTPS_SERVER_EVENT_ON_CONNECTED) tlsOpened++;
  else if (id == HTTPS_SERVER_EVENT_ERROR && data) {
    const esp_https_server_last_error_t *e = (const esp_https_server_last_error_t *)data;
    const int c = e->esp_tls_error_code < 0 ? -e->esp_tls_error_code : e->esp_tls_error_code;
    if (c == 0x7F00 || e->last_error == ESP_ERR_MBEDTLS_SSL_SETUP_FAILED || e->last_error == ESP_ERR_NO_MEM) tlsNoMemory++;
    else tlsAbandoned++;
  }
}
static int healthJson(char *b, size_t n) {
  return snprintf(b, n,
                  "{\"ok\":true,\"up\":%lu,\"boot\":%d,\"free\":%u,\"minFree\":%u,\"largest\":%u,"
                  "\"tls\":{\"opened\":%u,\"noMemory\":%u,\"abandoned\":%u}}",
                  (unsigned long)(millis() / 1000), (int)esp_reset_reason(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)tlsOpened.load(), (unsigned)tlsNoMemory.load(), (unsigned)tlsAbandoned.load());
}
// ── http, port 80 (Arduino's WebServer, on loop()) ──────────────────────────

static const char *webArg(const char *key) {
  static String held[6];                 // route() may hold a few at once
  static int at = 0;
  at = (at + 1) % 6;
  held[at] = web.arg(key);
  return held[at].c_str();
}

static void webApi() {
  web.sendHeader("Cache-Control", "no-store");
  if (web.uri() == "/health") {
    healthJson(replies[0], REPLY_MAX);
    web.send_P(200, "application/json", replies[0], strlen(replies[0]));
    return;
  }
  const size_t n = answer(0, web.uri().c_str(), webArg);
  web.send_P(200, "application/json", replies[0], n);
}

// ── https, port 443 (ESP-IDF's server, its own task) ────────────────────────

static esp_err_t sendJson(httpd_req_t *r, const char *s, size_t n) {
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_send(r, s, n);
}

static esp_err_t sAny(httpd_req_t *r) {
  secureRequests++;
  noteSecureUse(httpd_req_to_sockfd(r));
  const char *q = strchr(r->uri, '?');
  const std::string path(r->uri, q ? q - r->uri : strlen(r->uri));
  if (path == "/favicon.ico") {            // browsers ask; answering costs nothing, a 404 page more
    httpd_resp_set_status(r, "204 No Content");
    return httpd_resp_send(r, nullptr, 0);
  }
  if (path == "/") {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, TALK_PAGE, HTTPD_RESP_USE_STRLEN);
  }
  Args a;
  if (q) a.parse(q + 1, strlen(q + 1));
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
  if (path == "/health") return sendJson(r, replies[1], healthJson(replies[1], REPLY_MAX));
  const size_t len = answer(1, path.c_str(), [&](const char *k) { return a.get(k); });
  return sendJson(r, replies[1], len);
}

static void startSecure() {
  if (!cert::ready(HOST, WiFi.localIP())) return;
  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  conf.servercert = (const uint8_t *)cert::pem.c_str();
  conf.servercert_len = cert::pem.length() + 1;
  conf.prvtkey_pem = (const uint8_t *)cert::key.c_str();
  conf.prvtkey_len = cert::key.length() + 1;
  conf.httpd.max_open_sockets = SECURE_SOCKETS;
  conf.httpd.open_fn = onSecureOpen;     // called once the handshake is done
  conf.httpd.lru_purge_enable = true;    // one more phone closes the quietest connection
  conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
  conf.httpd.stack_size = 8192;
  // No session tickets: this core's TLS is built without server tickets, and asking for them
  // stops the whole https server from starting.
  conf.tls_handshake_timeout_ms = 5000;  // a stalled handshake must not hold 37 KB for long
  // A phone still on the "not private" warning, or a browser opening a spare connection and
  // dropping it, ends a handshake early. That is normal, and would otherwise fill the log.
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);
  esp_log_level_set("esp-tls", ESP_LOG_NONE);
  esp_log_level_set("esp_https_server", ESP_LOG_NONE);
  esp_log_level_set("httpd", ESP_LOG_NONE);
  esp_event_handler_register(ESP_HTTPS_SERVER_EVENT, ESP_EVENT_ANY_ID, onHttpsEvent, nullptr);
  const esp_err_t e = httpd_ssl_start(&secure, &conf);
  if (e != ESP_OK) { Serial.printf("talk: https did not start (%s)\n", esp_err_to_name(e)); return; }
  static const httpd_uri_t get = {"/*", HTTP_GET, sAny, nullptr};
  static const httpd_uri_t post = {"/*", HTTP_POST, sAny, nullptr};
  httpd_register_uri_handler(secure, &get);
  httpd_register_uri_handler(secure, &post);
  MDNS.addService("https", "tcp", 443);
  Serial.printf("talk: https ready: https://%s.local or https://%s\n", HOST, WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\ntalk: starting");
  tRand = hwRand;
  lock = xSemaphoreCreateMutex();

  const wifisetup::Config cfg = {HOST, "devkit-talk", "devkit-talk", "12", false};
  if (!wifisetup::begin(cfg, web)) return;     // no WiFi yet: the setup page runs from loop()

  web.on("/", [] {
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html; charset=utf-8", TALK_PAGE);
  });
  for (const char *p : {"/rooms", "/create", "/join", "/state", "/say", "/live", "/tr", "/me", "/leave", "/clear", "/health"})
    web.on(p, webApi);
  web.on("/favicon.ico", [] { web.send(204); });
  web.onNotFound([] { web.send(404, "text/plain", "not here"); });
  web.begin();
  startSecure();
  Serial.printf("talk: ready, %lu bytes free\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  wifisetup::loop();
  if (!wifisetup::online()) return;
  web.handleClient();
  static uint32_t lastTick = 0, lastLog = 0;
  const uint32_t now = millis();
  if (now - lastTick >= 100) {
    lastTick = now;
    xSemaphoreTake(lock, portMAX_DELAY);
    tick(now);
    xSemaphoreGive(lock);
    closeQuietSecure();
  }
  if (now - lastLog >= 60000) {
    lastLog = now;
    int rooms_ = 0, people = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    for (auto &r : rooms) if (r.used) { rooms_++; for (auto &p : r.p) people += p.used && !p.away; }
    xSemaphoreGive(lock);
    // The largest free block matters more than the total: a new secure connection needs two
    // 16 KB pieces in one go.
    Serial.printf("talk: %d room(s), %d people, %lu https requests/min, %lu bytes free (largest block %lu, lowest %lu)\n",
                  rooms_, people, (unsigned long)secureRequests.load(), (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), (unsigned long)ESP.getMinFreeHeap());
    secureRequests = 0;
  }
  delay(2);
}
