#pragma once
//
// One clipboard for every device: everything the board decides.
//
// The board keeps a group of devices that share a passphrase, and the sealed clipboard items they
// send. It never sees the passphrase or the contents: a device proves it belongs by answering a
// random challenge with HMAC-SHA256 under the group's auth key (one of three keys each device
// derives from the passphrase; the other two, which encrypt and seal, never leave the devices).
//
// Here: the group, joining, devices, items and their order, pins, one-time and one-device items,
// expiry, the storage budget, saving it all to flash, and the JSON. No WiFi and no Arduino, so it
// builds on a laptop: tools/clip_test.cpp checks it and tools/devserver.cpp serves the real page
// and helper through it. The item bytes themselves are files, behind Store.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include "clip_sha.h"

typedef uint32_t (*RandFn)();
static RandFn cRand = nullptr;

static const int MAX_DEVICES = 12;
static const int NAME_LEN = 24;
static const int HISTORY = 50;               // unpinned items kept
static const int MAX_PINS = 30;
static const int MAX_ITEMS = HISTORY + MAX_PINS + 1;
static const uint32_t BLOB_MAX = 420000;     // one sealed item: a 300 KB screenshot fits with room
static const uint32_t BUDGET = 1000000;      // all items together, on the board's ~1.4 MB of flash
static const uint32_t INLINE_MAX = 1536;     // small items ride along in /state, no second request
static const int REPLY_MAX = 12288;
static const uint32_t ONLINE_MS = 15000;     // no request for this long: shown offline
static const uint32_t CHALLENGE_MS = 60000;
static const int MAX_CHALLENGES = 6;
static const int MAX_WRONG = 5;              // wrong proofs in a row before a minute's pause
static const uint32_t WRONG_PAUSE_MS = 60000;

// F_IMAGE: so a phone can say "an image" before opening it. F_OPEN: a link for one computer to
// open in its browser rather than put on its clipboard.
enum { F_PIN = 1, F_ONCE = 2, F_IMAGE = 4, F_OPEN = 8 };

// Where the sealed bytes live: files on the board, a map on a laptop.
struct Store {
  virtual int read(uint32_t id, uint8_t *buf, int max) = 0;   // bytes read, or -1
  virtual void remove(uint32_t id) = 0;
  virtual void removeAll() = 0;
  virtual ~Store() {}
};
static Store *store = nullptr;

struct Device {
  bool used;
  uint8_t id;
  uint32_t tok;
  char name[NAME_LEN + 1];
  uint8_t kind;              // 0 a computer with the helper, 1 a browser
  bool paused;               // sends nothing (still receives)
  bool online;               // as last shown to everyone
  uint32_t seen;             // millis
};

struct Item {
  bool used;
  uint32_t id, ver, size;
  uint8_t from, to;          // to: 0 everyone, else one device
  uint8_t flags;
  uint32_t born;             // unix time, or 0 when the board had no clock yet
  uint32_t bornMs;
};

struct Group {
  bool set;
  uint8_t salt[16];
  uint8_t pendingSalt[16];   // offered by /hello before the first device sets up
  uint8_t auth[32];
  uint32_t epoch;            // changes on reset, so devices know to start again
  uint16_t keepHours;        // unpinned items expire after this; 0 never
  uint32_t ver, nextItem;
  uint8_t nextDev;
};

struct Challenge { uint8_t n[16]; uint32_t at; bool used; };

static Group group;
static Device devices[MAX_DEVICES];
static Item items[MAX_ITEMS];
static Challenge chals[MAX_CHALLENGES];
static uint32_t usedBytes = 0;
static int wrongProofs = 0;
static uint32_t wrongAt = 0;
static bool dirty = false;   // something worth saving to flash changed

// ── helpers ─────────────────────────────────────────────────────────────────────────────────

inline void randomBytes(uint8_t *p, int n) {
  for (int i = 0; i < n; i += 4) { const uint32_t r = cRand(); for (int k = 0; k < 4 && i + k < n; k++) p[i + k] = r >> (8 * k); }
}
inline uint32_t newToken() { uint32_t t; do t = cRand(); while (!t); return t; }

inline void toHex(const uint8_t *p, int n, char *out) {
  static const char *H = "0123456789abcdef";
  for (int i = 0; i < n; i++) { out[i * 2] = H[p[i] >> 4]; out[i * 2 + 1] = H[p[i] & 15]; }
  out[n * 2] = 0;
}
inline bool fromHex(const char *s, uint8_t *out, int n) {
  if ((int)strlen(s) != n * 2) return false;
  for (int i = 0; i < n * 2; i++) {
    const char c = s[i];
    const int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (v < 0) return false;
    if (i & 1) out[i / 2] |= v; else out[i / 2] = v << 4;
  }
  return true;
}

// A device's name as typed: no control characters or markup, single spaces, never a letter cut.
inline void cleanName(const char *in, char *out) {
  int o = 0;
  bool sp = false;
  for (const unsigned char *s = (const unsigned char *)in; *s;) {
    const unsigned char c = *s;
    const int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!len) { s++; continue; }
    if (len == 1 && (c < 0x20 || c == 0x7f || c == ' ')) { if (o) sp = true; s++; continue; }
    if (c == '"' || c == '\\' || c == '<' || c == '>') { s++; continue; }
    bool ok = true;
    for (int i = 1; i < len; i++) if ((s[i] & 0xC0) != 0x80) ok = false;
    if (!ok) { s++; continue; }
    if (o + (sp ? 1 : 0) + len > NAME_LEN) break;
    if (sp) { out[o++] = ' '; sp = false; }
    memcpy(out + o, s, len);
    o += len;
    s += len;
  }
  out[o] = 0;
}

inline Device *deviceByToken(uint32_t t) {
  if (!t) return nullptr;
  for (auto &d : devices) if (d.used && d.tok == t) return &d;
  return nullptr;
}
inline Device *deviceById(int id) {
  for (auto &d : devices) if (d.used && d.id == id) return &d;
  return nullptr;
}
inline Item *itemById(uint32_t id) {
  for (auto &i : items) if (i.used && i.id == id) return &i;
  return nullptr;
}
inline bool visibleTo(const Item &i, int dev) { return i.to == 0 || i.to == dev || i.from == dev; }

inline void bump() { group.ver++; dirty = true; }

inline void removeItem(Item &i) {
  if (!i.used) return;
  if (store) store->remove(i.id);
  usedBytes -= i.size;
  i.used = false;
  bump();
}

// ── the group ───────────────────────────────────────────────────────────────────────────────

inline void resetAll() {
  if (store) store->removeAll();
  memset(devices, 0, sizeof devices);
  memset(items, 0, sizeof items);
  memset(chals, 0, sizeof chals);
  const uint32_t ver = group.ver;
  memset(&group, 0, sizeof group);
  group.ver = ver + 1;
  group.epoch = newToken();
  group.keepHours = 24;
  group.nextItem = 1;
  group.nextDev = 1;
  randomBytes(group.pendingSalt, 16);
  usedBytes = 0;
  dirty = true;
}

inline bool setupGroup(const uint8_t salt[16], const uint8_t auth[32]) {
  if (group.set || memcmp(salt, group.pendingSalt, 16)) return false;
  memcpy(group.salt, salt, 16);
  memcpy(group.auth, auth, 32);
  group.set = true;
  bump();
  return true;
}

inline const uint8_t *newChallenge(uint32_t now) {
  Challenge *c = &chals[0];                                   // a free one, else the oldest
  for (auto &x : chals) {
    if (!x.used || now - x.at > CHALLENGE_MS) { c = &x; break; }
    if (now - x.at > now - c->at) c = &x;
  }
  randomBytes(c->n, 16);
  c->at = now;
  c->used = true;
  return c->n;
}

enum JoinResult { J_OK, J_NOGROUP, J_NOCHALLENGE, J_PROOF, J_WAIT, J_FULL };
static const char *const JOIN_TEXT[] = {"ok", "nogroup", "nochallenge", "proof", "wait", "full"};

// A device answers a challenge: HMAC-SHA256(auth key, challenge) as hex.
inline JoinResult join(const char *nHex, const char *aHex, const char *name, int kind, uint32_t now, Device **out) {
  if (!group.set) return J_NOGROUP;
  if (wrongProofs >= MAX_WRONG && now - wrongAt < WRONG_PAUSE_MS) return J_WAIT;
  uint8_t n[16], a[32];
  if (!fromHex(nHex, n, 16)) return J_NOCHALLENGE;
  Challenge *c = nullptr;
  for (auto &x : chals) if (x.used && now - x.at <= CHALLENGE_MS && !memcmp(x.n, n, 16)) c = &x;
  if (!c) return J_NOCHALLENGE;
  c->used = false;                                            // one answer per challenge
  uint8_t want[32];
  sha::hmac(group.auth, 32, n, 16, want);
  if (!fromHex(aHex, a, 32) || !sha::same(a, want, 32)) {
    if (wrongProofs >= MAX_WRONG) wrongProofs = 0;
    wrongProofs++;
    wrongAt = now;
    return J_PROOF;
  }
  wrongProofs = 0;
  Device *d = nullptr;
  for (auto &x : devices) if (!x.used) { d = &x; break; }
  if (!d) return J_FULL;
  memset(d, 0, sizeof *d);
  d->used = true;
  d->id = group.nextDev++;
  if (!group.nextDev) group.nextDev = 1;
  while (deviceById(group.nextDev)) group.nextDev++;
  d->tok = newToken();
  cleanName(name, d->name);
  if (!d->name[0]) snprintf(d->name, sizeof d->name, "Device %d", d->id);
  d->kind = kind == 1 ? 1 : 0;
  d->seen = now;
  d->online = true;
  bump();
  *out = d;
  return J_OK;
}

inline void forgetDevice(Device &d) {
  const int id = d.id;
  d.used = false;
  for (auto &i : items) if (i.used && i.to == id) removeItem(i);   // nobody else can see them
  bump();
}

// ── items ───────────────────────────────────────────────────────────────────────────────────

enum PutResult { P_OK, P_BIG, P_PAUSED, P_NOTO, P_EMPTY };
static const char *const PUT_TEXT[] = {"ok", "big", "paused", "noto", "empty"};

// Checked before the bytes arrive, so nothing is stored for a send that would be refused.
inline PutResult canPut(const Device &d, uint32_t size, int to) {
  if (!size) return P_EMPTY;
  if (size > BLOB_MAX) return P_BIG;
  if (d.paused) return P_PAUSED;
  if (to && (!deviceById(to) || to == d.id)) return P_NOTO;
  return P_OK;
}
inline PutResult canPut(const Device &d, uint32_t size, int to, int flags) {
  if ((flags & F_OPEN) && !to) return P_NOTO;                   // "open" is always for one computer
  return canPut(d, size, to);
}

// Makes room for `size` more bytes and a new unpinned item: the oldest unpinned go first.
inline void makeRoom(uint32_t size) {
  for (;;) {
    int unpinned = 0;
    Item *oldest = nullptr;
    for (auto &i : items)
      if (i.used && !(i.flags & F_PIN)) { unpinned++; if (!oldest || i.id < oldest->id) oldest = &i; }
    if (!oldest) return;
    if (unpinned < HISTORY && usedBytes + size <= BUDGET) return;
    removeItem(*oldest);
  }
}

// The bytes are already stored under the id this returns (the caller stores them first under a
// temporary name and renames on success). Returns nullptr only if there is no room at all.
inline Item *addItem(Device &d, uint32_t size, int flags, int to, uint32_t now, uint32_t unix) {
  makeRoom(size);
  Item *it = nullptr;
  for (auto &i : items) if (!i.used) { it = &i; break; }
  if (!it || usedBytes + size > BUDGET) return nullptr;          // only pins left, and they fill it
  memset(it, 0, sizeof *it);
  it->used = true;
  it->id = group.nextItem++;
  it->size = size;
  it->from = d.id;
  it->to = to;
  it->flags = flags & (F_ONCE | F_IMAGE | F_OPEN);                // pinned later, by choice
  it->born = unix;
  it->bornMs = now;
  usedBytes += size;
  bump();
  it->ver = group.ver;
  return it;
}

inline uint32_t nextItemId() { return group.nextItem; }

enum PinResult { PIN_OK, PIN_FULL, PIN_GONE, PIN_ONCE };
static const char *const PIN_TEXT[] = {"ok", "full", "gone", "once"};
inline PinResult pin(Item *i, bool on) {
  if (!i) return PIN_GONE;
  if (on && (i->flags & F_ONCE)) return PIN_ONCE;
  if (on && !(i->flags & F_PIN)) {
    int pins = 0;
    for (auto &x : items) pins += x.used && (x.flags & F_PIN);
    if (pins >= MAX_PINS) return PIN_FULL;
  }
  const uint8_t f = on ? (i->flags | F_PIN) : (i->flags & ~F_PIN);
  if (f != i->flags) { i->flags = f; bump(); i->ver = group.ver; }
  return PIN_OK;
}

// The clock: devices going offline, items expiring. `unix` is 0 until the board has the time.
inline void tick(uint32_t now, uint32_t unix) {
  for (auto &d : devices) {
    if (!d.used) continue;
    const bool on = now - d.seen < ONLINE_MS;
    if (on != d.online) { d.online = on; group.ver++; }          // not worth a flash write
  }
  if (unix && group.keepHours) {
    const uint32_t keep = (uint32_t)group.keepHours * 3600;
    for (auto &i : items)
      if (i.used && !(i.flags & F_PIN) && i.born && unix > i.born && unix - i.born > keep) removeItem(i);
  }
  // A one-time item nobody took goes after two minutes.
  for (auto &i : items)
    if (i.used && (i.flags & F_ONCE) && now - i.bornMs > 120000) removeItem(i);
}

// ── saving to flash ─────────────────────────────────────────────────────────────────────────
//
// The group, devices and the item list, in one small record. The item bytes are separate files.

static const uint32_t SAVE_MAGIC = 0x434c5031;   // "CLP1"
struct Saved {
  uint32_t magic;
  Group group;
  Device devices[MAX_DEVICES];
  Item items[MAX_ITEMS];
  uint32_t check;
};

inline uint32_t checksum(const uint8_t *p, size_t n) {
  uint8_t h[32];
  sha::Ctx c;
  sha::init(c); sha::update(c, p, n); sha::final(c, h);
  return (uint32_t)h[0] | (uint32_t)h[1] << 8 | (uint32_t)h[2] << 16 | (uint32_t)h[3] << 24;
}

inline void saveTo(Saved &s) {
  memset(&s, 0, sizeof s);
  s.magic = SAVE_MAGIC;
  s.group = group;
  memcpy(s.devices, devices, sizeof devices);
  memcpy(s.items, items, sizeof items);
  s.check = checksum((const uint8_t *)&s, offsetof(Saved, check));
  dirty = false;
}

// Back from flash after a restart. Items whose bytes are missing are dropped.
inline bool loadFrom(const Saved &s, uint32_t now) {
  if (s.magic != SAVE_MAGIC || s.check != checksum((const uint8_t *)&s, offsetof(Saved, check))) return false;
  group = s.group;
  memcpy(devices, s.devices, sizeof devices);
  memcpy(items, s.items, sizeof items);
  usedBytes = 0;
  uint8_t probe[1];
  for (auto &i : items) {
    if (!i.used) continue;
    if (store && store->read(i.id, probe, 1) < 0) { i.used = false; continue; }
    i.bornMs = now;                    // the uptime clock started again; unix time still holds
    usedBytes += i.size;
  }
  for (auto &d : devices) { d.seen = now; d.online = false; }
  group.ver++;
  return true;
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
  void str(const char *s) {          // names are cleaned on the way in; quote anyway
    if (n + 2 >= cap) return;
    b[n++] = '"';
    for (; *s && n < cap - 4; s++) {
      if (*s == '"' || *s == '\\') { b[n++] = '\\'; b[n++] = *s; }
      else if ((unsigned char)*s < 0x20) b[n++] = ' ';
      else b[n++] = *s;
    }
    b[n++] = '"';
    b[n] = 0;
  }
  void base64(const uint8_t *p, int len) {
    static const char *A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < len && n + 5 < cap; i += 3) {
      const uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < len ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < len ? p[i + 2] : 0);
      b[n++] = A[v >> 18 & 63];
      b[n++] = A[v >> 12 & 63];
      b[n++] = i + 1 < len ? A[v >> 6 & 63] : '=';
      b[n++] = i + 2 < len ? A[v & 63] : '=';
    }
    b[n] = 0;
  }
  int left() const { return cap - n; }
};

// Everything a device needs, or `same` when nothing changed. `ids` is every item it can see now,
// so it forgets the ones that went; `items` are those changed since `since`, oldest change first,
// the small ones with their sealed bytes inline. A long list continues in the next reply ("more").
inline void stateJson(Out &o, const Device &me, uint32_t since, uint32_t epoch) {
  if (since == group.ver && epoch == group.epoch) { o.add("{\"ok\":true,\"same\":true,\"v\":%lu}", (unsigned long)group.ver); return; }
  if (epoch != group.epoch) since = 0;
  o.add("{\"ok\":true,\"v\":@@@@@@@@@@,\"epoch\":%lu,\"me\":%d,\"keep\":%d,\"used\":%lu,\"budget\":%lu,\"devices\":[",
        (unsigned long)group.epoch, me.id, group.keepHours, (unsigned long)usedBytes, (unsigned long)BUDGET);
  // The @ slot is filled in at the end, once it is known whether this reply holds everything:
  // ten characters wide, the number then spaces (which JSON allows), so nothing has to move.
  bool first = true;
  for (auto &d : devices) {
    if (!d.used) continue;
    o.add("%s{\"i\":%d,\"n\":", first ? "" : ",", d.id);
    o.str(d.name);
    o.add(",\"k\":%d,\"on\":%s,\"p\":%s}", d.kind, d.online ? "true" : "false", d.paused ? "true" : "false");
    first = false;
  }
  o.add("],\"ids\":[");
  first = true;
  for (auto &i : items)
    if (i.used && visibleTo(i, me.id)) { o.add("%s%lu", first ? "" : ",", (unsigned long)i.id); first = false; }
  o.add("],\"items\":[");
  uint32_t sent = since;
  bool more = false;
  static uint8_t blob[INLINE_MAX];
  first = true;
  for (;;) {
    Item *next = nullptr;
    for (auto &i : items)
      if (i.used && i.ver > sent && visibleTo(i, me.id) && (!next || i.ver < next->ver)) next = &i;
    if (!next) break;
    const bool inl = next->size <= INLINE_MAX && !(next->flags & F_ONCE);
    const int need = 120 + (inl ? (int)next->size * 4 / 3 + 4 : 0);
    if (!first && o.left() < need + 48) { more = true; break; }
    o.add("%s{\"id\":%lu,\"v\":%lu,\"from\":%d,\"to\":%d,\"f\":%d,\"size\":%lu,\"at\":%lu", first ? "" : ",",
          (unsigned long)next->id, (unsigned long)next->ver, next->from, next->to, next->flags,
          (unsigned long)next->size, (unsigned long)next->born);
    if (inl && store) {
      const int len = store->read(next->id, blob, INLINE_MAX);
      if (len == (int)next->size) { o.add(",\"b\":\""); o.base64(blob, len); o.add("\""); }
    }
    o.add("}");
    sent = next->ver;
    first = false;
  }
  o.add("],\"more\":%s}", more ? "true" : "false");
  // Fill in the version: the whole room's if this reply has everything, else the last one sent.
  char v[12];
  snprintf(v, sizeof v, "%-10lu", (unsigned long)(more ? sent : group.ver));
  char *at = strstr(o.b, "@@@@@@@@@@");
  if (at) memcpy(at, v, 10);
}

// ── the requests ────────────────────────────────────────────────────────────────────────────
//
// Every JSON request, for both of the board's servers and the laptop server. /put and /blob carry
// bytes, so the server handles those itself with canPut()/addItem() and blobFor()/taken().
// arg(key) returns the request's value for key, or "".

template <class G> inline void route(const char *path, G arg, uint32_t now, uint32_t unix, Out &o) {
  auto fail = [&](const char *why) { o.add("{\"ok\":false,\"err\":\"%s\"}", why); };
  (void)unix;
  if (!strcmp(path, "/hello")) {
    char salt[33];
    toHex(group.set ? group.salt : group.pendingSalt, 16, salt);
    o.add("{\"ok\":true,\"set\":%s,\"salt\":\"%s\",\"epoch\":%lu}", group.set ? "true" : "false", salt, (unsigned long)group.epoch);
    return;
  }
  if (!strcmp(path, "/setup")) {
    uint8_t salt[16], auth[32];
    if (group.set) return fail("already");
    if (!fromHex(arg("salt"), salt, 16) || !fromHex(arg("auth"), auth, 32)) return fail("bad");
    if (!setupGroup(salt, auth)) return fail("salt");
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/challenge")) {
    if (!group.set) return fail("nogroup");
    char n[33];
    toHex(newChallenge(now), 16, n);
    o.add("{\"ok\":true,\"n\":\"%s\"}", n);
    return;
  }
  if (!strcmp(path, "/join")) {
    Device *d = nullptr;
    const JoinResult r = join(arg("n"), arg("a"), arg("name"), atoi(arg("kind")), now, &d);
    if (r != J_OK) return fail(JOIN_TEXT[r]);
    o.add("{\"ok\":true,\"t\":\"%08lx\",\"id\":%d}", (unsigned long)d->tok, d->id);
    return;
  }

  Device *me = deviceByToken((uint32_t)strtoul(arg("t"), nullptr, 16));
  if (!me) return fail(group.set ? "notin" : "nogroup");
  if (!me->online) { me->online = true; group.ver++; }
  me->seen = now;

  if (!strcmp(path, "/state"))
    return stateJson(o, *me, (uint32_t)strtoul(arg("v"), nullptr, 10), (uint32_t)strtoul(arg("epoch"), nullptr, 10));
  if (!strcmp(path, "/pin")) {
    Item *i = itemById((uint32_t)strtoul(arg("id"), nullptr, 10));
    if (i && !visibleTo(*i, me->id)) i = nullptr;
    const PinResult r = pin(i, arg("on")[0] == '1');
    if (r != PIN_OK) return fail(PIN_TEXT[r]);
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/del")) {
    Item *i = itemById((uint32_t)strtoul(arg("id"), nullptr, 10));
    if (!i || !visibleTo(*i, me->id)) return fail("gone");
    removeItem(*i);
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/clear")) {                 // the history, not the pins
    for (auto &i : items) if (i.used && !(i.flags & F_PIN) && visibleTo(i, me->id)) removeItem(i);
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/device")) {                // rename, pause or resume any device in the group
    Device *d = arg("id")[0] ? deviceById(atoi(arg("id"))) : me;
    if (!d) return fail("nodevice");
    if (arg("name")[0]) {
      char n[NAME_LEN + 1];
      cleanName(arg("name"), n);
      if (n[0]) memcpy(d->name, n, sizeof n);
    }
    if (arg("paused")[0]) d->paused = arg("paused")[0] == '1';
    bump();
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/forget")) {
    Device *d = deviceById(atoi(arg("id")));
    if (!d) return fail("nodevice");
    forgetDevice(*d);
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/settings")) {
    const int k = atoi(arg("keep"));
    if (k == 0 || k == 1 || k == 24 || k == 168) { group.keepHours = k; bump(); }
    else return fail("keep");
    o.add("{\"ok\":true}");
    return;
  }
  if (!strcmp(path, "/reset")) {                 // a fresh group: every device must set up again
    if (arg("sure")[0] != '1') return fail("sure");
    resetAll();
    o.add("{\"ok\":true}");
    return;
  }
  fail("unknown");
}

// /blob: may this device have item `id`? Its size, or -1.
inline int blobFor(uint32_t tok, uint32_t id, uint32_t now) {
  Device *me = deviceByToken(tok);
  if (!me) return -1;
  me->seen = now;
  Item *i = itemById(id);
  if (!i || !visibleTo(*i, me->id)) return -1;
  return (int)i->size;
}

// After a one-time item reached a device other than its sender, it is gone for everyone.
inline void taken(uint32_t tok, uint32_t id) {
  Device *me = deviceByToken(tok);
  Item *i = itemById(id);
  if (me && i && (i->flags & F_ONCE) && i->from != me->id) removeItem(*i);
}
