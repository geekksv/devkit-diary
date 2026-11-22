#pragma once
//
// Tambola (Housie), refereed by the board.
//
// Everything that decides anything is here: the tickets, the draw, the prizes, the rooms and
// players, the calling clock, and the JSON the phones are sent. No WiFi and no Arduino, so it
// builds on a laptop too. tools/tambola_test.cpp checks the rules, and tools/devserver.cpp
// serves the real page through this very code for the end-to-end tests. tambola.ino only
// passes requests in and replies out.

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// The random source: the chip's hardware generator on the board, anything on a laptop.
typedef uint32_t (*RandFn)();
static RandFn tRand = nullptr;
inline uint32_t rnd(uint32_t n) { return n ? tRand() % n : 0; }

template <class T> inline void shuffle(T *a, int n) {
  for (int i = n - 1; i > 0; i--) { const int j = rnd(i + 1); const T t = a[i]; a[i] = a[j]; a[j] = t; }
}

// ── tickets ─────────────────────────────────────────────────────────────────────────────────
//
// A ticket is 3 rows × 9 columns with 15 numbers: 5 in every row, at least one in every column,
// column 1 holding 1–9, column 2 10–19 … column 9 80–90, each column read top to bottom in order.
// They are made in strips of six that between them use every number 1–90 exactly once, the way
// printed books are, so tickets from one strip never share a number.

struct Ticket { uint8_t n[3][9]; };          // 0 = a blank square

inline int colLo(int c) { return c == 0 ? 1 : c * 10; }
inline int colHi(int c) { return c == 8 ? 90 : c * 10 + 9; }

inline bool makeStrip(Ticket *t) {
  uint8_t pool[9][11], psz[9];
  for (int c = 0; c < 9; c++) {
    psz[c] = 0;
    for (int v = colLo(c); v <= colHi(c); v++) pool[c][psz[c]++] = v;
    shuffle(pool[c], psz[c]);
  }
  // 1. how many numbers each ticket gets from each column: one each to start, then the 36 left
  //    over shared out so every ticket ends with 15 and no column holds more than 3
  uint8_t cnt[6][9], total[6];
  bool ok = false;
  for (int attempt = 0; attempt < 200 && !ok; attempt++) {
    for (int k = 0; k < 6; k++) { total[k] = 9; for (int c = 0; c < 9; c++) cnt[k][c] = 1; }
    uint8_t extra[36];
    int ne = 0;
    for (int c = 0; c < 9; c++) for (int i = 6; i < psz[c]; i++) extra[ne++] = c;
    shuffle(extra, ne);
    ok = true;
    for (int i = 0; i < ne && ok; i++) {
      const int c = extra[i];
      uint8_t cand[6];
      int nc = 0;
      for (int k = 0; k < 6; k++) if (total[k] < 15 && cnt[k][c] < 3) cand[nc++] = k;
      if (!nc) { ok = false; break; }
      const int k = cand[rnd(nc)];
      cnt[k][c]++;
      total[k]++;
    }
  }
  if (!ok) return false;
  // 2. each ticket: which rows each column's numbers sit in, so every row has exactly five
  uint8_t used[9] = {};
  for (int k = 0; k < 6; k++) {
    bool placed = false;
    for (int attempt = 0; attempt < 200 && !placed; attempt++) {
      bool grid[3][9] = {};
      int rows[3] = {0, 0, 0};
      int order[9];
      for (int c = 0; c < 9; c++) order[c] = c;
      shuffle(order, 9);
      for (int a = 0; a < 9; a++)                          // fullest columns first: they have least choice
        for (int b = a + 1; b < 9; b++)
          if (cnt[k][order[b]] > cnt[k][order[a]]) { const int x = order[a]; order[a] = order[b]; order[b] = x; }
      placed = true;
      for (int a = 0; a < 9 && placed; a++) {
        const int c = order[a];
        for (int m = 0; m < cnt[k][c]; m++) {
          int best = -1;
          for (int r = 0; r < 3; r++) {
            if (grid[r][c] || rows[r] >= 5) continue;
            if (best < 0 || rows[r] < rows[best] || (rows[r] == rows[best] && rnd(2))) best = r;
          }
          if (best < 0) { placed = false; break; }
          grid[best][c] = true;
          rows[best]++;
        }
      }
      if (!placed || rows[0] != 5 || rows[1] != 5 || rows[2] != 5) { placed = false; continue; }
      memset(&t[k], 0, sizeof(Ticket));
      for (int c = 0; c < 9; c++) {
        uint8_t v[3];
        for (int m = 0; m < cnt[k][c]; m++) v[m] = pool[c][used[c] + m];
        for (int a = 0; a < cnt[k][c]; a++)                // ascending, top to bottom
          for (int b = a + 1; b < cnt[k][c]; b++) if (v[b] < v[a]) { const uint8_t x = v[a]; v[a] = v[b]; v[b] = x; }
        int m = 0;
        for (int r = 0; r < 3; r++) if (grid[r][c]) t[k].n[r][c] = v[m++];
      }
    }
    if (!placed) return false;
    for (int c = 0; c < 9; c++) used[c] += cnt[k][c];
  }
  return true;
}

// Every rule above, checked (the tests use it on thousands of strips).
inline bool ticketOk(const Ticket &t) {
  int count = 0;
  for (int r = 0; r < 3; r++) {
    int inRow = 0;
    for (int c = 0; c < 9; c++) {
      const int v = t.n[r][c];
      if (!v) continue;
      if (v < colLo(c) || v > colHi(c)) return false;
      inRow++;
      count++;
    }
    if (inRow != 5) return false;
  }
  for (int c = 0; c < 9; c++) {
    int last = 0, inCol = 0;
    for (int r = 0; r < 3; r++) {
      const int v = t.n[r][c];
      if (!v) continue;
      if (v <= last) return false;
      last = v;
      inCol++;
    }
    if (!inCol) return false;
  }
  return count == 15;
}

// ── the draw and the prizes ─────────────────────────────────────────────────────────────────

struct Draw {
  uint8_t seq[90];           // the whole order, decided at the start
  uint8_t count;             // how many have been called
  uint8_t at[91];            // at[n]: when n was called (1 = first), 0 = not yet
  void init() {
    for (int i = 0; i < 90; i++) seq[i] = i + 1;
    shuffle(seq, 90);
    count = 0;
    memset(at, 0, sizeof at);
  }
  int next() {
    if (count >= 90) return 0;
    const int n = seq[count++];
    at[n] = count;
    return n;
  }
  int last() const { return count ? seq[count - 1] : 0; }
};

enum Prize { P_EARLY, P_TOP, P_MIDDLE, P_BOTTOM, P_CORNERS, P_FULL, P_FULL2, P_COUNT };
static const char *const PRIZE_KEYS[P_COUNT] = {"early", "top", "middle", "bottom", "corners", "full", "full2"};
static const uint8_t PRIZE_POINTS[P_COUNT] = {10, 15, 15, 15, 15, 40, 25};
const uint8_t PRIZES_DEFAULT = 0x3F;           // all but the Second Full House

// The numbers a prize needs from a ticket, into out; returns how many must be called.
inline int prizeNumbers(const Ticket &t, int p, uint8_t *out, int *n) {
  *n = 0;
  if (p == P_TOP || p == P_MIDDLE || p == P_BOTTOM) {
    const int r = p - P_TOP;
    for (int c = 0; c < 9; c++) if (t.n[r][c]) out[(*n)++] = t.n[r][c];
    return 5;
  }
  if (p == P_CORNERS) {
    for (int r = 0; r < 3; r += 2) {
      int first = 0, last = 0;
      for (int c = 0; c < 9; c++) if (t.n[r][c]) { if (!first) first = t.n[r][c]; last = t.n[r][c]; }
      out[(*n)++] = first;
      out[(*n)++] = last;
    }
    return 4;
  }
  for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) if (t.n[r][c]) out[(*n)++] = t.n[r][c];
  return p == P_EARLY ? 5 : 15;
}

// Does this ticket have the prize, counting only the first upTo numbers called?
inline bool qualifies(const Ticket &t, int p, const Draw &d, int upTo) {
  uint8_t nums[15];
  int n;
  const int need = prizeNumbers(t, p, nums, &n);
  int have = 0;
  for (int i = 0; i < n; i++) if (d.at[nums[i]] && d.at[nums[i]] <= upTo) have++;
  return have >= need;
}

// ── rooms and players ───────────────────────────────────────────────────────────────────────

const int MAX_ROOMS = 3, MAX_PLAYERS = 30, MAX_TIX = 6, NAME_LEN = 30, MAX_WINNERS = 6, MAX_EVENTS = 12;
const int MAX_TICKETS = 31 * 6;               // 30 players × 6, packed into strips, never runs short
const uint32_t FIRST_MS = 4000;               // from Start to the first number
const uint32_t GRACE_MS = 5000;               // after a win, before the next number: time to share it
const uint32_t HOST_GONE_MS = 60000;          // a silent host hands over to the next player
const uint32_t ROOM_GONE_MS = 15 * 60000;     // a room nobody has looked at this long is closed
const uint32_t EMOJI_MS = 4000;

enum Phase : uint8_t { LOBBY, CALLING, PAUSED, OVER };
enum EventKind : uint8_t { E_WIN = 1, E_BOGEY, E_START, E_OVER, E_PAUSE, E_RESUME, E_HOST };

struct Player {
  uint32_t tok;
  char name[NAME_LEN + 1];
  uint8_t want;               // tickets asked for (1–6)
  uint8_t ntix;
  uint8_t tix[MAX_TIX];       // indexes into Room::tk
  uint32_t seen;
  int16_t total;              // points from earlier rounds
  uint8_t bogey;              // prizes this player can no longer claim (strict mode), as a mask
  uint8_t emoji;
  uint32_t emojiAt;
};
struct PrizeState { uint8_t won, at, nw; uint8_t who[MAX_WINNERS]; };
struct Event { uint16_t id; uint8_t kind, p, who, num; };

struct Room {
  bool used;
  char code[5];
  uint8_t phase, host, np;
  Player pl[MAX_PLAYERS];
  Ticket tk[MAX_TICKETS];
  uint16_t nt, stripAt;       // tickets made so far; where the current strip begins
  uint8_t stripLeft;          // tickets still free in it
  Draw d;
  uint32_t nextAt, endAt, lastSeen;
  uint8_t pace;               // seconds between numbers; 0 = the host taps Next
  uint8_t strict;             // 1: a wrong claim is a Bogey; 0: the Claim button only lights when it's right
  uint8_t lang;               // 0 English calls, 1 English numbers, 2 Hindi
  uint8_t prizes;             // which prizes are on, as a mask
  PrizeState pz[P_COUNT];
  Event ev[MAX_EVENTS];
  uint8_t nev;
  uint16_t evSeq, round;
  uint32_t ver;               // bumps when anyone's tickets change, so phones refetch theirs
};

static Room rooms[MAX_ROOMS];

static const char CODE_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
inline Room *byCode(const char *c) {
  for (auto &r : rooms) if (r.used && !strcmp(r.code, c)) return &r;
  return nullptr;
}
inline int playerOf(const Room &r, uint32_t t) {
  if (!t) return -1;
  for (int i = 0; i < r.np; i++) if (r.pl[i].tok == t) return i;
  return -1;
}
inline uint32_t newToken() { uint32_t t; do t = tRand(); while (!t); return t; }

// A name as typed, made safe: no control characters or markup, single spaces, at most NAME_LEN
// bytes, and never a letter (Hindi, an emoji) cut in half.
inline void cleanName(const char *in, char *out) {
  int o = 0;
  bool sp = false;
  for (const unsigned char *s = (const unsigned char *)in; *s;) {
    const unsigned char c = *s;
    const int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    if (c < 0x20 || c == 0x7f || c == '"' || c == '\\' || c == '<' || c == '>' || (c >= 0x80 && c < 0xC0)) { s++; continue; }
    if (c == ' ') { if (o) sp = true; s++; continue; }
    int ok = 1;
    for (int i = 1; i < len; i++) if ((s[i] & 0xC0) != 0x80) ok = 0;
    if (!ok) { s++; continue; }
    if (o + (sp ? 1 : 0) + len > NAME_LEN) break;
    if (sp) { out[o++] = ' '; sp = false; }
    memcpy(out + o, s, len);
    o += len;
    s += len;
  }
  out[o] = 0;
}

inline void event(Room &r, uint8_t kind, uint8_t p = 0, uint8_t who = 0, uint8_t num = 0) {
  if (r.nev == MAX_EVENTS) { memmove(r.ev, r.ev + 1, sizeof(Event) * (MAX_EVENTS - 1)); r.nev--; }
  r.ev[r.nev++] = {++r.evSeq, kind, p, who, num};
}

// Tickets for one player, all from one strip so they never share a number.
inline bool dealTo(Room &r, Player &p) {
  p.ntix = 0;
  if (r.stripLeft < p.want) {
    if (r.nt + 6 > MAX_TICKETS) return false;
    while (!makeStrip(&r.tk[r.nt])) {}
    r.stripAt = r.nt;
    r.nt += 6;
    r.stripLeft = 6;
  }
  for (int i = 0; i < p.want; i++) p.tix[p.ntix++] = r.stripAt + (6 - r.stripLeft--);
  return true;
}
// Fresh tickets for everyone (a new round, or the pool ran out from people changing their minds).
inline void dealAll(Room &r) {
  r.nt = r.stripAt = 0;
  r.stripLeft = 0;
  for (int i = 0; i < r.np; i++) dealTo(r, r.pl[i]);
  r.ver++;
}
inline void deal(Room &r, Player &p) {
  if (!dealTo(r, p)) dealAll(r);
  r.ver++;
}

inline int addPlayer(Room &r, const char *name, int want, uint32_t now) {
  if (r.np >= MAX_PLAYERS) return -1;
  Player &p = r.pl[r.np];
  memset(&p, 0, sizeof p);
  p.tok = newToken();
  cleanName(name, p.name);
  if (!p.name[0]) snprintf(p.name, sizeof p.name, "Player %d", r.np + 1);
  p.want = want < 1 ? 1 : want > MAX_TIX ? MAX_TIX : want;
  p.seen = now;
  r.np++;
  deal(r, p);
  return r.np - 1;
}

inline Room *newRoom(uint32_t now) {
  Room *r = nullptr;
  for (auto &x : rooms) if (!x.used) { r = &x; break; }
  if (!r) return nullptr;
  memset(r, 0, sizeof(Room));
  r->used = true;
  for (bool clash = true; clash;) {
    for (int i = 0; i < 4; i++) r->code[i] = CODE_CHARS[rnd(sizeof(CODE_CHARS) - 1)];
    r->code[4] = 0;
    clash = false;
    for (auto &x : rooms) if (&x != r && x.used && !strcmp(x.code, r->code)) clash = true;
  }
  r->pace = 8;
  r->prizes = PRIZES_DEFAULT;
  r->lastSeen = now;
  r->round = 1;
  r->d.init();
  return r;
}

// Points for this round so far: a prize's points, split between those who share it.
inline int roundPoints(const Room &r, int who) {
  int pts = 0;
  for (int p = 0; p < P_COUNT; p++) {
    const PrizeState &z = r.pz[p];
    for (int i = 0; i < z.nw; i++) if (z.who[i] == who) pts += (PRIZE_POINTS[p] + z.nw - 1) / z.nw;
  }
  return pts;
}

// The last prize still to be won: the game ends when it goes.
inline int finalPrize(const Room &r) { return (r.prizes >> P_FULL2 & 1) ? P_FULL2 : P_FULL; }

inline void startRound(Room &r, uint32_t now) {
  if (r.phase == OVER) {                       // a new round: same people, new tickets
    for (int i = 0; i < r.np; i++) { r.pl[i].total += roundPoints(r, i); r.pl[i].bogey = 0; }
    memset(r.pz, 0, sizeof r.pz);
    r.round++;
    dealAll(r);
  }
  r.d.init();
  r.phase = CALLING;
  r.nextAt = now + FIRST_MS;
  r.endAt = 0;
  event(r, E_START);
}

inline void callNext(Room &r, uint32_t now) {
  const int n = r.d.next();
  if (!n) { r.phase = OVER; event(r, E_OVER); return; }
  r.nextAt = now + (r.pace ? r.pace : 0) * 1000u;
}

// Called often (each loop on the board): the calling clock, host hand-over, closing old rooms.
inline void tick(uint32_t now) {
  for (auto &r : rooms) {
    if (!r.used) continue;
    if (now - r.lastSeen > ROOM_GONE_MS) { r.used = false; continue; }
    if (r.np && now - r.pl[r.host].seen > HOST_GONE_MS) {
      for (int k = 1; k < r.np; k++) {
        const int i = (r.host + k) % r.np;
        if (now - r.pl[i].seen < 10000) { r.host = i; event(r, E_HOST, 0, i); break; }
      }
    }
    if (r.phase != CALLING) continue;
    if (r.endAt) {                              // the last prize went; a moment for anyone sharing it
      if ((int32_t)(now - r.endAt) >= 0) { r.phase = OVER; r.endAt = 0; event(r, E_OVER); }
      continue;
    }
    if (r.pace && (int32_t)(now - r.nextAt) >= 0) callNext(r, now);
  }
}

enum ClaimResult { C_WON, C_SHARED, C_TAKEN, C_NOT_YET, C_BOGEY, C_OUT, C_OFF, C_CLOSED };
static const char *const CLAIM_TEXT[] = {"won", "shared", "taken", "notyet", "bogey", "out", "off", "closed"};

// A player says "I have it". Only numbers already called count; a prize won on a number can be
// shared by anyone else who has it on that same number, until the next one is called.
inline ClaimResult claim(Room &r, int who, int p, uint32_t now) {
  if (r.phase != CALLING && r.phase != PAUSED) return C_CLOSED;
  if (p < 0 || p >= P_COUNT || !(r.prizes >> p & 1)) return C_OFF;
  if (p == P_FULL2 && !r.pz[P_FULL].won) return C_NOT_YET;
  Player &pl = r.pl[who];
  if (pl.bogey >> p & 1) return C_OUT;
  PrizeState &z = r.pz[p];
  for (int i = 0; i < z.nw; i++) if (z.who[i] == who) return C_TAKEN;
  if (p == P_FULL2) for (int i = 0; i < r.pz[P_FULL].nw; i++) if (r.pz[P_FULL].who[i] == who) return C_TAKEN;
  bool has = false;
  for (int i = 0; i < pl.ntix && !has; i++) has = qualifies(r.tk[pl.tix[i]], p, r.d, r.d.count);
  if (z.won && (z.at != r.d.count || z.nw >= MAX_WINNERS)) return C_TAKEN;
  if (!has) {
    if (!r.strict) return C_NOT_YET;
    pl.bogey |= 1 << p;
    event(r, E_BOGEY, p, who);
    return C_BOGEY;
  }
  const bool shared = z.won;
  z.won = 1;
  z.at = r.d.count;
  z.who[z.nw++] = who;
  event(r, E_WIN, p, who, r.d.last());
  if (r.phase == CALLING) {
    if (p == finalPrize(r)) r.endAt = now + GRACE_MS;
    else if (r.pace && (int32_t)(r.nextAt - (now + GRACE_MS)) < 0) r.nextAt = now + GRACE_MS;
  }
  return shared ? C_SHARED : C_WON;
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
  void str(const char *s) {                     // names are cleaned on the way in; quote anyway
    add("\"");
    for (; *s; s++) {
      if (*s == '"' || *s == '\\') add("\\%c", *s);
      else if ((unsigned char)*s < 0x20) add(" ");
      else add("%c", *s);
    }
    add("\"");
  }
};

inline void ticketJson(Out &o, const Ticket &t) {
  o.add("[");
  for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) o.add("%s%d", r || c ? "," : "", t.n[r][c]);
  o.add("]");
}

// Everything a phone (or the big screen) needs. me < 0: just watching. ver: the phone's copy of
// its tickets, which aren't sent again unless they changed.
inline void stateJson(Out &o, const Room &r, int me, uint32_t ver, uint32_t now) {
  o.add("{\"ok\":true,\"code\":\"%s\",\"phase\":%d,\"round\":%d,\"me\":%d,\"host\":%d,\"pace\":%d,\"strict\":%d,"
        "\"lang\":%d,\"prizes\":%d",
        r.code, r.phase, r.round, me, r.host, r.pace, r.strict, r.lang, r.prizes);
  o.add(",\"next\":%ld", r.phase == CALLING && r.pace && !r.endAt ? (long)(int32_t)(r.nextAt - now) : -1L);
  o.add(",\"called\":[");
  for (int i = 0; i < r.d.count; i++) o.add("%s%d", i ? "," : "", r.d.seq[i]);
  o.add("],\"players\":[");
  for (int i = 0; i < r.np; i++) {
    const Player &p = r.pl[i];
    o.add("%s{\"n\":", i ? "," : "");
    o.str(p.name);
    o.add(",\"t\":%d,\"pts\":%d,\"round\":%d,\"on\":%d,\"bog\":%d", p.want, p.total + roundPoints(r, i),
          roundPoints(r, i), now - p.seen < 12000 ? 1 : 0, p.bogey);
    if (p.emojiAt && now - p.emojiAt < EMOJI_MS) o.add(",\"e\":%d,\"ea\":%lu", p.emoji, (unsigned long)p.emojiAt);
    o.add("}");
  }
  o.add("],\"pz\":[");
  for (int p = 0; p < P_COUNT; p++) {
    const PrizeState &z = r.pz[p];
    o.add("%s{\"k\":\"%s\",\"pts\":%d,\"won\":%d,\"at\":%d,\"who\":[", p ? "," : "", PRIZE_KEYS[p], PRIZE_POINTS[p],
          z.won, z.at);
    for (int i = 0; i < z.nw; i++) o.add("%s%d", i ? "," : "", z.who[i]);
    o.add("]}");
  }
  o.add("],\"ev\":[");
  for (int i = 0; i < r.nev; i++) {
    const Event &e = r.ev[i];
    o.add("%s[%d,%d,%d,%d,%d]", i ? "," : "", e.id, e.kind, e.p, e.who, e.num);
  }
  o.add("],\"ver\":%lu", (unsigned long)r.ver);
  if (me >= 0 && ver != r.ver) {
    const Player &p = r.pl[me];
    o.add(",\"tok\":\"%08lx\",\"tix\":[", (unsigned long)p.tok);
    for (int i = 0; i < p.ntix; i++) { if (i) o.add(","); ticketJson(o, r.tk[p.tix[i]]); }
    o.add("]");
  }
  o.add("}");
}

// ── the requests ────────────────────────────────────────────────────────────────────────────
//
// One entry point for every request, so the board and the laptop server answer identically.
// arg(key) returns the request's value for key, or "".

template <class G> inline void route(const char *path, G arg, uint32_t now, Out &o) {
  auto fail = [&](const char *why) { o.add("{\"ok\":false,\"err\":\"%s\"}", why); };
  if (!strcmp(path, "/rooms")) {               // games you could join, for the start screen
    o.add("{\"rooms\":[");
    bool first = true;
    for (auto &r : rooms) {
      if (!r.used) continue;
      o.add("%s{\"c\":\"%s\",\"phase\":%d,\"n\":%d,\"host\":", first ? "" : ",", r.code, r.phase, r.np);
      o.str(r.np ? r.pl[r.host].name : "");
      o.add("}");
      first = false;
    }
    o.add("]}");
    return;
  }
  if (!strcmp(path, "/create")) {
    Room *r = newRoom(now);
    if (!r) return fail("full");
    const int i = addPlayer(*r, arg("n"), atoi(arg("k")), now);
    r->host = i;
    return stateJson(o, *r, i, 0, now);
  }
  char code[5] = {0};
  for (int i = 0; i < 4 && arg("c")[i]; i++) code[i] = arg("c")[i] >= 'a' && arg("c")[i] <= 'z' ? arg("c")[i] - 32 : arg("c")[i];
  Room *rp = byCode(code);
  if (!rp) return fail("nogame");
  Room &r = *rp;
  r.lastSeen = now;
  const uint32_t tok = (uint32_t)strtoul(arg("t"), nullptr, 16);
  int me = playerOf(r, tok);
  if (me >= 0) r.pl[me].seen = now;
  const bool host = me >= 0 && me == r.host;

  if (!strcmp(path, "/join")) {
    if (me >= 0) return stateJson(o, r, me, 0, now);
    if (r.phase == CALLING || r.phase == PAUSED) return fail("started");
    me = addPlayer(r, arg("n"), atoi(arg("k")), now);
    if (me < 0) return fail("roomfull");
    return stateJson(o, r, me, 0, now);
  }
  if (!strcmp(path, "/state")) return stateJson(o, r, me, (uint32_t)strtoul(arg("v"), nullptr, 10), now);
  if (me < 0) return fail("notplaying");

  if (!strcmp(path, "/tickets")) {             // how many tickets, or a fresh set of the same number
    if (r.phase != LOBBY && r.phase != OVER) return fail("started");
    const int k = atoi(arg("k"));
    Player &p = r.pl[me];
    if (k >= 1 && k <= MAX_TIX) p.want = k;
    deal(r, p);
    return stateJson(o, r, me, 0, now);
  }
  if (!strcmp(path, "/claim")) {
    int p = -1;
    for (int i = 0; i < P_COUNT; i++) if (!strcmp(arg("p"), PRIZE_KEYS[i])) p = i;
    const ClaimResult c = claim(r, me, p, now);
    return o.add("{\"ok\":%s,\"r\":\"%s\"}", c == C_WON || c == C_SHARED ? "true" : "false", CLAIM_TEXT[c]);
  }
  if (!strcmp(path, "/emoji")) {
    r.pl[me].emoji = atoi(arg("e")) & 7;
    r.pl[me].emojiAt = now ? now : 1;
    return o.add("{\"ok\":true}");
  }
  if (!strcmp(path, "/leave")) {
    if (r.phase == CALLING || r.phase == PAUSED) { r.pl[me].seen = 0; return o.add("{\"ok\":true}"); }
    // before or after a game, the seat really goes (and everyone after it moves up one)
    for (int p = 0; p < P_COUNT; p++) {
      PrizeState &z = r.pz[p];
      int k = 0;
      for (int i = 0; i < z.nw; i++) if (z.who[i] != me) z.who[k++] = z.who[i] > me ? z.who[i] - 1 : z.who[i];
      z.nw = k;
    }
    memmove(&r.pl[me], &r.pl[me + 1], sizeof(Player) * (r.np - me - 1));
    r.np--;
    if (!r.np) { r.used = false; return o.add("{\"ok\":true}"); }
    if (r.host > me || r.host >= r.np) r.host = r.host > me ? r.host - 1 : 0;
    r.nev = 0;                                   // old announcements name people by position
    r.ver++;
    return o.add("{\"ok\":true}");
  }

  // the host's controls
  if (!host) return fail("nothost");
  if (!strcmp(path, "/settings")) {
    const char *v;
    if (*(v = arg("pace"))) { const int s = atoi(v); r.pace = s == 0 ? 0 : s < 3 ? 3 : s > 30 ? 30 : s; if (r.pace) r.nextAt = now + r.pace * 1000u; }
    if (r.phase == LOBBY || r.phase == OVER) {
      if (*(v = arg("strict"))) r.strict = atoi(v) ? 1 : 0;
      if (*(v = arg("prizes"))) r.prizes = (atoi(v) & 0x7F) | 1 << P_FULL;   // Full House is always on
    }
    if (*(v = arg("lang"))) { const int l = atoi(v); if (l >= 0 && l <= 2) r.lang = l; }
    return stateJson(o, r, me, 0, now);
  }
  if (!strcmp(path, "/start")) {
    if (r.phase != LOBBY && r.phase != OVER) return fail("started");
    startRound(r, now);
    return stateJson(o, r, me, 0, now);
  }
  if (!strcmp(path, "/next")) {
    if (r.phase != CALLING || r.endAt) return fail("notcalling");
    callNext(r, now);
    return stateJson(o, r, me, r.ver, now);
  }
  if (!strcmp(path, "/pause")) {
    if (r.phase == CALLING && !r.endAt) { r.phase = PAUSED; event(r, E_PAUSE); }
    else if (r.phase == PAUSED) { r.phase = CALLING; r.nextAt = now + 3000; event(r, E_RESUME); }
    return stateJson(o, r, me, r.ver, now);
  }
  if (!strcmp(path, "/end")) {                  // stop this round now; nobody wins what's left
    if (r.phase == CALLING || r.phase == PAUSED) { r.phase = OVER; r.endAt = 0; event(r, E_OVER); }
    return stateJson(o, r, me, r.ver, now);
  }
  if (!strcmp(path, "/again")) {                // back to the lobby with new tickets, scores kept
    if (r.phase != OVER) return fail("notover");
    for (int i = 0; i < r.np; i++) { r.pl[i].total += roundPoints(r, i); r.pl[i].bogey = 0; }
    memset(r.pz, 0, sizeof r.pz);
    r.round++;
    r.phase = LOBBY;
    r.d.init();
    r.nev = 0;
    dealAll(r);
    return stateJson(o, r, me, 0, now);
  }
  fail("unknown");
}
