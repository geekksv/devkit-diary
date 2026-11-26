#pragma once
// The glue between talk.h and the board's two web servers: one lock, one reply buffer, and
// reading form-encoded arguments for the https server. In a header rather than talk.ino,
// because Arduino writes prototypes for every function in a .ino, and a template's prototype
// lands above the line that declares its type.

#include <Arduino.h>
#include <string>
#include <vector>
#include <esp_random.h>
#include "talk.h"

static SemaphoreHandle_t lock;           // route() and tick() run on two server tasks and loop()
// One reply buffer per server (0: http on loop(), 1: https task). stateJson() keeps under it and
// sends "more". A reply is built and sent from here, never copied into a heap that the secure
// connections keep nearly full.
static char replies[2][REPLY_MAX];

static uint32_t hwRand() { return esp_random(); }

// Runs one request through talk.h into that server's own buffer, and returns its length. The
// lock covers only the room, not the (slow, secure) sending.
template <class G> static size_t answer(int server, const char *path, G arg) {
  char *reply = replies[server];
  xSemaphoreTake(lock, portMAX_DELAY);
  Out o = {reply, REPLY_MAX, 0};
  reply[0] = 0;
  route(path, arg, millis(), o);
  xSemaphoreGive(lock);
  return o.n;
}


static std::string urlDecode(const char *s, size_t n) {
  std::string o;
  o.reserve(n);
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < n) {
      const char h[3] = {s[i + 1], s[i + 2], 0};
      o += (char)strtol(h, nullptr, 16);
      i += 2;
    } else o += s[i];
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

