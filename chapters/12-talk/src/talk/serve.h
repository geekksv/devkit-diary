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
static char reply[REPLY_MAX];            // stateJson() keeps under this and sends "more"

static uint32_t hwRand() { return esp_random(); }

// Runs one request through talk.h. Returns the reply as its own copy, so the lock is not held
// while it goes out over a (slow, secure) connection.
template <class G> static std::string answer(const char *path, G arg) {
  xSemaphoreTake(lock, portMAX_DELAY);
  Out o = {reply, sizeof reply, 0};
  reply[0] = 0;
  route(path, arg, millis(), o);
  std::string s(reply, o.n);
  xSemaphoreGive(lock);
  return s;
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

