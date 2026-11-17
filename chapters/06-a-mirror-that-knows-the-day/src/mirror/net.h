#pragma once
//
// Fetching over HTTPS, for a board with 320 KB of RAM.
//
// In a header rather than the .ino on purpose: the Arduino build writes a declaration for
// every function in a .ino before compiling it, and a templated function comes out of that
// mangled ("'F' has not been declared"). Headers are left alone. Found the first time this
// was built anywhere that runs that step — GitHub's build, not this laptop's.

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "parse.h"

static const char *USER_AGENT = "devkit-diary-mirror/1.0";

// The root certificates the core already carries (Mozilla's list). With them the board can
// check that it is really talking to api.open-meteo.com and not to something pretending.
extern const uint8_t CERTS_START[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t CERTS_END[] asm("_binary_x509_crt_bundle_end");

// A response read straight off the connection, byte by byte, so a 150 KB feed never has to fit
// in memory. Gives up after `limit` bytes or five quiet seconds.
struct NetSource : Source {
  NetworkClient *c;
  size_t left;
  explicit NetSource(NetworkClient *client, size_t limit) : c(client), left(limit) {}
  int next() override {
    if (!left) return -1;
    const uint32_t t0 = millis();
    while (!c->available()) {
      if (!c->connected() || millis() - t0 > 5000) return -1;
      delay(2);
    }
    left--;
    return c->read();
  }
};

// GET a small answer whole. Returns the HTTP status, or a negative HTTPClient error.
static int getSmall(const String &url, String &body, size_t cap = 16384) {
  NetworkClientSecure tls;
  tls.setCACertBundle(CERTS_START, CERTS_END - CERTS_START);
  HTTPClient http;
  http.setUserAgent(USER_AGENT);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(8000);
  http.setTimeout(12000);
  if (!http.begin(tls, url)) return -1;
  const int code = http.GET();
  if (code == 200) {
    if (http.getSize() > (int)cap) { http.end(); return -100; }
    body = http.getString();
  }
  http.end();
  return code;
}

// GET a big answer and hand it to `read` as it arrives. HTTP/1.0, so the server sends the body
// plain rather than in chunks — chunk headers in the middle of the bytes would land inside a
// headline.
template <typename F>
static int getStream(const String &url, size_t limit, F read) {
  NetworkClientSecure tls;
  tls.setCACertBundle(CERTS_START, CERTS_END - CERTS_START);
  HTTPClient http;
  http.useHTTP10(true);
  http.setUserAgent(USER_AGENT);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(8000);
  http.setTimeout(12000);
  if (!http.begin(tls, url)) return -1;
  const int code = http.GET();
  if (code == 200) {
    NetSource src(http.getStreamPtr(), limit);
    read(src);
  }
  http.end();                  // closes the connection: the rest of the feed is never downloaded
  return code;
}
