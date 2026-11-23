// Chapter 12, step 1: can a phone use its microphone on a page from the board?
//
// Phones only allow the microphone on https pages, so this answers the question the whole
// chapter depends on, before anything else is built:
//   1. The board makes its own certificate (cert.h) and serves the same test page on
//      https://talk.local (port 443) and http://talk.local (port 80).
//   2. On the phone: tap the mic, speak, and see whether the browser allowed it.
//   3. The words go to the board, which translates them through Google (or MyMemory if Google
//      fails), while the https server is running, which also tests whether two secure
//      connections at once fit in the board's memory.
//   4. The phone reads the translation aloud with its own voice.
// The page shows a report of each step, and the Serial Monitor shows the board's side.
//
// It uses the WiFi saved by any earlier chapter (wifi_setup.h). If there is none, it opens
// devkit-talk to set one up from a phone.
//
// Board: ESP32 DevKit V1, nothing attached. Flash with the default partition scheme.

// ESP-IDF's https server first: it defines HTTP_ANY too, and Arduino's WebServer needs its own.
#include <esp_https_server.h>
#undef HTTP_ANY
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "cert.h"
#include "translate.h"
#include "page.h"

const char *HOST = "talk";

WebServer web(80);
static httpd_handle_t secure = nullptr;
static SemaphoreHandle_t lock;

// The last few things said, newest first, for the page's log.
struct Said { String t, tr, err, via; uint32_t ms; };
static Said log_[8];
static int logCount = 0;

static String fetch(const std::string &url, int &code) {
  NetworkClientSecure tls;
  // For this test only: the answer is checked by its shape, not the server's certificate.
  // The chapter itself will check Google's and MyMemory's certificates with the built-in bundle.
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(6000);
  http.setConnectTimeout(5000);
  if (!http.begin(tls, url.c_str())) { code = -1; return ""; }
  http.addHeader("User-Agent", "Mozilla/5.0");
  code = http.GET();
  String body = code == 200 ? http.getString() : String();
  http.end();
  return body;
}

// "<to>\n<text>" in, JSON out. Runs on either server's task.
static String said(const String &body, const char *door) {
  const int nl = body.indexOf('\n');
  String to = nl > 0 ? body.substring(0, nl) : String("en");
  String text = nl > 0 ? body.substring(nl + 1) : body;
  text.trim();
  if (text.length() > 500) text = text.substring(0, 500);
  Said s;
  s.t = text;
  const uint32_t t0 = millis();
  std::string out, src;
  int code = 0;
  String a = fetch(tr::googleUrl(to.c_str(), text.c_str()), code);
  if (tr::googleParse(a.c_str(), a.length(), out, src)) s.via = "google";
  else {
    Serial.printf("talk: google failed (HTTP %d), trying MyMemory\n", code);
    a = fetch(tr::myMemoryUrl(to.c_str(), text.c_str()), code);
    if (tr::myMemoryParse(a.c_str(), a.length(), out, src)) s.via = "mymemory";
    else { s.via = "none"; s.err = String("both services failed (last HTTP ") + code + ")"; }
  }
  s.via += String(", ") + door + " page";
  s.tr = out.c_str();
  s.ms = millis() - t0;
  Serial.printf("talk: [%s→%s] \"%s\" → \"%s\" (%s, %lu ms, heap %lu, lowest %lu)\n", src.c_str(), to.c_str(),
                s.t.c_str(), s.tr.c_str(), s.via.c_str(), (unsigned long)s.ms, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap());

  xSemaphoreTake(lock, portMAX_DELAY);
  for (int i = 7; i > 0; i--) log_[i] = log_[i - 1];
  log_[0] = s;
  if (logCount < 8) logCount++;
  xSemaphoreGive(lock);

  return String("{\"tr\":\"") + uiJson(s.tr.c_str()) + "\",\"src\":\"" + uiJson(src.c_str()) + "\",\"via\":\"" +
         uiJson(s.via.c_str()) + "\",\"err\":\"" + uiJson(s.err.c_str()) + "\",\"ms\":" + s.ms +
         ",\"heap\":" + ESP.getFreeHeap() + ",\"min\":" + ESP.getMinFreeHeap() + "}";
}

static String heard() {
  xSemaphoreTake(lock, portMAX_DELAY);
  String o = "{\"log\":[";
  for (int i = 0; i < logCount; i++) {
    if (i) o += ',';
    o += "{\"t\":\"" + uiJson(log_[i].t.c_str()) + "\",\"tr\":\"" + uiJson(log_[i].tr.c_str()) + "\",\"err\":\"" +
         uiJson(log_[i].err.c_str()) + "\",\"via\":\"" + uiJson(log_[i].via.c_str()) + "\",\"ms\":" + log_[i].ms + "}";
  }
  xSemaphoreGive(lock);
  return o + "]}";
}

// ── the https side (ESP-IDF's server, its own task) ─────────────────────────

static esp_err_t sPage(httpd_req_t *r) {
  httpd_resp_set_type(r, "text/html; charset=utf-8");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_send(r, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t sJson(httpd_req_t *r, const String &s) {
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_send(r, s.c_str(), s.length());
}

static esp_err_t sSaid(httpd_req_t *r) {
  const size_t n = r->content_len;
  if (n == 0 || n > 2000) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "0-2000 bytes");
  String body;
  body.reserve(n);
  char buf[256];
  size_t got = 0;
  while (got < n) {
    const int k = httpd_req_recv(r, buf, min(sizeof buf, n - got));
    if (k == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (k <= 0) return ESP_FAIL;
    body.concat(buf, k);
    got += k;
  }
  return sJson(r, said(body, "https"));
}

static esp_err_t sHeard(httpd_req_t *r) { return sJson(r, heard()); }

static void startSecure() {
  if (!cert::ready(HOST, WiFi.localIP())) return;
  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  conf.servercert = (const uint8_t *)cert::pem.c_str();
  conf.servercert_len = cert::pem.length() + 1;
  conf.prvtkey_pem = (const uint8_t *)cert::key.c_str();
  conf.prvtkey_len = cert::key.length() + 1;
  conf.httpd.max_open_sockets = 3;       // each secure connection costs ~40 KB of memory
  conf.httpd.lru_purge_enable = true;    // a fourth phone pushes out the quietest one
  conf.httpd.stack_size = 16384;         // translation opens its own secure connection from here
  const uint32_t t = millis();
  const esp_err_t e = httpd_ssl_start(&secure, &conf);
  if (e != ESP_OK) { Serial.printf("talk: https did not start (%s)\n", esp_err_to_name(e)); return; }
  static const httpd_uri_t uris[] = {
      {"/", HTTP_GET, sPage, nullptr},
      {"/heard", HTTP_GET, sHeard, nullptr},
      {"/said", HTTP_POST, sSaid, nullptr},
  };
  for (const auto &u : uris) httpd_register_uri_handler(secure, &u);
  MDNS.addService("https", "tcp", 443);
  Serial.printf("talk: https ready in %lu ms: https://%s.local or https://%s (heap %lu)\n",
                (unsigned long)(millis() - t), HOST, WiFi.localIP().toString().c_str(), (unsigned long)ESP.getFreeHeap());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\ntalk mic test: starting");
  lock = xSemaphoreCreateMutex();

  const wifisetup::Config cfg = {HOST, "devkit-talk", "devkit-talk-mictest", "12.0", false};
  if (!wifisetup::begin(cfg, web)) return;     // no WiFi yet: the setup page runs from loop()

  web.on("/", [] {
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  web.on("/heard", [] { web.send(200, "application/json", heard()); });
  web.on("/said", HTTP_POST, [] { web.send(200, "application/json", said(web.arg("plain"), "http")); });
  web.begin();
  startSecure();
}

void loop() {
  wifisetup::loop();
  if (wifisetup::online()) web.handleClient();
  delay(2);
}
