#pragma once
// The ad blocker's brain, with nothing from Arduino in it, so a laptop can test it
// (tools/filter_test.cpp). noads.ino moves the packets; this file decides what happens to them.
//
// A DNS question ("where is ads.example.com?") arrives. If the name, or any name it sits under,
// is on the blocklist, the board answers at once: "0.0.0.0", nowhere, and the ad never loads.
// Otherwise the question goes to a real DNS server and its answer comes back through the board,
// remembered for a while so the next phone asking gets it instantly.
//
// The blocklist is ~100,000 names. They don't fit in memory as text, so each is kept as a 6-byte
// fingerprint (a hash), sorted, in the board's flash (blocklist.h). Two different names sharing a
// fingerprint is a one-in-a-billion event per lookup.

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ── names and fingerprints ───────────────────────────────────────────────────

static const int NAME_MAX_LEN = 253;

// Lower-case, no trailing dot. False for anything that isn't a plausible domain name.
inline bool normalizeName(const char *in, char *out, int cap) {
  int n = 0;
  for (; *in && n < cap - 1; in++) {
    const unsigned char c = *in;
    if (c <= ' ' || c == 0x7f) return false;
    out[n++] = (char)tolower(c);
  }
  if (*in) return false;
  while (n && out[n - 1] == '.') n--;
  out[n] = 0;
  return n > 0 && n <= NAME_MAX_LEN && out[0] != '.';
}

// 64-bit FNV-1a, then a mixer, keeping 48 bits. The list builder (tools/build_list.py and the
// board) must use exactly this.
inline uint64_t fingerprint(const char *name, int len = -1) {
  uint64_t h = 1469598103934665603ULL;
  for (int i = 0; len < 0 ? name[i] != 0 : i < len; i++) {
    h ^= (uint8_t)tolower((unsigned char)name[i]);
    h *= 1099511628211ULL;
  }
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return h >> 16;                 // 48 bits
}

// ── a small list of names (yours: always allowed, always blocked) ───────────

static const int USER_NAMES = 32;

struct NameList {
  char names[USER_NAMES][64];
  int n;
  bool has(const char *name) const {          // the name itself, or anything under it
    const int len = strlen(name);
    for (int i = 0; i < n; i++) {
      const int l = strlen(names[i]);
      if (l == len && !strcmp(name, names[i])) return true;
      if (len > l && name[len - l - 1] == '.' && !strcmp(name + len - l, names[i])) return true;
    }
    return false;
  }
  bool add(const char *raw) {
    char nm[64];
    if (!normalizeName(raw, nm, sizeof nm) || !strchr(nm, '.')) return false;
    for (int i = 0; i < n; i++)
      if (!strcmp(names[i], nm)) return true;
    if (n >= USER_NAMES) return false;
    strcpy(names[n++], nm);
    return true;
  }
  bool remove(const char *raw) {
    char nm[64];
    if (!normalizeName(raw, nm, sizeof nm)) return false;
    for (int i = 0; i < n; i++)
      if (!strcmp(names[i], nm)) {
        memmove(names[i], names[i + 1], (n - i - 1) * sizeof names[0]);
        n--;
        return true;
      }
    return false;
  }
};

// ── deciding ─────────────────────────────────────────────────────────────────

enum Verdict : uint8_t { ALLOW, BLOCK_LIST, BLOCK_YOURS, ALLOW_YOURS, BLOCK_SPECIAL };

// `listHas` looks a fingerprint up in the big list (blocklist.h on the board, a set on a laptop).
// The name and every name above it are checked ("x.ads.example.com", "ads.example.com",
// "example.com"), but never a bare top-level name like "com".
template <class L> inline Verdict decide(const char *name, const NameList &allow, const NameList &block, L listHas,
                                         bool paused) {
  // Firefox turns its own DNS-over-HTTPS off when this name doesn't exist, which keeps it using
  // the board (Mozilla's documented "canary domain").
  if (!strcmp(name, "use-application-dns.net")) return BLOCK_SPECIAL;
  if (allow.has(name)) return ALLOW_YOURS;
  if (block.has(name)) return BLOCK_YOURS;
  if (paused) return ALLOW;
  for (const char *p = name; p; p = strchr(p, '.') ? strchr(p, '.') + 1 : nullptr) {
    if (!strchr(p, '.')) break;                          // a bare "com"
    if (listHas(fingerprint(p))) return BLOCK_LIST;
  }
  return ALLOW;
}

// ── DNS packets ──────────────────────────────────────────────────────────────

static const int DNS_MAX = 1500;
enum : uint16_t { T_A = 1, T_AAAA = 28, T_HTTPS = 65, T_SVCB = 64 };

struct Question {
  uint16_t id;
  char name[NAME_MAX_LEN + 2];
  uint16_t type, klass;
  int end;                       // where the question ends in the packet
};

inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
inline void wr16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }

// Reads a standard query with exactly one question. False for anything else (responses, odd
// opcodes, compressed or malformed names): those are dropped, never forwarded blind.
inline bool parseQuery(const uint8_t *b, int n, Question &q) {
  if (n < 17) return false;
  const uint16_t flags = rd16(b + 2);
  if (flags & 0x8000) return false;                     // a response, not a question
  if ((flags >> 11) & 0xF) return false;                // only standard queries
  if (rd16(b + 4) != 1) return false;                   // exactly one question
  int at = 12, len = 0;
  for (;;) {
    if (at >= n) return false;
    const uint8_t l = b[at++];
    if (l == 0) break;
    if (l & 0xC0) return false;                         // no compression in a question
    if (at + l > n || len + l + 1 > NAME_MAX_LEN) return false;
    if (len) q.name[len++] = '.';
    for (int i = 0; i < l; i++) {
      const unsigned char c = b[at + i];
      if (c <= ' ' || c == '.' || c == 0x7f) return false;
      q.name[len++] = (char)tolower(c);
    }
    at += l;
  }
  if (!len || at + 4 > n) return false;
  q.name[len] = 0;
  q.id = rd16(b);
  q.type = rd16(b + at);
  q.klass = rd16(b + at + 2);
  q.end = at + 4;
  return true;
}

// The answer for a blocked name, built from the question: A gets 0.0.0.0, AAAA gets ::, any other
// type gets an empty answer. `nx` answers "no such name" instead (for the Firefox canary).
// Returns its length.
inline int blockedAnswer(const uint8_t *query, const Question &q, uint8_t *out, bool nx = false) {
  memcpy(out, query, q.end);
  const uint16_t rd = rd16(query + 2) & 0x0100;          // copy "recursion desired"
  wr16(out + 2, 0x8000 | rd | 0x0080 | (nx ? 3 : 0));   // a response, recursion available, NOERROR / NXDOMAIN
  wr16(out + 4, 1);
  wr16(out + 8, 0);
  wr16(out + 10, 0);
  int n = q.end;
  const bool a = q.type == T_A && !nx, aaaa = q.type == T_AAAA && !nx;
  wr16(out + 6, (a || aaaa) ? 1 : 0);
  if (a || aaaa) {
    wr16(out + n, 0xC00C);                               // the name: "same as the question"
    wr16(out + n + 2, q.type);
    wr16(out + n + 4, 1);                                // IN
    out[n + 6] = 0; out[n + 7] = 0; wr16(out + n + 8, 60);   // TTL 60 s: unblocking takes effect soon
    const int dl = a ? 4 : 16;
    wr16(out + n + 10, dl);
    memset(out + n + 12, 0, dl);
    n += 12 + dl;
  }
  return n;
}

// The smallest TTL in an upstream answer, for how long to keep it. 0 if it can't be read.
inline uint32_t answerTtl(const uint8_t *b, int n) {
  if (n < 12) return 0;
  const int qd = rd16(b + 4), an = rd16(b + 6);
  int at = 12;
  auto skipName = [&]() {
    while (at < n) {
      const uint8_t l = b[at];
      if (l == 0) { at++; return true; }
      if ((l & 0xC0) == 0xC0) { at += 2; return true; }
      at += 1 + l;
    }
    return false;
  };
  for (int i = 0; i < qd; i++) {
    if (!skipName()) return 0;
    at += 4;
  }
  uint32_t best = 0xFFFFFFFF;
  for (int i = 0; i < an; i++) {
    if (!skipName() || at + 10 > n) return 0;
    const uint32_t ttl = (uint32_t)b[at + 4] << 24 | (uint32_t)b[at + 5] << 16 | (uint32_t)b[at + 6] << 8 | b[at + 7];
    if (ttl < best) best = ttl;
    at += 10 + rd16(b + at + 8);
  }
  if (at > n) return 0;
  if (!an) return 60;                                    // "no such record": keep a minute
  return best;
}

// ── the cache of upstream answers ────────────────────────────────────────────

static const int CACHE = 48, CACHE_BYTES = 512;   // ~25 KB: the board needs the rest for downloading the list

struct CacheEntry {
  uint64_t key;                  // fingerprint of the name, mixed with the type
  uint32_t until;                // ms
  uint32_t used;
  uint16_t len;
  uint8_t data[CACHE_BYTES];
};

struct Cache {
  CacheEntry e[CACHE];
  static uint64_t keyOf(const Question &q) { return fingerprint(q.name) * 31 + q.type + 1; }
  // Copies a cached answer for q into out, with q's id. Returns its length, or 0.
  int get(const Question &q, uint32_t now, uint8_t *out) {
    const uint64_t k = keyOf(q);
    for (auto &x : e)
      if (x.len && x.key == k && (int32_t)(x.until - now) > 0) {
        memcpy(out, x.data, x.len);
        wr16(out, q.id);
        x.used = now;
        return x.len;
      }
    return 0;
  }
  void put(const Question &q, const uint8_t *ans, int n, uint32_t now) {
    const int rcode = n >= 12 ? (rd16(ans + 2) & 0xF) : -1;
    if (n > CACHE_BYTES || n < 12 || (rcode != 0 && rcode != 3)) return;   // too big, or a server error
    uint32_t ttl = answerTtl(ans, n);
    if (!ttl) return;
    if (ttl > 3600) ttl = 3600;
    const uint64_t k = keyOf(q);
    CacheEntry *slot = &e[0];
    for (auto &x : e) {
      if (x.len && x.key == k) { slot = &x; break; }
      if (!x.len || (int32_t)(x.until - now) <= 0) { slot = &x; break; }
      if (x.used < slot->used) slot = &x;
    }
    slot->key = k;
    slot->until = now + ttl * 1000;
    slot->used = now;
    slot->len = (uint16_t)n;
    memcpy(slot->data, ans, n);
  }
  void clear() { memset(e, 0, sizeof e); }
};

// ── statistics for the dashboard ─────────────────────────────────────────────

static const int TOP = 15, CLIENTS = 16, RECENT = 40, MINUTES = 60;

struct TopName {                 // "space-saving" top-k: a fixed table that keeps the heavy hitters
  char name[64];
  uint32_t count;
};

struct Recent {
  char name[64];
  uint32_t client;
  uint32_t at;                   // ms
  uint8_t verdict;               // Verdict, or 9 for "from the cache"
};

struct Stats {
  uint32_t total, blocked, cached, forwarded, failed;
  uint16_t perMinute[MINUTES], blockedPerMinute[MINUTES];
  uint32_t minuteNow;            // which minute perMinute[minuteNow % MINUTES] is
  TopName top[TOP];
  struct { uint32_t ip, total, blocked; } clients[CLIENTS];
  Recent recent[RECENT];
  uint32_t recentSeq;
};

inline void rollMinutes(Stats &s, uint32_t now) {
  const uint32_t m = now / 60000;
  if (m == s.minuteNow) return;
  const uint32_t steps = m - s.minuteNow > MINUTES ? MINUTES : m - s.minuteNow;
  for (uint32_t i = 1; i <= steps; i++) {
    s.perMinute[(s.minuteNow + i) % MINUTES] = 0;
    s.blockedPerMinute[(s.minuteNow + i) % MINUTES] = 0;
  }
  s.minuteNow = m;
}

inline void countTop(Stats &s, const char *name) {
  int low = 0;
  for (int i = 0; i < TOP; i++) {
    if (s.top[i].count && !strncmp(s.top[i].name, name, sizeof s.top[i].name - 1)) { s.top[i].count++; return; }
    if (s.top[i].count < s.top[low].count) low = i;
  }
  // replace the smallest, inheriting its count (the space-saving rule: never under-counts a
  // heavy hitter, over-counts newcomers by at most the smallest count)
  strncpy(s.top[low].name, name, sizeof s.top[low].name - 1);
  s.top[low].name[sizeof s.top[low].name - 1] = 0;
  s.top[low].count++;
}

// One answered question. `fromCache` marks an allowed name answered from the cache.
inline void record(Stats &s, const char *name, uint32_t client, Verdict v, bool fromCache, uint32_t now) {
  rollMinutes(s, now);
  const bool blocked = v == BLOCK_LIST || v == BLOCK_YOURS || v == BLOCK_SPECIAL;
  s.total++;
  s.perMinute[s.minuteNow % MINUTES]++;
  if (blocked) {
    s.blocked++;
    s.blockedPerMinute[s.minuteNow % MINUTES]++;
    if (v != BLOCK_SPECIAL) countTop(s, name);
  } else if (fromCache) s.cached++;
  else s.forwarded++;
  int slot = -1, low = 0;
  for (int i = 0; i < CLIENTS; i++) {
    if (s.clients[i].total && s.clients[i].ip == client) { slot = i; break; }
    if (s.clients[i].total < s.clients[low].total) low = i;
  }
  if (slot < 0) {
    slot = low;
    s.clients[slot].ip = client;
    s.clients[slot].total = s.clients[slot].blocked = 0;
  }
  s.clients[slot].total++;
  if (blocked) s.clients[slot].blocked++;
  Recent &r = s.recent[s.recentSeq++ % RECENT];
  strncpy(r.name, name, sizeof r.name - 1);
  r.name[sizeof r.name - 1] = 0;
  r.client = client;
  r.at = now;
  r.verdict = fromCache && !blocked ? 9 : v;
}

// ── JSON for the dashboard ───────────────────────────────────────────────────

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

inline void ipText(uint32_t ip, char *out) {   // ip as stored by lwIP: first octet in the low byte
  snprintf(out, 16, "%u.%u.%u.%u", (unsigned)(ip & 255), (unsigned)(ip >> 8 & 255), (unsigned)(ip >> 16 & 255),
           (unsigned)(ip >> 24));
}

inline void statsJson(Out &o, Stats &s, uint32_t now) {
  rollMinutes(s, now);
  o.add("\"total\":%lu,\"blocked\":%lu,\"cached\":%lu,\"forwarded\":%lu,\"failed\":%lu,\"minutes\":[",
        (unsigned long)s.total, (unsigned long)s.blocked, (unsigned long)s.cached, (unsigned long)s.forwarded,
        (unsigned long)s.failed);
  for (int i = MINUTES - 1; i >= 0; i--) {          // oldest first
    const uint32_t m = (s.minuteNow + MINUTES - i) % MINUTES;
    o.add(i == MINUTES - 1 ? "[%u,%u]" : ",[%u,%u]", s.perMinute[m], s.blockedPerMinute[m]);
  }
  o.add("],\"top\":[");
  // sorted by count, biggest first
  int order[TOP], k = 0;
  for (int i = 0; i < TOP; i++)
    if (s.top[i].count) order[k++] = i;
  for (int i = 1; i < k; i++)
    for (int j = i; j > 0 && s.top[order[j]].count > s.top[order[j - 1]].count; j--) {
      const int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
    }
  for (int i = 0; i < k; i++) {
    o.add(i ? ",[" : "[");
    o.str(s.top[order[i]].name);
    o.add(",%lu]", (unsigned long)s.top[order[i]].count);
  }
  o.add("],\"clients\":[");
  bool first = true;
  for (auto &c : s.clients) {
    if (!c.total) continue;
    char ip[16];
    ipText(c.ip, ip);
    o.add(first ? "[\"%s\",%lu,%lu]" : ",[\"%s\",%lu,%lu]", ip, (unsigned long)c.total, (unsigned long)c.blocked);
    first = false;
  }
  o.add("],\"recent\":[");
  const uint32_t from = s.recentSeq > RECENT ? s.recentSeq - RECENT : 0;
  first = true;
  for (uint32_t i = s.recentSeq; i-- > from;) {   // newest first
    const Recent &r = s.recent[i % RECENT];
    char ip[16];
    ipText(r.client, ip);
    o.add(first ? "[" : ",[");
    o.str(r.name);
    o.add(",\"%s\",%u,%lu]", ip, r.verdict, (unsigned long)((now - r.at) / 1000));
    first = false;
  }
  o.add("]");
}
