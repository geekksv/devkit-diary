#pragma once
// The rules of the reaction game, with nothing from Arduino in them, so a laptop can test them
// (tools/reaction_test.cpp). reaction.ino feeds in the time and the BOOT button, and lights the LED.
//
// A round: start (BOOT or the page) -> wait a random 1.5 to 5 s -> the LED lights -> press BOOT.
// Pressing before the light is a false start. Not pressing within 3 s is a miss. Under 100 ms is
// a guess, not a reaction (no one sees and presses that fast), so it counts as a false start too.

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

struct Out {
  char *b;
  int cap, n;
  void add(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    const int w = vsnprintf(b + n, n < cap ? cap - n : 0, fmt, a);
    va_end(a);
    if (w > 0) n = n + w < cap ? n + w : cap - 1;
  }
  void str(const char *s) {
    add("\"");
    for (; *s; s++) {
      if (*s == '"' || *s == '\\') add("\\%c", *s);
      else if ((unsigned char)*s < 0x20) add(" ");
      else add("%c", *s);
    }
    add("\"");
  }
};

enum Phase : uint8_t { IDLE, WAITING, LIT, DONE, EARLY, MISSED };
static const char *const PHASE_NAME[] = {"idle", "waiting", "lit", "done", "early", "missed"};

static const uint32_t WAIT_MIN = 1500, WAIT_SPAN = 3500;   // the light comes 1.5 to 5 s after start
static const uint32_t MISS_AFTER = 3000;                   // no press in 3 s: a miss
static const uint32_t GUESS_UNDER = 100;                   // faster than this is a guess
static const int TOP = 10, NAME_LEN = 16, RECENT = 5;

struct Score {
  char name[NAME_LEN + 1];
  uint16_t ms;
};

struct Game {
  Phase phase;
  uint32_t litAt, endAt;
  uint16_t lastMs;
  int8_t rank;                 // where the last time landed in the top ten, or -1
  char player[NAME_LEN + 1];
  Score top[TOP];
  uint8_t nTop;
  uint16_t recent[RECENT];     // this player's last few times, newest first
  uint8_t nRecent;
  uint32_t rounds, ver;
  bool scoresChanged;          // reaction.ino saves the top ten when this is set
};

static Game g;
static uint32_t (*rRand)() = nullptr;

// Names: printable, trimmed, at most NAME_LEN bytes, never cut inside a UTF-8 letter.
inline void cleanName(const char *in, char *out) {
  while (*in == ' ') in++;
  int n = 0;
  for (; *in && n < NAME_LEN; in++) {
    const unsigned char c = *in;
    if (c < 0x20 || c == 0x7f) continue;
    out[n++] = (char)c;
  }
  while (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0x80) n--;   // a cut letter's tail
  if (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0xC0) n--;      // and its start
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  if (!n) strcpy(out, "Player");
}

inline void gameInit() {
  memset(&g, 0, sizeof g);
  strcpy(g.player, "Player");
  g.rank = -1;
  g.ver = 1;
}

inline void setPlayer(const char *name) {
  char c[NAME_LEN + 1];
  cleanName(name, c);
  if (strcmp(c, g.player)) {
    strcpy(g.player, c);
    g.nRecent = 0;
  }
  g.ver++;
}

inline void startRound(uint32_t now) {
  g.phase = WAITING;
  g.litAt = now + WAIT_MIN + (rRand ? rRand() % WAIT_SPAN : WAIT_SPAN / 2);
  g.rank = -1;
  g.rounds++;
  g.ver++;
}

// Puts a time in the top ten, sorted, fastest first. Returns its place, or -1.
inline int addScore(const char *name, uint16_t ms) {
  int at = g.nTop;
  while (at > 0 && g.top[at - 1].ms > ms) at--;    // equal times: the earlier one stays ahead
  if (at >= TOP) return -1;
  const int last = g.nTop < TOP ? g.nTop : TOP - 1;
  memmove(&g.top[at + 1], &g.top[at], (last - at) * sizeof(Score));
  strncpy(g.top[at].name, name, NAME_LEN);
  g.top[at].name[NAME_LEN] = 0;
  g.top[at].ms = ms;
  if (g.nTop < TOP) g.nTop++;
  g.scoresChanged = true;
  return at;
}

// The BOOT button went down.
inline void press(uint32_t now) {
  switch (g.phase) {
    case WAITING:
      g.phase = EARLY;
      g.endAt = now;
      break;
    case LIT: {
      const uint32_t ms = now - g.litAt;
      if (ms < GUESS_UNDER) {
        g.phase = EARLY;
      } else {
        g.phase = DONE;
        g.lastMs = (uint16_t)ms;
        g.rank = (int8_t)addScore(g.player, g.lastMs);
        memmove(&g.recent[1], &g.recent[0], (RECENT - 1) * sizeof g.recent[0]);
        g.recent[0] = g.lastMs;
        if (g.nRecent < RECENT) g.nRecent++;
      }
      g.endAt = now;
      break;
    }
    default:                    // idle, or a round over: the button starts the next one
      startRound(now);
      return;
  }
  g.ver++;
}

inline void tick(uint32_t now) {
  if (g.phase == WAITING && (int32_t)(now - g.litAt) >= 0) {
    g.phase = LIT;
    g.ver++;
  } else if (g.phase == LIT && now - g.litAt > MISS_AFTER) {
    g.phase = MISSED;
    g.endAt = now;
    g.ver++;
  }
}

// The LED: on while lit; after a false start, a quick flicker for a second.
inline bool ledOn(uint32_t now) {
  if (g.phase == LIT) return true;
  if (g.phase == EARLY && now - g.endAt < 1000) return (now / 80) % 2;
  return false;
}

inline void clearScores() {
  g.nTop = 0;
  g.nRecent = 0;
  g.scoresChanged = true;
  g.ver++;
}

inline void stateJson(Out &o, uint32_t since) {
  if (since == g.ver) {
    o.add("{\"ok\":true,\"same\":true,\"v\":%lu}", (unsigned long)g.ver);
    return;
  }
  o.add("{\"ok\":true,\"v\":%lu,\"phase\":\"%s\",\"ms\":%u,\"rank\":%d,\"rounds\":%lu,\"player\":",
        (unsigned long)g.ver, PHASE_NAME[g.phase], g.lastMs, g.rank, (unsigned long)g.rounds);
  o.str(g.player);
  o.add(",\"recent\":[");
  for (int i = 0; i < g.nRecent; i++) o.add(i ? ",%u" : "%u", g.recent[i]);
  o.add("],\"top\":[");
  for (int i = 0; i < g.nTop; i++) {
    o.add(i ? ",{\"n\":" : "{\"n\":");
    o.str(g.top[i].name);
    o.add(",\"ms\":%u}", g.top[i].ms);
  }
  o.add("]}");
}

// Every request: path, and arg(key) for its values ("" when missing).
template <class A> inline void route(const char *path, A arg, uint32_t now, Out &o) {
  if (!strcmp(path, "/state")) {
    const char *v = arg("v");
    return stateJson(o, *v ? (uint32_t)strtoul(v, nullptr, 10) : 0);
  }
  if (!strcmp(path, "/start")) {
    if (*arg("name")) setPlayer(arg("name"));
    if (g.phase == WAITING || g.phase == LIT) return o.add("{\"ok\":false,\"err\":\"busy\"}");
    startRound(now);
    return stateJson(o, 0);
  }
  if (!strcmp(path, "/name")) {
    setPlayer(arg("name"));
    return stateJson(o, 0);
  }
  if (!strcmp(path, "/clear")) {
    if (strcmp(arg("sure"), "yes")) return o.add("{\"ok\":false,\"err\":\"sure\"}");
    clearScores();
    return stateJson(o, 0);
  }
  o.add("{\"ok\":false,\"err\":\"what\"}");
}
