// The board's rules in clip.h, checked on a laptop through the same route() the devices use.
//
//   c++ -std=c++17 -O1 -fsanitize=address,undefined -I../src/clip clip_test.cpp -o clip_test && ./clip_test
#include "clip.h"
#include <map>
#include <set>
#include <vector>
#include <random>
#include <string>
#include <cstdio>

static std::mt19937 gen(4242);
static uint32_t testRand() { return gen(); }
static int passed = 0, failed = 0;
#define CHECK(c, ...) do { if (c) passed++; else { failed++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ── a store in memory ───────────────────────────────────────────────────────────────────────
struct MemStore : Store {
  std::map<uint32_t, std::string> files;
  int read(uint32_t id, uint8_t *buf, int max) override {
    auto i = files.find(id);
    if (i == files.end()) return -1;
    const int n = (int)i->second.size() < max ? (int)i->second.size() : max;
    memcpy(buf, i->second.data(), n);
    return n;
  }
  void remove(uint32_t id) override { files.erase(id); }
  void removeAll() override { files.clear(); }
};
static MemStore mem;

// ── a small JSON reader ─────────────────────────────────────────────────────────────────────
struct J {
  enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
  bool b = false; double n = 0; std::string s; std::vector<J> a; std::map<std::string, J> o;
  const J &operator[](const char *k) const { static J none; auto i = o.find(k); return i == o.end() ? none : i->second; }
};
struct Parser {
  const char *p, *e; bool ok = true;
  void ws() { while (p < e && (*p == ' ' || *p == '\n')) p++; }
  bool str(std::string &out) {
    if (p >= e || *p != '"') return ok = false;
    for (p++; p < e && *p != '"'; p++) {
      if ((unsigned char)*p < 0x20) return ok = false;
      if (*p == '\\') { p++; if (p >= e) return ok = false; }
      out += *p;
    }
    if (p >= e) return ok = false;
    p++; return true;
  }
  J val() {
    J v; ws();
    if (p >= e) { ok = false; return v; }
    if (*p == '{') { v.t = J::OBJ; p++; ws(); if (p < e && *p == '}') { p++; return v; }
      for (;;) { ws(); std::string k; if (!str(k)) return v; ws(); if (p >= e || *p != ':') { ok = false; return v; } p++;
        v.o[k] = val(); if (!ok) return v; ws(); if (p < e && *p == ',') { p++; continue; } if (p < e && *p == '}') { p++; return v; } ok = false; return v; } }
    if (*p == '[') { v.t = J::ARR; p++; ws(); if (p < e && *p == ']') { p++; return v; }
      for (;;) { v.a.push_back(val()); if (!ok) return v; ws(); if (p < e && *p == ',') { p++; continue; } if (p < e && *p == ']') { p++; return v; } ok = false; return v; } }
    if (*p == '"') { v.t = J::STR; str(v.s); return v; }
    if (!strncmp(p, "true", 4)) { v.t = J::BOOL; v.b = true; p += 4; return v; }
    if (!strncmp(p, "false", 5)) { v.t = J::BOOL; p += 5; return v; }
    char *end; v.n = strtod(p, &end); if (end == p) ok = false; v.t = J::NUM; p = end; return v;
  }
};

static char buf[REPLY_MAX];
static uint32_t clk = 1000, unixNow = 1795000000;
static int maxReply = 0;
static J call(const char *path, std::map<std::string, std::string> args) {
  Out o = {buf, sizeof buf, 0};
  buf[0] = 0;
  route(path, [&](const char *k) -> const char * { auto i = args.find(k); return i == args.end() ? "" : i->second.c_str(); }, clk, unixNow, o);
  Parser ps{buf, buf + o.n};
  J j = ps.val();
  ps.ws();
  if (!(ps.ok && ps.p == ps.e)) { failed++; printf("FAIL bad JSON from %s: %.300s\n", path, buf); }
  if (o.n > maxReply) maxReply = o.n;
  return j;
}

static std::string hex(const uint8_t *p, int n) { char o[200]; toHex(p, n, o); return o; }
static std::string b64decode(const std::string &s) {
  static const std::string A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o; uint32_t v = 0; int bits = 0;
  for (char c : s) { if (c == '=') break; v = v << 6 | A.find(c); bits += 6; if (bits >= 8) { bits -= 8; o += (char)(v >> bits & 255); } }
  return o;
}

// The board's view of a device: joins with the auth key, then sends and reads.
static uint8_t AUTH[32];
struct Dev {
  std::string t; int id = 0; uint32_t v = 0, epoch = 0; std::map<uint32_t, J> items; std::set<uint32_t> ids;
  bool join(const char *name, int kind = 0, const uint8_t *auth = AUTH) {
    J c = call("/challenge", {});
    uint8_t n[16], a[32];
    fromHex(c["n"].s.c_str(), n, 16);
    sha::hmac(auth, 32, n, 16, a);
    J j = call("/join", {{"n", c["n"].s}, {"a", hex(a, 32)}, {"name", name}, {"kind", std::to_string(kind)}});
    if (!j["ok"].b) return false;
    t = j["t"].s; id = (int)j["id"].n; return true;
  }
  // What a device does to send: check, store under the next id, add.
  J put(const std::string &bytes, int flags = 0, int to = 0) {
    Device *d = deviceByToken((uint32_t)strtoul(t.c_str(), nullptr, 16));
    J r; r.t = J::OBJ;
    if (!d) { r.o["err"].s = "notin"; return r; }
    const PutResult p = canPut(*d, bytes.size(), to);
    if (p != P_OK) { r.o["err"].s = PUT_TEXT[p]; return r; }
    const uint32_t id = nextItemId();
    mem.files[id] = bytes;
    Item *it = addItem(*d, bytes.size(), flags, to, clk, unixNow);
    if (!it) { mem.files.erase(id); r.o["err"].s = "full"; return r; }
    CHECK(it->id == id, "the id stored under is the id given");
    r.o["ok"].b = true; r.o["id"].n = id; return r;
  }
  void sync() {
    for (int guard = 0; guard < 100; guard++) {
      J j = call("/state", {{"t", t}, {"v", std::to_string(v)}, {"epoch", std::to_string(epoch)}});
      if (!j["ok"].b) return;
      if (j["same"].b) return;
      if ((uint32_t)j["epoch"].n != epoch) { items.clear(); epoch = (uint32_t)j["epoch"].n; }
      ids.clear();
      for (auto &x : j["ids"].a) ids.insert((uint32_t)x.n);
      for (auto &x : j["items"].a) items[(uint32_t)x["id"].n] = x;
      for (auto it = items.begin(); it != items.end();) it = ids.count(it->first) ? std::next(it) : items.erase(it);
      v = (uint32_t)j["v"].n;
      if (!j["more"].b) return;
    }
    failed++; printf("FAIL sync never ended\n");
  }
};

static void freshBoard() {
  resetAll();
  for (auto &x : AUTH) x = gen();
  J h = call("/hello", {});
  CHECK(h["ok"].b && !h["set"].b && h["salt"].s.size() == 32, "hello before setup");
  CHECK(!call("/setup", {{"salt", std::string(32, '0')}, {"auth", hex(AUTH, 32)}})["ok"].b, "setup with another salt refused");
  CHECK(call("/setup", {{"salt", h["salt"].s}, {"auth", hex(AUTH, 32)}})["ok"].b, "setup");
}

static void testSha() {
  uint8_t h[32];
  sha::Ctx c;
  sha::init(c); sha::final(c, h);
  CHECK(hex(h, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty");
  sha::init(c); sha::update(c, (const uint8_t *)"abc", 3); sha::final(c, h);
  CHECK(hex(h, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc");
  std::string m(1000000, 'a');
  sha::init(c);
  for (size_t i = 0; i < m.size(); i += 777) sha::update(c, (const uint8_t *)m.data() + i, std::min<size_t>(777, m.size() - i));
  sha::final(c, h);
  CHECK(hex(h, 32) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "sha256 a million a's, in pieces");
  sha::hmac((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, h);
  CHECK(hex(h, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "hmac RFC 4231 case 2");
  uint8_t key[131]; memset(key, 0xaa, sizeof key);
  const char *msg = "Test Using Larger Than Block-Size Key - Hash Key First";
  sha::hmac(key, sizeof key, (const uint8_t *)msg, strlen(msg), h);
  CHECK(hex(h, 32) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", "hmac RFC 4231 case 6 (long key)");
}

// The real helper's code makes the proof; the board accepts it.
static void testWithPython() {
  resetAll();
  J h = call("/hello", {});
  const std::string salt = h["salt"].s;
  char cmd[600];
  snprintf(cmd, sizeof cmd, "python3 -c \"import sys; sys.path.insert(0,'../helper'); import clipcrypto as c; "
           "k=c.derive('मेरा पासवर्ड 😀', bytes.fromhex('%s'), 1000); print(k.auth.hex())\"", salt.c_str());
  FILE *f = popen(cmd, "r");
  char out[200] = {0};
  if (f) { if (!fgets(out, sizeof out, f)) out[0] = 0; pclose(f); }
  std::string auth(out); while (!auth.empty() && (auth.back() == '\n' || auth.back() == '\r')) auth.pop_back();
  CHECK(auth.size() == 64, "python gave an auth key: [%s]", auth.c_str());
  CHECK(call("/setup", {{"salt", salt}, {"auth", auth}})["ok"].b, "setup with python's key");
  J c = call("/challenge", {});
  snprintf(cmd, sizeof cmd, "python3 -c \"import sys; sys.path.insert(0,'../helper'); import clipcrypto as c; "
           "k=c.derive('मेरा पासवर्ड 😀', bytes.fromhex('%s'), 1000); print(c.answer(k, bytes.fromhex('%s')))\"", salt.c_str(), c["n"].s.c_str());
  f = popen(cmd, "r");
  out[0] = 0;
  if (f) { if (!fgets(out, sizeof out, f)) out[0] = 0; pclose(f); }
  std::string ans(out); while (!ans.empty() && (ans.back() == '\n' || ans.back() == '\r')) ans.pop_back();
  J j = call("/join", {{"n", c["n"].s}, {"a", ans}, {"name", "Python"}});
  CHECK(j["ok"].b, "the board accepts the helper's proof: %s", j["err"].s.c_str());
}

static void testJoin() {
  freshBoard();
  CHECK(call("/setup", {{"salt", std::string(32, 'a')}, {"auth", hex(AUTH, 32)}})["err"].s == "already", "setup only once");
  Dev a, b;
  CHECK(a.join("Laptop") && b.join("<Phone>", 1), "two devices join");
  J st = call("/state", {{"t", a.t}});
  CHECK(st["devices"].a.size() == 2 && st["devices"].a[1]["n"].s == "Phone" && st["devices"].a[1]["k"].n == 1, "names cleaned, kinds kept");
  // Wrong proofs.
  uint8_t other[32]; memset(other, 7, 32);
  Dev x;
  CHECK(!x.join("Intruder", 0, other), "the wrong key is refused");
  J c = call("/challenge", {});
  uint8_t n[16], ans[32]; fromHex(c["n"].s.c_str(), n, 16); sha::hmac(AUTH, 32, n, 16, ans);
  CHECK(call("/join", {{"n", c["n"].s}, {"a", hex(ans, 32)}})["ok"].b, "a right answer");
  CHECK(call("/join", {{"n", c["n"].s}, {"a", hex(ans, 32)}})["err"].s == "nochallenge", "a challenge answers once");
  c = call("/challenge", {});
  clk += CHALLENGE_MS + 1;
  fromHex(c["n"].s.c_str(), n, 16); sha::hmac(AUTH, 32, n, 16, ans);
  CHECK(call("/join", {{"n", c["n"].s}, {"a", hex(ans, 32)}})["err"].s == "nochallenge", "an old challenge is refused");
  for (int i = 0; i < MAX_WRONG; i++) x.join("Intruder", 0, other);
  Dev y;
  CHECK(!y.join("Me"), "after five wrong answers, even a right one waits");
  clk += WRONG_PAUSE_MS + 1;
  CHECK(y.join("Me"), "a minute later it works");
  CHECK(call("/state", {{"t", "12345678"}})["err"].s == "notin", "a made-up token gets nothing");
  CHECK(call("/pin", {{"id", "1"}, {"on", "1"}})["err"].s == "notin", "nor does no token");
  int joined = 4;
  Dev z;
  while (z.join("More")) { joined++; z = Dev(); }
  CHECK(joined == MAX_DEVICES, "device limit: %d", joined);
}

static void testItems() {
  freshBoard();
  Dev a, b, c;
  a.join("A"); b.join("B"); c.join("C");
  // An item for everyone, small enough to ride along.
  J p = a.put("sealed-bytes-1");
  CHECK(p["ok"].b, "put");
  b.sync();
  CHECK(b.items.size() == 1 && b64decode(b.items.begin()->second["b"].s) == "sealed-bytes-1", "the bytes come inline");
  CHECK(b.items.begin()->second["from"].n == a.id && b.items.begin()->second["at"].n == unixNow, "who and when");
  // Nothing new: short.
  J same = call("/state", {{"t", b.t}, {"v", std::to_string(b.v)}, {"epoch", std::to_string(b.epoch)}});
  CHECK(same["same"].b, "same");
  // Big items are fetched separately.
  std::string big(INLINE_MAX + 1, 'x');
  const uint32_t bigId = (uint32_t)a.put(big)["id"].n;
  b.sync();
  CHECK(b.items[bigId]["b"].t == J::NUL && b.items[bigId]["size"].n == big.size(), "a big item comes by itself");
  CHECK(blobFor(strtoul(b.t.c_str(), nullptr, 16), bigId, clk) == (int)big.size(), "and may be fetched");
  // For one device only.
  const uint32_t toB = (uint32_t)a.put("for B only", 0, b.id)["id"].n;
  b.sync(); c.sync();
  CHECK(b.ids.count(toB) && !c.ids.count(toB), "only B (and A) see it");
  CHECK(blobFor(strtoul(c.t.c_str(), nullptr, 16), toB, clk) == -1, "C cannot fetch it");
  CHECK(a.put("x", 0, a.id)["err"].s == "noto" && a.put("x", 0, 99)["err"].s == "noto", "not to yourself or nobody");
  // One time only.
  const uint32_t once = (uint32_t)a.put("123456", F_ONCE)["id"].n;
  c.sync();
  CHECK(c.items[once]["b"].t == J::NUL && (int)c.items[once]["f"].n == F_ONCE, "a one-time item never rides inline");
  taken(strtoul(a.t.c_str(), nullptr, 16), once);
  CHECK(itemById(once), "the sender looking does not use it up");
  taken(strtoul(c.t.c_str(), nullptr, 16), once);
  CHECK(!itemById(once) && !mem.files.count(once), "the first other device takes it, for everyone");
  b.sync();
  CHECK(!b.ids.count(once), "and it is gone from the others' lists");
  const uint32_t once2 = (uint32_t)a.put("999", F_ONCE)["id"].n;
  clk += 121000; tick(clk, unixNow);
  CHECK(!itemById(once2), "an untaken one-time item goes after two minutes");
  // Empty, too big, paused.
  CHECK(a.put("")["err"].s == "empty", "empty");
  CHECK(a.put(std::string(BLOB_MAX + 1, 'z'))["err"].s == "big", "too big");
  call("/device", {{"t", b.t}, {"id", std::to_string(a.id)}, {"paused", "1"}});
  CHECK(a.put("x")["err"].s == "paused", "a paused device sends nothing");
  call("/device", {{"t", a.t}, {"paused", "0"}, {"name", "Laptop ✨"}});
  CHECK(a.put("x")["ok"].b, "resumed");
  CHECK(deviceById(a.id) && !strcmp(deviceById(a.id)->name, "Laptop ✨"), "renamed");
  // Delete, pin, clear.
  const uint32_t keep = (uint32_t)a.put("my address")["id"].n;
  CHECK(call("/pin", {{"t", b.t}, {"id", std::to_string(keep)}, {"on", "1"}})["ok"].b, "pin");
  CHECK(call("/pin", {{"t", c.t}, {"id", std::to_string(toB)}, {"on", "1"}})["err"].s == "gone", "cannot pin what you cannot see");
  CHECK(call("/del", {{"t", c.t}, {"id", std::to_string(bigId)}})["ok"].b && !mem.files.count(bigId), "delete removes the bytes");
  call("/clear", {{"t", a.t}});
  a.sync();
  CHECK(a.ids.size() == 1 && a.ids.count(keep), "clear keeps the pins: %zu left", a.ids.size());
  // Forgetting a device removes what only it could see.
  const uint32_t toC = (uint32_t)a.put("for C", 0, c.id)["id"].n;
  call("/forget", {{"t", a.t}, {"id", std::to_string(c.id)}});
  CHECK(!itemById(toC) && call("/state", {{"t", c.t}})["err"].s == "notin", "forgotten, with its items");
}

static void testLimits() {
  freshBoard();
  Dev a; a.join("A");
  std::vector<uint32_t> ids;
  for (int i = 0; i < 60; i++) ids.push_back((uint32_t)a.put("item " + std::to_string(i))["id"].n);
  a.sync();
  CHECK((int)a.ids.size() == HISTORY && !a.ids.count(ids[9]) && a.ids.count(ids[10]), "history keeps the newest %d", HISTORY);
  CHECK((int)mem.files.size() == HISTORY, "and their bytes only: %zu", mem.files.size());
  // Pins survive, up to MAX_PINS.
  int pinned = 0;
  for (int i = 10; i < 60 && pinned < MAX_PINS + 1; i++)
    if (call("/pin", {{"t", a.t}, {"id", std::to_string(ids[i])}, {"on", "1"}})["ok"].b) pinned++;
  CHECK(pinned == MAX_PINS, "pin limit: %d", pinned);
  for (int i = 0; i < 70; i++) a.put("more " + std::to_string(i));
  a.sync();
  int pins = 0;
  for (auto &kv : a.items) pins += ((int)kv.second["f"].n & F_PIN) != 0;
  CHECK(pins == MAX_PINS && (int)a.ids.size() == HISTORY + MAX_PINS, "pins survive the history moving on (%d pins, %zu items)", pins, a.ids.size());
  // The byte budget.
  freshBoard();
  Dev b; b.join("B");
  for (int i = 0; i < 5; i++) CHECK(b.put(std::string(BLOB_MAX, 'a' + i))["ok"].b, "big item %d", i);
  CHECK(usedBytes <= BUDGET, "budget held: %u", usedBytes);
  b.sync();
  CHECK(b.ids.size() == BUDGET / BLOB_MAX, "the oldest big ones went: %zu left", b.ids.size());
  // Expiry.
  freshBoard();
  Dev e; e.join("E");
  const uint32_t old = (uint32_t)e.put("old")["id"].n;
  const uint32_t pinnedOld = (uint32_t)e.put("pinned")["id"].n;
  call("/pin", {{"t", e.t}, {"id", std::to_string(pinnedOld)}, {"on", "1"}});
  const uint32_t savedUnix = unixNow;
  unixNow = 0;
  const uint32_t noClock = (uint32_t)e.put("born before the clock")["id"].n;
  unixNow = savedUnix + 24 * 3600 + 5;
  tick(clk, unixNow);
  CHECK(!itemById(old) && itemById(pinnedOld) && itemById(noClock), "a day later: unpinned gone, pinned and undated kept");
  CHECK(call("/settings", {{"t", e.t}, {"keep", "0"}})["ok"].b && !call("/settings", {{"t", e.t}, {"keep", "5"}})["ok"].b, "keep: 0, 1, 24, 168 only");
  const uint32_t forever = (uint32_t)e.put("forever")["id"].n;
  unixNow += 400 * 24 * 3600; tick(clk, unixNow);
  CHECK(itemById(forever), "keep 0 means never");
  unixNow = savedUnix;
}

static void testOnlineSaveReset() {
  freshBoard();
  Dev a, b; a.join("A"); b.join("B");
  b.sync();
  clk += ONLINE_MS + 1000;
  call("/state", {{"t", a.t}});
  tick(clk, unixNow);
  J st = call("/state", {{"t", a.t}, {"v", "0"}});
  bool bOff = false;
  for (auto &d : st["devices"].a) if ((int)d["i"].n == b.id) bOff = !d["on"].b;
  CHECK(bOff, "a quiet device shows offline");
  const uint32_t before = b.v;
  b.sync();
  CHECK(b.v != before, "and coming back is a change");
  // Save, wipe memory, load.
  const uint32_t i1 = (uint32_t)a.put("kept across a restart")["id"].n;
  const uint32_t i2 = (uint32_t)a.put("bytes lost")["id"].n;
  static Saved s;
  saveTo(s);
  CHECK(!dirty, "saving clears dirty");
  mem.files.erase(i2);
  memset(items, 0, sizeof items); memset(devices, 0, sizeof devices);
  CHECK(loadFrom(s, clk), "load");
  CHECK(itemById(i1) && !itemById(i2), "items come back, minus any whose bytes are missing");
  CHECK(usedBytes == itemById(i1)->size, "bytes recounted: %u", usedBytes);
  CHECK(deviceByToken(strtoul(a.t.c_str(), nullptr, 16)), "tokens still work after a restart");
  s.items[0].size ^= 1;
  CHECK(!loadFrom(s, clk), "a damaged record is refused");
  // Reset: a new group; everyone starts again.
  const uint32_t epoch = group.epoch;
  CHECK(!call("/reset", {{"t", a.t}})["ok"].b, "reset needs sure=1");
  CHECK(call("/reset", {{"t", a.t}, {"sure", "1"}})["ok"].b, "reset");
  CHECK(group.epoch != epoch && !group.set && mem.files.empty(), "a new epoch, no group, no bytes");
  CHECK(call("/state", {{"t", a.t}})["err"].s == "nogroup", "old tokens are gone");
}

static void testPaging() {
  freshBoard();
  Dev a, b; a.join("A"); b.join("B");
  for (int i = 0; i < HISTORY; i++) a.put(std::string(INLINE_MAX, '"'));
  b.sync();
  int inl = 0;
  for (auto &kv : b.items) inl += kv.second["b"].s.size() > 0;
  CHECK((int)b.items.size() == HISTORY && inl == HISTORY, "all %d inline items arrive over several replies (%d inline)", HISTORY, inl);
  CHECK(maxReply < REPLY_MAX - 1, "every reply fits: largest %d", maxReply);
}

// Random use, checking after every step that the books balance and every reply is whole JSON.
static void testRandom() {
  for (int round = 0; round < 60; round++) {
    freshBoard();
    std::vector<Dev> ds(2 + gen() % 6);
    for (size_t i = 0; i < ds.size(); i++) ds[i].join(("D" + std::to_string(i)).c_str(), gen() % 2);
    for (int step = 0; step < 300; step++) {
      Dev &d = ds[gen() % ds.size()];
      const int w = gen() % 12;
      clk += gen() % 4000;
      if (w < 4) {
        const int sz = gen() % 6 == 0 ? 1 + gen() % BLOB_MAX : 1 + gen() % 3000;
        const int to = gen() % 5 == 0 ? ds[gen() % ds.size()].id : 0;
        d.put(std::string(sz, 'a' + gen() % 26), gen() % 4 == 0 ? F_ONCE : 0, to);
      } else if (w < 6) d.sync();
      else if (w == 6) call("/pin", {{"t", d.t}, {"id", std::to_string(1 + gen() % group.nextItem)}, {"on", gen() % 2 ? "1" : "0"}});
      else if (w == 7) call("/del", {{"t", d.t}, {"id", std::to_string(1 + gen() % group.nextItem)}});
      else if (w == 8) taken(strtoul(d.t.c_str(), nullptr, 16), 1 + gen() % group.nextItem);
      else if (w == 9) call("/device", {{"t", d.t}, {"paused", gen() % 3 == 0 ? "1" : "0"}});
      else if (w == 10) { unixNow += gen() % 20000; tick(clk, unixNow); }
      else call("/nonsense", {{"t", d.t}});
      uint32_t sum = 0; int n = 0, pins = 0;
      for (auto &i : items) if (i.used) { sum += i.size; n++; pins += (i.flags & F_PIN) != 0; if (!mem.files.count(i.id)) { failed++; printf("FAIL item without bytes\n"); } }
      if (sum != usedBytes || (int)mem.files.size() != n || sum > BUDGET || n - pins > HISTORY || pins > MAX_PINS) {
        failed++; printf("FAIL books: sum %u used %u files %zu items %d pins %d\n", sum, usedBytes, mem.files.size(), n, pins); break;
      }
    }
    // Everyone who catches up sees exactly what the board says they may.
    for (auto &d : ds) {
      d.sync();
      Dev fresh = d; fresh.v = 0; fresh.items.clear(); fresh.sync();
      std::set<uint32_t> want;
      for (auto &i : items) if (i.used && visibleTo(i, d.id)) want.insert(i.id);
      CHECK(d.ids == want && fresh.ids == want && fresh.items.size() == want.size(), "round %d: a device's view matches the board", round);
    }
  }
}

int main() {
  cRand = testRand;
  store = &mem;
  testSha();
  testWithPython();
  testJoin();
  testItems();
  testLimits();
  testOnlineSaveReset();
  testPaging();
  testRandom();
  printf("largest reply %d bytes of %d\n", maxReply, REPLY_MAX);
  printf("%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
