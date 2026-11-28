// The rules, on a laptop:  c++ -std=c++17 -I../src/reaction reaction_test.cpp -o t && ./t
#include "reaction.h"
#include <map>
#include <random>
#include <string>

static std::mt19937 rng(7);
static uint32_t hostRand() { return rng(); }
static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

struct Args {
  std::map<std::string, std::string> m;
  const char *operator()(const char *k) const { auto i = m.find(k); return i == m.end() ? "" : i->second.c_str(); }
};
static char buf[4096];
static std::string call(const char *path, Args a, uint32_t now) {
  Out o = {buf, sizeof buf, 0};
  route(path, a, now, o);
  return std::string(buf, o.n);
}
static bool has(const std::string &s, const char *x) { return s.find(x) != std::string::npos; }

// Runs time forward to the light, then presses `after` ms later. Returns the phase.
static Phase roundWith(uint32_t &now, uint32_t after) {
  startRound(now);
  while (g.phase == WAITING) tick(++now);
  now += after;
  press(now);
  tick(now);
  return g.phase;
}

int main() {
  rRand = hostRand;
  gameInit();

  // The wait is always 1.5 to 5 s.
  uint32_t lo = 1u << 31, hi = 0, now = 1000;
  for (int i = 0; i < 2000; i++) {
    startRound(now);
    const uint32_t w = g.litAt - now;
    lo = w < lo ? w : lo;
    hi = w > hi ? w : hi;
  }
  CHECK(lo >= 1500 && hi < 5000);
  CHECK(hi - lo > 3000);                      // and spread out, so it can't be learned

  gameInit();
  now = 10000;
  // Light, then press 250 ms later: done, 250 ms, first place.
  CHECK(roundWith(now, 250) == DONE);
  CHECK(g.lastMs == 250 && g.rank == 0 && g.nTop == 1 && g.scoresChanged);
  CHECK(!ledOn(now));

  // The LED is on exactly while lit.
  startRound(now);
  CHECK(!ledOn(now));
  while (g.phase == WAITING) tick(++now);
  CHECK(ledOn(now) && g.phase == LIT);
  press(now + 300);
  CHECK(!ledOn(now + 300) && g.lastMs == 300 && g.rank == 1);

  // Pressing during the wait: a false start, no score, the LED flickers for a second.
  startRound(now);
  const uint8_t before = g.nTop;
  press(now + 500);
  CHECK(g.phase == EARLY && g.nTop == before);
  bool flicker[2] = {false, false};
  for (uint32_t t = now + 500; t < now + 1490; t += 7) flicker[ledOn(t)] = true;
  CHECK(flicker[0] && flicker[1]);
  CHECK(!ledOn(now + 1600));

  // Faster than 100 ms is a guess, not a reaction.
  CHECK(roundWith(now, 60) == EARLY);
  CHECK(g.nTop == before);

  // No press within 3 s: missed.
  startRound(now);
  while (g.phase == WAITING) tick(++now);
  tick(now + 2999);
  CHECK(g.phase == LIT);
  tick(now + 3001);
  CHECK(g.phase == MISSED);

  // After a round, BOOT starts the next one.
  press(now + 4000);
  CHECK(g.phase == WAITING);

  // The top ten: sorted, capped at ten, slow times don't get in, equal times keep the first.
  gameInit();
  now = 50000;
  const uint32_t times[] = {400, 300, 500, 200, 350, 450, 250, 600, 220, 330, 700, 210, 330};
  for (uint32_t t : times) roundWith(now, t);
  CHECK(g.nTop == TOP);
  bool sorted = true;
  for (int i = 1; i < g.nTop; i++) sorted &= g.top[i - 1].ms <= g.top[i].ms;
  CHECK(sorted);
  CHECK(g.top[0].ms == 200 && g.top[1].ms == 210 && g.top[TOP - 1].ms == 450);
  CHECK(g.rank == 6);                        // the second 330: after the first one
  roundWith(now, 900);
  CHECK(g.rank == -1 && g.top[TOP - 1].ms == 450);

  // The last five, newest first.
  CHECK(g.nRecent == RECENT && g.recent[0] == 900 && g.recent[1] == 330);

  // Names: trimmed, cut to 16 bytes without breaking a letter, never empty.
  char c[NAME_LEN + 1];
  cleanName("   Asha  ", c);
  CHECK(!strcmp(c, "Asha"));
  cleanName("", c);
  CHECK(!strcmp(c, "Player"));
  cleanName("\x01\x02", c);
  CHECK(!strcmp(c, "Player"));
  cleanName("अनुष्का शर्मा", c);   // Devanagari: 3 bytes a letter
  CHECK(strlen(c) <= NAME_LEN && strlen(c) % 3 == 0);
  // A new player starts a new "last five".
  setPlayer("Ben");
  CHECK(g.nRecent == 0 && !strcmp(g.player, "Ben"));

  // The web side.
  std::string s = call("/state", {}, now);
  CHECK(has(s, "\"ok\":true") && has(s, "\"player\":\"Ben\"") && has(s, "\"top\":[{"));
  char vq[16];
  snprintf(vq, sizeof vq, "%lu", (unsigned long)g.ver);
  CHECK(has(call("/state", {{{"v", vq}}}, now), "\"same\":true"));
  s = call("/start", {{{"name", "Chen \"the\" fast"}}}, now);
  CHECK(has(s, "\"phase\":\"waiting\"") && has(s, "Chen \\\"the\\\" fast"));
  CHECK(has(call("/start", {}, now), "busy"));
  CHECK(has(call("/clear", {}, now), "\"err\":\"sure\""));
  CHECK(g.nTop == TOP);
  CHECK(has(call("/clear", {{{"sure", "yes"}}}, now), "\"top\":[]"));
  CHECK(has(call("/nope", {}, now), "\"err\":\"what\""));

  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
