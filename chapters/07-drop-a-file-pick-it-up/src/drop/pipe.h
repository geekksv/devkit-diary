#pragma once
//
// The pipe: a file goes in one connection and out of another, and the board holds only the few
// kilobytes in between.
//
// The ordinary web server (port 80) answers one request at a time, start to finish. A pipe needs
// two connections open at once — the receiver's download and the sender's upload — so it gets a
// small server of its own on port 8080, written at the socket level:
//
//   receiver:  GET  /get?o=<offer>&k=<token>    — held open, becomes the download
//   sender:    POST /put?o=<offer>&k=<token>    — its body is the file
//
// When both are connected, the receiver is sent the download's headers (name, type, size), and
// from then on every pass of loop() moves up to 32 KB from the sender's socket to the receiver's,
// 4 KB at a time. TCP does the rest: when the board stops reading, the sender's browser simply
// waits, so a slow receiver slows the sender down instead of filling the board's memory.
//
// In a header, not the .ino: chapter 6 found that the Arduino build's function-declaration step
// leaves headers alone.

#include <WiFi.h>

const uint16_t PIPE_PORT = 8080;
const uint32_t PIPE_WAIT_MS = 60000;        // one side waiting this long for the other gives up
const uint32_t PIPE_STALL_MS = 20000;       // no bytes moving for this long mid-transfer: give up

// Provided by the sketch.
bool pipeAllowed(const String &offerId, bool sender, const String &token);
void pipeDescribe(String &name, String &mime, uint64_t &size);
void pipeReceiverReady();
void pipeStarted();
void pipeFinished(bool ok, const char *why);

static NetworkServer pipeServer(PIPE_PORT);
static NetworkClient pSender, pReceiver;
static bool pRunning = false;               // both ends connected, bytes moving
// 64-bit: a file is allowed to be bigger than 4 GB, which a 32-bit count — the ESP32's natural
// size — would wrap round at. A long phone video passes 4 GB easily.
static uint64_t pSize = 0, pMoved = 0;
static uint32_t pSince = 0, pLastByte = 0, pStarted = 0;
static uint8_t pBuf[4096];

inline uint64_t pipeSent() { return pMoved; }

static String u64(uint64_t v) { char b[24]; snprintf(b, sizeof(b), "%llu", (unsigned long long)v); return String(b); }
inline bool pipeBusy() { return pRunning; }

static const char *CORS =
  "Access-Control-Allow-Origin: *\r\n"
  "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
  "Access-Control-Allow-Headers: Content-Type\r\n"
  "Access-Control-Max-Age: 600\r\n";

static void pipeReply(NetworkClient &c, int code, const char *status, const char *body) {
  c.printf("HTTP/1.1 %d %s\r\n%sContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n%s",
           code, status, CORS, (unsigned)strlen(body), body);
  c.stop();
}

static void pipeReset() {
  if (pSender) pSender.stop();
  if (pReceiver) pReceiver.stop();
  pRunning = false;
  pSize = pMoved = pSince = pLastByte = 0;
}

inline void pipeAbort() { pipeReset(); }

static void pipeFail(const char *why) {
  Serial.printf("pipe: failed after %llu of %llu bytes — %s\n", (unsigned long long)pMoved, (unsigned long long)pSize, why);
  if (pSender && pSender.connected()) {
    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":false,\"err\":\"%s\"}", why);
    pipeReply(pSender, 500, "Failed", body);
  }
  pipeReset();
  pipeFinished(false, why);
}

// Reads one request line and its headers, up to the blank line. The body (a sender's file) is
// left in the socket, untouched.
static bool readHead(NetworkClient &c, String &method, String &path, String &query, uint64_t &length) {
  String line;
  bool first = true;
  length = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < 3000) {
    if (!c.available()) { if (!c.connected()) return false; delay(1); continue; }
    const char ch = c.read();
    if (ch == '\r') continue;
    if (ch != '\n') { if (line.length() < 512) line += ch; continue; }
    if (first) {
      const int s1 = line.indexOf(' '), s2 = line.indexOf(' ', s1 + 1);
      if (s1 < 0 || s2 < 0) return false;
      method = line.substring(0, s1);
      String target = line.substring(s1 + 1, s2);
      const int q = target.indexOf('?');
      path = q < 0 ? target : target.substring(0, q);
      query = q < 0 ? String("") : target.substring(q + 1);
      first = false;
    } else if (!line.length()) {
      return true;                          // the blank line: headers done
    } else if (line.substring(0, 15).equalsIgnoreCase("content-length:")) {
      length = strtoull(line.substring(15).c_str(), NULL, 10);
    }
    line = "";
  }
  return false;
}

// One value from a query string. The values here are hex ids and tokens: nothing to decode.
static String queryArg(const String &q, const char *key) {
  const String k = String(key) + "=";
  int at = -1;
  if (q.startsWith(k)) at = k.length();
  else {
    const int i = q.indexOf("&" + k);
    if (i >= 0) at = i + 1 + k.length();
  }
  if (at < 0) return "";
  const int end = q.indexOf('&', at);
  return q.substring(at, end < 0 ? q.length() : end);
}

// The file's name, twice: a plain fallback, and RFC 5987 UTF-8 for names with accents or emoji.
static String disposition(const String &name) {
  String plain, enc;
  for (unsigned i = 0; i < name.length(); i++) {
    const uint8_t c = name[i];
    plain += (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') ? (char)c : '_';
    if (isalnum(c) || strchr("-._~", c)) enc += (char)c;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); enc += b; }
  }
  return "attachment; filename=\"" + plain + "\"; filename*=UTF-8''" + enc;
}

static void pipeAccept(NetworkClient c) {
  String method, path, query;
  uint64_t length;
  if (!readHead(c, method, path, query, length)) { c.stop(); return; }
  if (method == "OPTIONS") {                // the browser's preflight for an upload with progress
    c.printf("HTTP/1.1 204 No Content\r\n%sContent-Length: 0\r\nConnection: close\r\n\r\n", CORS);
    c.stop();
    return;
  }
  const String o = queryArg(query, "o"), k = queryArg(query, "k");
  const bool sender = method == "POST" && path == "/put";
  const bool receiver = method == "GET" && path == "/get";
  if (!sender && !receiver) { pipeReply(c, 404, "Not Found", "{\"ok\":false,\"err\":\"path\"}"); return; }
  if (!pipeAllowed(o, sender, k)) { pipeReply(c, 403, "Forbidden", "{\"ok\":false,\"err\":\"offer\"}"); return; }
  if (pRunning || (sender && pSender) || (receiver && pReceiver)) {
    pipeReply(c, 409, "Busy", "{\"ok\":false,\"err\":\"busy\"}");
    return;
  }
  String name, mime;
  uint64_t size;
  pipeDescribe(name, mime, size);
  if (sender && length != size) { pipeReply(c, 400, "Bad Length", "{\"ok\":false,\"err\":\"size\"}"); return; }
  c.setNoDelay(true);
  if (sender) pSender = c; else { pReceiver = c; pipeReceiverReady(); }
  if (!pSince) pSince = millis();
  Serial.printf("pipe: %s connected\n", sender ? "sender" : "receiver");

  if (pSender && pReceiver) {               // both here: open the download and start moving
    pSize = size;
    pReceiver.printf("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %s\r\nContent-Disposition: %s\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     mime.c_str(), u64(size).c_str(), disposition(name).c_str());
    pRunning = true;
    pStarted = pLastByte = millis();
    pipeStarted();
    Serial.printf("pipe: moving \"%s\", %s bytes\n", name.c_str(), u64(size).c_str());
  }
}

inline void pipeBegin() { pipeServer.begin(); }

// Called every pass of loop(): take any new connection, then move up to 32 KB.
inline void pipeLoop() {
  NetworkClient c = pipeServer.accept();
  if (c) pipeAccept(c);

  if (!pRunning) {                          // one side waiting for the other
    if ((pSender || pReceiver) && millis() - pSince > PIPE_WAIT_MS) pipeFail("the other side never connected");
    return;
  }
  size_t budget = 32 * 1024;
  while (budget && pMoved < pSize) {
    const int avail = pSender.available();
    if (avail <= 0) break;
    // What is left can be more than 4 GB, and size_t is 32 bits here: compare before narrowing.
    size_t want = min((size_t)avail, sizeof(pBuf));
    if (pSize - pMoved < want) want = (size_t)(pSize - pMoved);
    const int got = pSender.read(pBuf, want);
    if (got <= 0) break;
    if (pReceiver.write(pBuf, got) != (size_t)got) { pipeFail("the receiver closed the download"); return; }
    pMoved += got;
    budget -= min(budget, (size_t)got);
    pLastByte = millis();
  }
  if (pMoved >= pSize) {
    const uint32_t ms = millis() - pStarted;
    Serial.printf("pipe: done — %s bytes in %u ms (%.0f KB/s)\n", u64(pSize).c_str(), (unsigned)ms,
                  ms ? (double)pSize / 1.024 / ms : 0.0);
    pReceiver.flush();
    pipeReply(pSender, 200, "OK", "{\"ok\":true}");
    pipeReset();
    pipeFinished(true, "");
    return;
  }
  if (!pSender.connected() && !pSender.available()) { pipeFail("the sender stopped"); return; }
  if (!pReceiver.connected()) { pipeFail("the receiver closed the download"); return; }
  if (millis() - pLastByte > PIPE_STALL_MS) pipeFail("nothing moved for 20 seconds");
}
