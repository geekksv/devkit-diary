// The room rules in talk.h, checked on a laptop, through the same route() the phones use.
//
//   c++ -std=c++17 -O1 -fsanitize=address,undefined -I../src/talk talk_test.cpp -o talk_test && ./talk_test
#include "talk.h"
#include <map>
#include <set>
#include <random>
#include <string>
#include <cstdio>

static std::mt19937 gen(12345);
static uint32_t testRand() { return gen(); }
static int passed = 0, failed = 0;
#define CHECK(c, ...) do { if (c) passed++; else { failed++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ── a small JSON reader, enough to check every reply and read values back ─────────────────────
struct J {
  enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
  bool b = false; double n = 0; std::string s;
  std::vector<J> a; std::map<std::string, J> o;
  const J &operator[](const char *k) const { static J none; auto i = o.find(k); return i == o.end() ? none : i->second; }
};
struct Parser {
  const char *p, *e; bool ok = true;
  void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')) p++; }
  bool str(std::string &out) {
    if (p >= e || *p != '"') return ok = false;
    for (p++; p < e && *p != '"'; p++) {
      if ((unsigned char)*p < 0x20) return ok = false;
      if (*p == '\\') { p++; if (p >= e || !strchr("\"\\/bfnrtu", *p)) return ok = false; out += *p == 'n' ? '\n' : *p; }
      else out += *p;
    }
    if (p >= e) return ok = false;
    p++; return true;
  }
  J val() {
    J v; ws();
    if (p >= e) { ok = false; return v; }
    if (*p == '{') {
      v.t = J::OBJ; p++; ws();
      if (p < e && *p == '}') { p++; return v; }
      for (;;) { ws(); std::string k; if (!str(k)) return v; ws(); if (p >= e || *p != ':') { ok = false; return v; } p++;
        v.o[k] = val(); if (!ok) return v; ws(); if (p < e && *p == ',') { p++; continue; } if (p < e && *p == '}') { p++; return v; } ok = false; return v; }
    }
    if (*p == '[') {
      v.t = J::ARR; p++; ws();
      if (p < e && *p == ']') { p++; return v; }
      for (;;) { v.a.push_back(val()); if (!ok) return v; ws(); if (p < e && *p == ',') { p++; continue; } if (p < e && *p == ']') { p++; return v; } ok = false; return v; }
    }
    if (*p == '"') { v.t = J::STR; str(v.s); return v; }
    if (!strncmp(p, "true", 4)) { v.t = J::BOOL; v.b = true; p += 4; return v; }
    if (!strncmp(p, "false", 5)) { v.t = J::BOOL; p += 5; return v; }
    if (!strncmp(p, "null", 4)) { p += 4; return v; }
    char *end; v.n = strtod(p, &end); if (end == p) ok = false; v.t = J::NUM; p = end; return v;
  }
};

static char buf[REPLY_MAX];
static uint32_t clk = 1000;
static J call(const char *path, std::map<std::string, std::string> args, bool *valid = nullptr, int *len = nullptr) {
  Out o = {buf, sizeof buf, 0};
  buf[0] = 0;
  route(path, [&](const char *k) -> const char * { auto i = args.find(k); return i == args.end() ? "" : i->second.c_str(); }, clk, o);
  Parser ps{buf, buf + o.n};
  J j = ps.val();
  ps.ws();
  const bool good = ps.ok && ps.p == ps.e;
  if (valid) *valid = good;
  if (len) *len = o.n;
  if (!good) { failed++; printf("FAIL bad JSON from %s: %.300s\n", path, buf); }
  return j;
}

static bool validUtf8(const std::string &s) {
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = s[i];
    const int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!len || i + len > s.size()) return false;
    for (int k = 1; k < len; k++) if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
    i += len;
  }
  return true;
}

static uint32_t roomBytes(Room &r) {
  uint32_t b = 0;
  for (int i = 0; i < r.count; i++) b += msgAt(r, i).bytes();
  return b;
}

// Everything a phone would hold after polling until "more" is false.
static int maxState = 0;
struct Phone {
  std::string c, t, tl; uint32_t v = 0; std::map<uint32_t, J> msgs; int replies = 0;
  void sync() {
    for (int guard = 0; guard < 200; guard++) {
      int len = 0;
      J j = call("/state", {{"c", c}, {"t", t}, {"v", std::to_string(v)}, {"tl", tl}}, nullptr, &len);
      if (len > maxState) maxState = len;
      replies++;
      if (j["reset"].b) msgs.clear();
      for (auto &m : j["msgs"].a) msgs[(uint32_t)m["id"].n] = m;
      v = (uint32_t)j["v"].n;
      if (!j["more"].b) return;
    }
    failed++; printf("FAIL sync never finished\n");
  }
};

static void testText() {
  CHECK(cleanText("  hello \n\t world  ", 100) == "hello world", "spaces");
  CHECK(cleanText("a\x01" "b\x7f" "c", 100) == "a bc" || cleanText("a\x01" "b\x7f" "c", 100) == "a b c", "controls: [%s]", cleanText("a\x01" "b\x7f" "c", 100).c_str());
  const std::string hi = "नमस्ते दुनिया";            // 3 bytes a letter
  for (int max = 0; max < 40; max++) {
    const std::string s = cleanText(hi.c_str(), max);
    CHECK((int)s.size() <= max && validUtf8(s), "hindi cut at %d", max);
  }
  CHECK(cleanText("ok \xff\xfe bad \xe0\xa4 end", 100) == "ok bad end", "broken bytes: [%s]", cleanText("ok \xff\xfe bad \xe0\xa4 end", 100).c_str());
  CHECK(cleanText("😀😀", 5) == "😀", "emoji cut");
  char n[NAME_LEN + 1];
  cleanName("<b>\"Riya\"</b>", n);
  CHECK(!strcmp(n, "bRiya/b"), "name markup: %s", n);
  cleanName("राजेश कुमार शर्मा जी नमस्ते", n);
  CHECK(strlen(n) <= NAME_LEN && validUtf8(n), "hindi name");
  char l[LANG_LEN + 1];
  CHECK(cleanLang("hi-IN", l) && !strcmp(l, "hi-IN"), "lang ok");
  CHECK(cleanLang("zh-CN", l), "zh-CN");
  CHECK(!cleanLang("h", l) && !cleanLang("hi\"IN", l) && !cleanLang("<script>", l) && !cleanLang("abcdefghijkl", l), "bad langs");
}

static void testRoom() {
  memset(rooms, 0, sizeof rooms);  // fine at start: nothing allocated yet
  J a = call("/create", {{"n", "Asha"}, {"l", "hi-IN"}});
  CHECK(a["ok"].b && a["c"].s.size() == 4, "create");
  const std::string c = a["c"].s, ta = a["t"].s;
  J b = call("/join", {{"c", c}, {"n", "Ben"}, {"l", "en-GB"}});
  CHECK(b["ok"].b && b["me"].n == 1, "join");
  const std::string tb = b["t"].s;
  CHECK(!call("/join", {{"c", "ZZZZ"}, {"n", "x"}})["ok"].b, "no room");
  J lower = call("/join", {{"c", std::string(c).replace(0, 4, std::string{(char)tolower(c[0]), (char)tolower(c[1]), (char)tolower(c[2]), (char)tolower(c[3])})}, {"t", tb}});
  CHECK(lower["ok"].b && lower["me"].n == 1, "lowercase code, rejoin keeps the seat");

  // Rejoining with a new language changes it.
  call("/join", {{"c", c}, {"t", tb}, {"l", "ta-IN"}});
  J st = call("/state", {{"c", c}, {"t", ta}});
  CHECK(st["people"].a.size() == 2 && st["people"].a[1]["l"].s == "ta-IN", "rejoin with a new language");
  call("/me", {{"c", c}, {"t", tb}, {"l", "en-GB"}, {"n", "Benjamin"}});
  st = call("/state", {{"c", c}, {"t", ta}});
  CHECK(st["people"].a[1]["l"].s == "en-GB" && st["people"].a[1]["n"].s == "Benjamin", "/me");

  // Messages, and a translation shared for one language only.
  J s1 = call("/say", {{"c", c}, {"t", ta}, {"x", "आप कैसे हैं?"}, {"k", "0"}});
  CHECK(s1["ok"].b && s1["id"].n == 1, "say");
  CHECK(!call("/say", {{"c", c}, {"t", ta}, {"x", "   "}})["ok"].b, "empty message refused");
  CHECK(!call("/say", {{"c", c}, {"x", "hi"}})["ok"].b, "a watcher cannot say");
  Phone pb{c, tb, "en"};
  pb.sync();
  CHECK(pb.msgs.size() == 1 && pb.msgs[1]["x"].s == "आप कैसे हैं?" && pb.msgs[1]["l"].s == "hi-IN" && pb.msgs[1]["t"].t == J::NUL, "no translation yet");
  J t1 = call("/tr", {{"c", c}, {"t", tb}, {"id", "1"}, {"tl", "en"}, {"x", "How are you?"}});
  CHECK(t1["r"].s == "kept", "tr kept");
  CHECK(call("/tr", {{"c", c}, {"id", "1"}, {"tl", "en"}, {"x", "How are you doing?"}})["r"].s == "had", "first translation wins");
  CHECK(call("/tr", {{"c", c}, {"id", "99"}, {"tl", "en"}, {"x", "x"}})["r"].s == "gone", "tr for a missing message");
  CHECK(call("/tr", {{"c", c}, {"id", "1"}, {"tl", "<x>"}, {"x", "x"}})["r"].s == "bad", "tr with a bad language");
  const uint32_t before = pb.v;
  pb.sync();
  CHECK(pb.v > before && pb.msgs[1]["t"].s == "How are you?", "translation arrives as a change");
  Phone pd{c, "", "de"};                                   // a watcher reading German
  pd.sync();
  CHECK(pd.msgs[1]["t"].t == J::NUL, "German gets no English translation");
  for (int k = 0; k < MAX_TR + 2; k++) {
    char l[8]; snprintf(l, sizeof l, "x%d", k);
    const auto r = call("/tr", {{"c", c}, {"id", "1"}, {"tl", l}, {"x", "t"}})["r"].s;
    CHECK(k < MAX_TR - 1 ? r == "kept" : r == "full", "tr #%d: %s", k, r.c_str());
  }
  // The speaker's phone may send its own translation along.
  J s2 = call("/say", {{"c", c}, {"t", tb}, {"x", "Fine, thanks"}, {"tl", "hi"}, {"tx", "ठीक हूँ, धन्यवाद"}});
  Phone pa{c, ta, "hi"};
  pa.sync();
  CHECK(pa.msgs[(uint32_t)s2["id"].n]["t"].s == "ठीक हूँ, धन्यवाद", "translation sent with the message");
  CHECK(pa.msgs[(uint32_t)s2["id"].n]["n"].s == "Benjamin" && pa.msgs[(uint32_t)s2["id"].n]["w"].n == 1, "who said it");

  // Live lines come and go.
  call("/live", {{"c", c}, {"t", ta}, {"x", "मैं बोल"}, {"on", "1"}});
  st = call("/state", {{"c", c}, {"t", tb}});
  CHECK(st["people"].a[0]["live"].s == "मैं बोल" && st["people"].a[0]["on"].b, "live line");
  clk += LIVE_MS + 200; tick(clk);
  st = call("/state", {{"c", c}, {"t", tb}});
  CHECK(st["people"].a[0]["live"].s.empty() && st["people"].a[0]["on"].b, "a stale live line goes, listening stays");
  call("/live", {{"c", c}, {"t", ta}, {"x", ""}, {"on", "0"}});
  call("/live", {{"c", c}, {"t", ta}, {"x", "half a sentence"}, {"on", "1"}});
  call("/say", {{"c", c}, {"t", ta}, {"x", "half a sentence, finished"}});
  st = call("/state", {{"c", c}, {"t", tb}});
  CHECK(st["people"].a[0]["live"].s.empty(), "saying clears the live line");

  // Nothing new: a few bytes. Something new: the change.
  pb.sync();
  int len = 0;
  J same = call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(pb.v)}, {"tl", "en"}}, nullptr, &len);
  CHECK(same["same"].b && len < 40 && (uint32_t)same["v"].n == pb.v, "an unchanged room is a short reply (%d bytes)", len);
  call("/say", {{"c", c}, {"t", ta}, {"x", "नया संदेश"}});
  J changed = call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(pb.v)}, {"tl", "en"}});
  CHECK(!changed["same"].b && changed["msgs"].a.size() == 1, "a change is not 'same'");
  // Going away and coming back both reach phones that only ever ask "anything new?".
  pb.sync();
  const uint32_t vBefore = pb.v;
  for (int k = 0; k < 5; k++) { clk += 6000; call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(pb.v)}}); tick(clk); }
  J aw = call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(vBefore)}});
  CHECK(!aw["same"].b && aw["people"].a[0]["away"].b, "someone going quiet shows as away to phones that only poll for changes");
  pb.sync();
  call("/state", {{"c", c}, {"t", ta}});                                     // Asha is back
  J back = call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(pb.v)}});
  CHECK(!back["same"].b && !back["people"].a[0]["away"].b, "coming back shows too");

  // Clear: phones with older versions are told to start again.
  CHECK(!call("/clear", {{"c", c}, {"t", tb}})["ok"].b, "only the host clears");
  CHECK(call("/clear", {{"c", c}, {"t", ta}})["ok"].b, "host clears");
  st = call("/state", {{"c", c}, {"t", tb}, {"v", std::to_string(pb.v)}});
  CHECK(st["reset"].b && st["msgs"].a.empty(), "reset after clear");
  pb.sync();
  CHECK(pb.msgs.empty(), "phone empty after clear");

  // Host hand-over, leaving, closing.
  clk += 30000; call("/state", {{"c", c}, {"t", tb}});      // Ben keeps polling, Asha does not
  clk += 31000; call("/state", {{"c", c}, {"t", tb}}); tick(clk);
  st = call("/state", {{"c", c}, {"t", tb}});
  CHECK(st["host"].n == 1, "host moves to someone present: %g", st["host"].n);
  CHECK(st["people"].a[0]["away"].b, "Asha shown away");
  call("/leave", {{"c", c}, {"t", ta}});
  st = call("/state", {{"c", c}, {"t", tb}});
  CHECK(st["people"].a.size() == 1, "left");
  call("/leave", {{"c", c}, {"t", tb}});
  CHECK(!call("/state", {{"c", c}})["ok"].b, "the last one out closes the room");
}

static void testLimits() {
  for (auto &r : rooms) if (r.used) closeRoom(r);
  std::vector<std::string> codes;
  for (int i = 0; i < MAX_ROOMS; i++) codes.push_back(call("/create", {{"n", "H"}, {"l", "en-IN"}})["c"].s);
  CHECK(codes[0] != codes[1], "codes differ");
  CHECK(call("/create", {{"n", "x"}})["err"].s == "full", "room limit");
  const std::string c = codes[0];
  std::vector<std::string> toks;
  for (int i = 1; i < MAX_PEOPLE; i++) toks.push_back(call("/join", {{"c", c}, {"n", "P" + std::to_string(i)}, {"l", "hi-IN"}})["t"].s);
  CHECK(call("/join", {{"c", c}, {"n", "late"}})["err"].s == "roomfull", "people limit");
  // Keep everyone but P1 polling; after SEAT_FREE_MS P1's seat goes to a newcomer.
  for (uint32_t t = 0; t < SEAT_FREE_MS + 2000; t += 15000) {
    clk += 15000;
    for (size_t i = 1; i < toks.size(); i++) call("/state", {{"c", c}, {"t", toks[i]}});
  }
  J late = call("/join", {{"c", c}, {"n", "late"}, {"l", "fr-FR"}});
  CHECK(late["ok"].b && late["me"].n == 1, "an away seat is reused: me %g", late["me"].n);
  CHECK(!call("/state", {{"c", c}, {"t", toks[0]}, {"v", "0"}})["me"].n == false || call("/state", {{"c", c}, {"t", toks[0]}})["me"].n == -1, "the old token no longer has a seat");
  // A room nobody touches closes.
  clk += ROOM_GONE_MS + 1000; tick(clk);
  CHECK(!call("/state", {{"c", c}})["ok"].b && !call("/state", {{"c", codes[1]}})["ok"].b, "idle rooms close");
}

static std::string randomText(int maxBytes) {
  static const char *parts[] = {"hello ", "नमस्ते ", "வணக்கம் ", "😀", "\"q\" ", "\\ ", "<b>", "\n", "\x01", "\xff", "a", "  ", "日本語 ", "\xe0\xa4"};
  std::string s;
  const int n = gen() % 40;
  for (int i = 0; i < n && (int)s.size() < maxBytes; i++) s += parts[gen() % 14];
  if (gen() % 10 == 0) s = std::string(maxBytes + gen() % 900, 'x');
  return s;
}

// Many phones, random requests (including nonsense), checking after every step that every reply
// is valid JSON that fits, the memory budget holds, and every phone ends up with the same room.
static void testRandom() {
  int maxLen = 0, moreReplies = 0;
  for (int game = 0; game < 300; game++) {
    for (auto &r : rooms) if (r.used) closeRoom(r);
    J a = call("/create", {{"n", randomText(30)}, {"l", "hi-IN"}});
    const std::string c = a["c"].s;
    std::vector<Phone> ph;
    ph.push_back({c, a["t"].s, "hi"});
    const char *tls[] = {"en", "ta", "fr", "zh-CN", "hi"};
    const int people = 2 + gen() % (MAX_PEOPLE - 1);
    for (int i = 1; i < people; i++) {
      J j = call("/join", {{"c", c}, {"n", randomText(40)}, {"l", gen() % 7 ? "en-IN" : "\"bad"}});
      ph.push_back({c, j["t"].s, tls[gen() % 5]});
    }
    ph.push_back({c, "", "fr"});                       // a caption screen
    const int steps = 50 + gen() % 300;
    for (int s = 0; s < steps; s++) {
      clk += gen() % 3000;
      Phone &p = ph[gen() % ph.size()];
      const int what = gen() % 10;
      bool valid; int len;
      if (what < 3) call("/say", {{"c", c}, {"t", p.t}, {"x", randomText(TEXT_MAX + 200)}, {"k", std::to_string(gen() % 4)}}, &valid, &len);
      else if (what < 6) {
        Room *r = byCode(c.c_str());
        const uint32_t id = r && r->nextId > 1 ? 1 + gen() % (r->nextId - 1) : 1;
        call("/tr", {{"c", c}, {"t", p.t}, {"id", std::to_string(id)}, {"tl", p.tl}, {"x", randomText(TR_MAX + 300)}}, &valid, &len);
      } else if (what < 8) { const int before = p.replies; p.sync(); moreReplies += p.replies - before - 1; len = 0; valid = true; }
      else if (what == 8) call("/live", {{"c", c}, {"t", p.t}, {"x", randomText(300)}, {"on", gen() % 2 ? "1" : "0"}}, &valid, &len);
      else { const char *junk[] = {"/nope", "/clear", "/me", "/rooms"}; call(junk[gen() % 4], {{"c", c}, {"t", p.t}, {"l", randomText(12)}, {"n", randomText(30)}}, &valid, &len); }
      if (len > maxLen) maxLen = len;
      tick(clk);
      Room *r = byCode(c.c_str());
      if (!r) break;
      if (r->bytes != roomBytes(*r)) { failed++; printf("FAIL byte count %u vs %u\n", r->bytes, roomBytes(*r)); break; }
      // The budget holds, apart from the newest message, which is always kept.
      uint32_t newest = r->count ? msgAt(*r, r->count - 1).bytes() : 0;
      if (r->bytes > ROOM_BUDGET + newest) { failed++; printf("FAIL budget %u\n", r->bytes); break; }
      if (r->count > MAX_MSGS) { failed++; printf("FAIL count\n"); break; }
    }
    // Every phone that syncs from scratch sees the same messages as one that kept up.
    Room *r = byCode(c.c_str());
    if (!r) continue;
    for (auto &p : ph) {
      p.sync();
      Phone fresh{c, p.t, p.tl};
      fresh.sync();
      // A phone that kept up may still hold messages the board has since forgotten: compare
      // only those the board still has.
      bool same = true;
      for (auto &kv : fresh.msgs) {
        auto it = p.msgs.find(kv.first);
        if (it == p.msgs.end() || it->second["x"].s != kv.second["x"].s || it->second["t"].s != kv.second["t"].s) same = false;
        if (!validUtf8(kv.second["x"].s) || kv.second["x"].s.size() > (size_t)TEXT_MAX) same = false;
      }
      CHECK(same && (int)fresh.msgs.size() == r->count, "game %d: a phone that kept up matches a fresh one (%zu vs %d)", game, fresh.msgs.size(), r->count);
    }
  }
  CHECK(maxLen < (int)sizeof buf - 1 && maxState < (int)sizeof buf - 1, "replies fit: largest %d, state %d", maxLen, maxState);
  printf("random rooms: largest state reply %d bytes, %d extra replies for long histories\n", maxState, moreReplies);
}

// The worst reply: everyone talking with live lines of quotes, a long message of quotes and
// its long translation of quotes.
static void testWorstReply() {
  for (auto &r : rooms) if (r.used) closeRoom(r);
  J a = call("/create", {{"n", std::string(40, '"')}, {"l", "hi-IN"}});
  const std::string c = a["c"].s;
  std::vector<std::string> t{a["t"].s};
  for (int i = 1; i < MAX_PEOPLE; i++) t.push_back(call("/join", {{"c", c}, {"n", std::string(40, '\\')}})["t"].s);
  for (auto &k : t) call("/live", {{"c", c}, {"t", k}, {"x", std::string(400, '"')}, {"on", "1"}});
  for (int m = 0; m < 3; m++) {
    J s = call("/say", {{"c", c}, {"t", t[0]}, {"x", std::string(900, '"')}});
    call("/tr", {{"c", c}, {"id", std::to_string((int)s["id"].n)}, {"tl", "en"}, {"x", std::string(2000, '\\')}});
  }
  for (auto &k : t) call("/live", {{"c", c}, {"t", k}, {"x", std::string(400, '"')}, {"on", "1"}});
  int len = 0;
  bool valid = false;
  J st = call("/state", {{"c", c}, {"t", t[1]}, {"tl", "en"}}, &valid, &len);
  CHECK(valid && st["msgs"].a.size() >= 1 && st["more"].b, "worst reply is whole JSON (%d bytes, %zu msgs)", len, st["msgs"].a.size());
  CHECK(len < REPLY_MAX - 1, "worst reply fits: %d", len);
  Phone p{c, t[1], "en"};
  p.sync();
  CHECK(p.msgs.size() == 3 && p.msgs.begin()->second["t"].s.size() == (size_t)TR_MAX, "all three arrive, over several replies");
  printf("worst reply: %d bytes of %d\n", len, REPLY_MAX);
}

int main() {
  tRand = testRand;
  testText();
  testRoom();
  testLimits();
  testWorstReply();
  testRandom();
  printf("%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
