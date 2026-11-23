#pragma once
//
// Talk across languages: the rooms, run by the board.
//
// Everything the board decides is here: rooms and codes, who is in them and in which language,
// the messages, the translations phones share, the live "speaking now" lines, the memory budget,
// and the JSON phones are sent. No WiFi and no Arduino, so it builds on a laptop too:
// tools/talk_test.cpp checks it, and tools/devserver.cpp serves the real page through this very
// code. talk.ino only passes requests in from its two servers (http and https) and replies out.
//
// Who does what:
//   - Phones listen (their speech recognition), translate (Google, or MyMemory, straight from the
//     phone) and speak (their voices).
//   - The board keeps the room: it hands every message to everyone, and keeps each translation
//     a phone made so other phones in that language reuse it instead of asking again.
//
// The board does not translate. An early version did, and the test showed why not: one secure
// connection out to Google takes ~55 KB of the board's memory while it runs, the same memory
// the phones' secure connections need.

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <vector>

typedef uint32_t (*RandFn)();
static RandFn tRand = nullptr;

static const int MAX_ROOMS = 2;
static const int MAX_PEOPLE = 10;        // with a seat; anyone can watch without one
static const int MAX_MSGS = 60;          // per room, oldest dropped first
static const int MAX_TR = 8;             // translations kept per message
static const int NAME_LEN = 24;          // bytes of UTF-8
static const int LANG_LEN = 11;          // "en-IN", "zh-CN"
static const int TEXT_MAX = 600;         // one message, bytes
static const int TR_MAX = 1500;          // one translation: Indian scripts take 3 bytes a letter
static const int LIVE_MAX = 200;
// The reply buffer. The worst reply: ten people each with a 200-byte live line, then one message
// of 600 bytes with a 1,500-byte translation, every byte a quote that doubles when escaped. About
// 10 KB; more messages wait for the next reply ("more").
static const int REPLY_MAX = 12288;
static const uint32_t ROOM_BUDGET = 16384;   // bytes of text a room may hold; oldest go first
static const uint32_t AWAY_MS = 20000;       // no poll for this long: shown as away
static const uint32_t HOST_GONE_MS = 60000;  // the host away this long: the next person hosts
static const uint32_t SEAT_FREE_MS = 600000; // away this long: the seat can go to someone new
static const uint32_t ROOM_GONE_MS = 1800000;// nobody at all for 30 minutes: the room closes
static const uint32_t LIVE_MS = 5000;        // a "speaking now" line older than this is dropped

struct Person {
  bool used;
  uint32_t tok;
  char name[NAME_LEN + 1];
  char lang[LANG_LEN + 1];   // what they speak and read, as a speech code: "hi-IN"
  uint32_t seen;
  bool listening;            // keeps the mic open (captions)
  std::string live;          // what they are saying right now, not final yet
  uint32_t liveAt;
};

struct Tr {
  char lang[LANG_LEN + 1];   // a translation code: "hi", "en", "zh-CN"
  std::string text;
};

struct Msg {
  uint32_t id, ver, at;      // at: seconds since the room opened
  int8_t who;                // the seat, or -1 once they have left
  char name[NAME_LEN + 1];   // kept, so the history still says who
  char lang[LANG_LEN + 1];   // the language it was said in (speech code)
  uint8_t kind;              // 0 spoken, 1 typed, 2 a quick phrase
  std::string text;
  std::vector<Tr> tr;
  uint32_t bytes() const {
    uint32_t b = text.size();
    for (auto &t : tr) b += t.text.size();
    return b;
  }
};

struct Room {
  bool used;
  char code[5];
  int8_t host;
  uint32_t opened, active, nextId, ver, clearedVer;
  Person p[MAX_PEOPLE];
  Msg m[MAX_MSGS];
  int head, count;           // m[] is a ring: the oldest is at head
  uint32_t bytes;
};

static Room rooms[MAX_ROOMS];

// ── small helpers ───────────────────────────────────────────────────────────────────────────

static const char CODE_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
inline uint32_t rnd(uint32_t n) { return n ? tRand() % n : 0; }
inline uint32_t newToken() { uint32_t t; do t = tRand(); while (!t); return t; }

inline Room *byCode(const char *c) {
  for (auto &r : rooms) if (r.used && !strcmp(r.code, c)) return &r;
  return nullptr;
}
inline int personOf(const Room &r, uint32_t t) {
  if (!t) return -1;
  for (int i = 0; i < MAX_PEOPLE; i++) if (r.p[i].used && r.p[i].tok == t) return i;
  return -1;
}

// Copies UTF-8 text: no control characters (newlines become spaces), single spaces, at most
// `max` bytes, never a letter (Hindi, an emoji) cut in half, broken sequences dropped. With
// `markup` false, < > " \ go too (for names).
inline std::string cleanText(const char *in, int max, bool markup = true) {
  std::string out;
  bool sp = false;
  for (const unsigned char *s = (const unsigned char *)in; *s;) {
    const unsigned char c = *s;
    const int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!len) { s++; continue; }                    // a stray continuation byte or 0xF8+
    if (len == 1 && (c < 0x20 || c == 0x7f || c == ' ')) { if (!out.empty()) sp = true; s++; continue; }
    if (!markup && (c == '"' || c == '\\' || c == '<' || c == '>')) { s++; continue; }
    bool ok = true;
    for (int i = 1; i < len; i++) if ((s[i] & 0xC0) != 0x80) ok = false;
    if (!ok) { s++; continue; }
    if ((int)out.size() + (sp ? 1 : 0) + len > max) break;
    if (sp) { out += ' '; sp = false; }
    out.append((const char *)s, len);
    s += len;
  }
  return out;
}

inline void cleanName(const char *in, char *out) {
  const std::string s = cleanText(in, NAME_LEN, false);
  memcpy(out, s.c_str(), s.size() + 1);
}

// A language code as phones send it: letters, digits and '-', 2 to LANG_LEN long.
inline bool cleanLang(const char *in, char *out) {
  int n = 0;
  for (; in[n]; n++) {
    const char c = in[n];
    if (n >= LANG_LEN || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  if (n < 2) return false;
  memcpy(out, in, n + 1);
  return true;
}

inline Msg &msgAt(Room &r, int i) { return r.m[(r.head + i) % MAX_MSGS]; }
inline Msg *msgById(Room &r, uint32_t id) {
  for (int i = 0; i < r.count; i++) if (msgAt(r, i).id == id) return &msgAt(r, i);
  return nullptr;
}

inline void dropOldest(Room &r) {
  if (!r.count) return;
  Msg &m = r.m[r.head];
  r.bytes -= m.bytes();
  m.text.clear();
  m.text.shrink_to_fit();
  m.tr.clear();
  m.tr.shrink_to_fit();
  r.head = (r.head + 1) % MAX_MSGS;
  r.count--;
}

// Keeps the room under its budget by forgetting the oldest messages. The newest is always kept,
// however long, so a room is never left empty by one long message.
inline void fitBudget(Room &r) {
  while (r.bytes > ROOM_BUDGET && r.count > 1) dropOldest(r);
}

// ── rooms and people ────────────────────────────────────────────────────────────────────────

inline void closeRoom(Room &r) {
  while (r.count) dropOldest(r);
  for (auto &p : r.p) { p.live.clear(); p.live.shrink_to_fit(); p.used = false; }
  r.used = false;
}

inline Room *newRoom(uint32_t now) {
  Room *r = nullptr;
  for (auto &x : rooms) if (!x.used) { r = &x; break; }
  if (!r) return nullptr;
  char code[5];
  do {
    for (int i = 0; i < 4; i++) code[i] = CODE_CHARS[rnd(sizeof CODE_CHARS - 1)];
    code[4] = 0;
  } while (byCode(code));
  closeRoom(*r);
  memcpy(r->code, code, 5);
  r->used = true;
  r->host = -1;
  r->opened = r->active = now;
  r->nextId = 1;
  r->ver = 1;
  r->clearedVer = 0;
  r->head = r->count = 0;
  r->bytes = 0;
  return r;
}

// A seat for someone new, or -1 when the room is full. A seat away long enough is reused.
inline int addPerson(Room &r, const char *name, const char *lang, uint32_t now) {
  int at = -1;
  for (int i = 0; i < MAX_PEOPLE; i++) if (!r.p[i].used) { at = i; break; }
  if (at < 0)
    for (int i = 0; i < MAX_PEOPLE; i++)
      if (i != r.host && now - r.p[i].seen > SEAT_FREE_MS) { at = i; break; }
  if (at < 0) return -1;
  for (int i = 0; i < r.count; i++) if (msgAt(r, i).who == at) msgAt(r, i).who = -1;
  Person &p = r.p[at];
  p.used = true;
  p.tok = newToken();
  cleanName(name, p.name);
  if (!p.name[0]) snprintf(p.name, sizeof p.name, "Guest %d", at + 1);
  if (!cleanLang(lang, p.lang)) strcpy(p.lang, "en-IN");
  p.seen = now;
  p.listening = false;
  p.live.clear();
  p.liveAt = 0;
  r.ver++;
  return at;
}

inline void removePerson(Room &r, int i) {
  r.p[i].used = false;
  r.p[i].live.clear();
  r.p[i].live.shrink_to_fit();
  for (int k = 0; k < r.count; k++) if (msgAt(r, k).who == i) msgAt(r, k).who = -1;
  if (r.host == i) {
    r.host = -1;
    for (int k = 0; k < MAX_PEOPLE; k++) if (r.p[k].used) { r.host = k; break; }
  }
  r.ver++;
}

// The clock: host hand-over, live lines going stale, empty rooms closing.
inline void tick(uint32_t now) {
  for (auto &r : rooms) {
    if (!r.used) continue;
    if (now - r.active > ROOM_GONE_MS) { closeRoom(r); continue; }
    if (r.host >= 0 && now - r.p[r.host].seen > HOST_GONE_MS) {
      for (int k = 1; k < MAX_PEOPLE; k++) {
        const int i = (r.host + k) % MAX_PEOPLE;
        if (r.p[i].used && now - r.p[i].seen < AWAY_MS) { r.host = i; r.ver++; break; }
      }
    }
    for (auto &p : r.p)
      if (p.used && !p.live.empty() && now - p.liveAt > LIVE_MS) { p.live.clear(); r.ver++; }
  }
}

// A new message from seat `who`; returns it, or nullptr if there was nothing to say.
inline Msg *say(Room &r, int who, const char *text, const char *lang, int kind, uint32_t now) {
  const std::string t = cleanText(text, TEXT_MAX);
  if (t.empty()) return nullptr;
  if (r.count == MAX_MSGS) dropOldest(r);
  Msg &m = r.m[(r.head + r.count) % MAX_MSGS];
  r.count++;
  m.id = r.nextId++;
  m.ver = ++r.ver;
  m.at = (now - r.opened) / 1000;
  m.who = who;
  memcpy(m.name, r.p[who].name, sizeof m.name);
  if (!cleanLang(lang, m.lang)) memcpy(m.lang, r.p[who].lang, sizeof m.lang);
  m.kind = kind >= 0 && kind <= 2 ? kind : 0;
  m.text = t;
  m.tr.clear();
  r.bytes += m.bytes();
  r.p[who].live.clear();
  fitBudget(r);
  return &m;
}

enum TrResult { TR_KEPT, TR_HAD, TR_FULL, TR_GONE, TR_BAD };
static const char *const TR_TEXT[] = {"kept", "had", "full", "gone", "bad"};

// A phone's translation of message `id` into `lang`. The first one for each language is kept.
inline TrResult addTr(Room &r, uint32_t id, const char *lang, const char *text) {
  Msg *m = msgById(r, id);
  if (!m) return TR_GONE;
  char l[LANG_LEN + 1];
  if (!cleanLang(lang, l)) return TR_BAD;
  const std::string t = cleanText(text, TR_MAX);
  if (t.empty()) return TR_BAD;
  for (auto &x : m->tr) if (!strcmp(x.lang, l)) return TR_HAD;
  if ((int)m->tr.size() >= MAX_TR) return TR_FULL;
  Tr x;
  memcpy(x.lang, l, sizeof x.lang);
  x.text = t;
  m->tr.push_back(x);
  r.bytes += t.size();
  m->ver = ++r.ver;
  fitBudget(r);                         // may forget this very message, if it was the oldest
  return TR_KEPT;
}

inline void clearRoom(Room &r) {
  while (r.count) dropOldest(r);
  r.bytes = 0;
  r.clearedVer = ++r.ver;
}

// ── JSON out ────────────────────────────────────────────────────────────────────────────────

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
    if (n + 2 >= cap) return;
    b[n++] = '"';
    for (; *s && n < cap - 8; s++) {
      const unsigned char c = *s;
      if (c == '"' || c == '\\') { b[n++] = '\\'; b[n++] = c; }
      else if (c < 0x20) b[n++] = ' ';
      else b[n++] = c;
    }
    b[n++] = '"';
    b[n] = 0;
  }
  int left() const { return cap - n; }
};

// The room as one phone (or the caption screen) needs it. `since`: the room version the phone
// already has; only messages changed after it are sent. `tl`: the phone's translation code, so
// only that translation of each message is sent. A long history comes over several replies:
// "more" asks the phone to come straight back.
inline void stateJson(Out &o, Room &r, int me, uint32_t since, const char *tl, uint32_t now) {
  const bool reset = since < r.clearedVer || since > r.ver;
  if (reset) since = 0;
  o.add("{\"ok\":true,\"c\":\"%s\",\"me\":%d,\"host\":%d,\"now\":%lu,\"reset\":%s,\"people\":[", r.code, me, r.host,
        (unsigned long)((now - r.opened) / 1000), reset ? "true" : "false");
  bool first = true;
  for (int i = 0; i < MAX_PEOPLE; i++) {
    const Person &p = r.p[i];
    if (!p.used) continue;
    o.add("%s{\"i\":%d,\"n\":", first ? "" : ",", i);
    o.str(p.name);
    o.add(",\"l\":\"%s\",\"away\":%s,\"on\":%s,\"live\":", p.lang, now - p.seen > AWAY_MS ? "true" : "false",
          p.listening ? "true" : "false");
    o.str(p.live.c_str());
    o.add("}");
    first = false;
  }
  o.add("],\"msgs\":[");
  // Changed messages in version order, so a reply cut short can be continued from its last one.
  uint32_t sent = since;
  bool more = false;
  first = true;
  for (;;) {
    Msg *next = nullptr;
    for (int i = 0; i < r.count; i++) {
      Msg &m = msgAt(r, i);
      if (m.ver > sent && (!next || m.ver < next->ver)) next = &m;
    }
    if (!next) break;
    const Tr *t = nullptr;
    if (tl && *tl) for (auto &x : next->tr) if (!strcmp(x.lang, tl)) t = &x;
    const int need = 160 + 2 * ((int)next->text.size() + (t ? (int)t->text.size() : 0) + NAME_LEN);  // escaped
    if (!first && o.left() < need + 64) { more = true; break; }
    o.add("%s{\"id\":%lu,\"v\":%lu,\"at\":%lu,\"w\":%d,\"n\":", first ? "" : ",", (unsigned long)next->id,
          (unsigned long)next->ver, (unsigned long)next->at, next->who);
    o.str(next->name);
    o.add(",\"l\":\"%s\",\"k\":%d,\"x\":", next->lang, next->kind);
    o.str(next->text.c_str());
    if (t) { o.add(",\"t\":"); o.str(t->text.c_str()); }
    o.add("}");
    sent = next->ver;
    first = false;
  }
  // Without "more", the phone is up to date with the whole room, people included.
  o.add("],\"v\":%lu,\"more\":%s}", (unsigned long)(more ? sent : r.ver), more ? "true" : "false");
}

// ── the requests ────────────────────────────────────────────────────────────────────────────
//
// One entry point, so the board's two servers and the laptop server answer identically.
// arg(key) returns the request's value for key, or "".

template <class G> inline void route(const char *path, G arg, uint32_t now, Out &o) {
  auto fail = [&](const char *why) { o.add("{\"ok\":false,\"err\":\"%s\"}", why); };
  if (!strcmp(path, "/rooms")) {
    o.add("{\"ok\":true,\"rooms\":[");
    bool first = true;
    for (auto &r : rooms) {
      if (!r.used) continue;
      int n = 0;
      for (auto &p : r.p) n += p.used && now - p.seen < AWAY_MS;
      o.add("%s{\"c\":\"%s\",\"n\":%d,\"host\":", first ? "" : ",", r.code, n);
      o.str(r.host >= 0 ? r.p[r.host].name : "");
      o.add(",\"langs\":[");
      bool f2 = true;
      for (auto &p : r.p) if (p.used) { o.add("%s\"%s\"", f2 ? "" : ",", p.lang); f2 = false; }
      o.add("]}");
      first = false;
    }
    o.add("]}");
    return;
  }
  if (!strcmp(path, "/create")) {
    Room *r = newRoom(now);
    if (!r) return fail("full");
    const int i = addPerson(*r, arg("n"), arg("l"), now);
    r->host = i;
    o.add("{\"ok\":true,\"c\":\"%s\",\"t\":\"%08lx\",\"me\":%d}", r->code, (unsigned long)r->p[i].tok, i);
    return;
  }
  char code[5] = {0};
  for (int i = 0; i < 4 && arg("c")[i]; i++) {
    const char c = arg("c")[i];
    code[i] = c >= 'a' && c <= 'z' ? c - 32 : c;
  }
  Room *rp = byCode(code);
  if (!rp) return fail("noroom");
  Room &r = *rp;
  r.active = now;
  const uint32_t tok = (uint32_t)strtoul(arg("t"), nullptr, 16);
  int me = personOf(r, tok);
  if (me >= 0) {
    if (now - r.p[me].seen > AWAY_MS) r.ver++;   // back from away: everyone sees it
    r.p[me].seen = now;
  }

  if (!strcmp(path, "/join")) {
    if (me >= 0) {                               // back with your seat: maybe a new language
      char l[LANG_LEN + 1];
      if (cleanLang(arg("l"), l) && strcmp(l, r.p[me].lang)) { memcpy(r.p[me].lang, l, sizeof l); r.ver++; }
    }
    if (me < 0) me = addPerson(r, arg("n"), arg("l"), now);
    if (me < 0) return fail("roomfull");
    o.add("{\"ok\":true,\"c\":\"%s\",\"t\":\"%08lx\",\"me\":%d}", r.code, (unsigned long)r.p[me].tok, me);
    return;
  }
  if (!strcmp(path, "/state"))
    return stateJson(o, r, me, (uint32_t)strtoul(arg("v"), nullptr, 10), arg("tl"), now);
  if (!strcmp(path, "/tr")) {                    // anyone may share a translation, watchers too
    const TrResult t = addTr(r, (uint32_t)strtoul(arg("id"), nullptr, 10), arg("tl"), arg("x"));
    o.add("{\"ok\":%s,\"r\":\"%s\"}", t == TR_KEPT || t == TR_HAD ? "true" : "false", TR_TEXT[t]);
    return;
  }
  if (me < 0) return fail("notin");
  Person &p = r.p[me];

  if (!strcmp(path, "/say")) {
    Msg *m = say(r, me, arg("x"), arg("l"), atoi(arg("k")), now);
    if (!m) return fail("empty");
    // The speaker's phone may send its own translation of what it said along with it.
    if (arg("tl")[0] && arg("tx")[0]) addTr(r, m->id, arg("tl"), arg("tx"));
    o.add("{\"ok\":true,\"id\":%lu}", (unsigned long)m->id);
    return;
  }
  if (!strcmp(path, "/live")) {
    const std::string t = cleanText(arg("x"), LIVE_MAX);
    const bool on = arg("on")[0] == '1';
    if (t != p.live || on != p.listening) r.ver++;
    p.live = t;
    p.liveAt = now;
    p.listening = on;
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/me")) {                    // change name or language
    char l[LANG_LEN + 1];
    if (arg("l")[0] && cleanLang(arg("l"), l)) memcpy(p.lang, l, sizeof l);
    if (arg("n")[0]) {
      char n[NAME_LEN + 1];
      cleanName(arg("n"), n);
      if (n[0]) memcpy(p.name, n, sizeof n);
    }
    r.ver++;
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/leave")) {
    removePerson(r, me);
    bool anyone = false;
    for (auto &q : r.p) anyone |= q.used;
    if (!anyone) closeRoom(r);
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/clear")) {
    if (me != r.host) return fail("nothost");
    clearRoom(r);
    o.add("{\"ok\":true}");
    return;
  }
  fail("unknown");
}
