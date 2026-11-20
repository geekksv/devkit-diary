// Ludo for two to four phones, refereed by the board.
//
// One phone creates a room and gets a four-letter code; up to three more join, or anyone can
// watch. The board is the only one that rolls the die and the only one that decides a move is
// legal — a phone only ever says "roll" or "move token 2". The whole rule engine is in ludo.h
// and is tested apart from all of this; here is the room around it: seats, a turn clock,
// reaction emojis, a spectator view, and the network.
//
// It runs on your home WiFi (ludo.local), or makes its own network devkit-ludo as a fallback.
//
// Board: ESP32 DevKit V1, nothing attached.

#include <WiFi.h>
#include <WebServer.h>
#include <esp_random.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "ludo.h"
#include "ludo_page.h"

const char *HOST = "ludo";
const char *OWN_AP = "devkit-ludo";
const int MAX_ROOMS = 4;
const int NAME_LEN = 12;
const uint32_t SEAT_MS = 60000;        // a seat is released this long after its phone goes quiet
const uint32_t TURN_MS = 25000;        // time for a turn; run it out and the board plays for you
const uint32_t AUTO_MS = 1100;         // pause before the board plays a move that was the only choice
const uint32_t EMOJI_MS = 4000;        // how long a flung emoji lingers in the state

const int EMOJI_COUNT = 8;   // the page holds the glyphs; the board only passes the index

struct Room {
  bool used;
  char code[5];
  uint8_t phase;                       // 0 waiting, 1 playing, 2 over
  uint8_t mode;                        // tokens per player: 4 classic, 2 quick
  uint32_t tok[LUDO_SEATS];            // 0 = empty seat; seat index is the colour
  char name[LUDO_SEATS][NAME_LEN + 1];
  uint32_t seen[LUDO_SEATS];
  uint32_t viewers;                    // a rolling token for the most recent watcher (for its heartbeat)
  LudoState g;
  int8_t first;                        // who opened this game; rotates on a rematch
  uint32_t turnAt;                     // when the current turn began
  bool autoed;                         // the board played the last move on a timeout
  uint8_t rollVal;                     // the last die rolled, so the page can show it
  int8_t rollSeat;                     // who rolled it (-1 before any roll this game)
  uint32_t rollId;                     // bumps on every roll, so the page animates even on a repeat
  uint32_t autoAt;                     // when the board will play the only move on offer (0: none)
  int8_t autoTok;                      // which token that is
  uint16_t wins[LUDO_SEATS];
  // the most recent emoji: who threw it, which, and when
  int8_t emojiSeat;
  uint8_t emojiIdx;
  uint32_t emojiAt;
  uint32_t ver;
};

Room rooms[MAX_ROOMS];
WebServer web(80);

// ── rooms and seats ──────────────────────────────────────────────────────────

static int seated(const Room &r) {
  int n = 0;
  for (int s = 0; s < LUDO_SEATS; s++) if (r.tok[s]) n++;
  return n;
}
static int hostOf(const Room &r) {
  for (int s = 0; s < LUDO_SEATS; s++) if (r.tok[s]) return s;
  return -1;
}

// Two players sit on opposite colours (red and yellow, or green and blue), the way Ludo is played:
// each gets a clear half of the board instead of sharing a corner. If the two are side by side,
// the second moves across — token, name and score go with them, and their phone follows by token.
static void seatAcross(Room &r) {
  if (seated(r) != 2) return;
  const int h = hostOf(r);
  int o = -1;
  for (int s = 0; s < LUDO_SEATS; s++) if (r.tok[s] && s != h) o = s;
  const int to = (h + 2) % LUDO_SEATS;
  if (o == to) return;
  r.tok[to] = r.tok[o];   r.tok[o] = 0;
  memcpy(r.name[to], r.name[o], sizeof r.name[o]); r.name[o][0] = 0;
  r.seen[to] = r.seen[o];
  r.wins[to] = r.wins[o]; r.wins[o] = 0;
  if (r.emojiSeat == o) r.emojiSeat = -1;
}
static bool activeMask(const Room &r, bool out[LUDO_SEATS]) {
  int n = 0;
  for (int s = 0; s < LUDO_SEATS; s++) { out[s] = r.tok[s] != 0; if (out[s]) n++; }
  return n >= 2;
}

static Room *byCode(const char *c) {
  for (int i = 0; i < MAX_ROOMS; i++)
    if (rooms[i].used && !strcmp(rooms[i].code, c)) return &rooms[i];
  return NULL;
}
static uint32_t argToken() { return (uint32_t)strtoul(web.arg("t").c_str(), NULL, 16); }
static int seatOf(const Room &r, uint32_t t) {
  if (!t) return -1;
  for (int s = 0; s < LUDO_SEATS; s++) if (r.tok[s] == t) return s;
  return -1;
}
static void argCode(char *out) {
  String c = web.arg("c"); c.trim(); c.toUpperCase();
  strncpy(out, c.c_str(), 4); out[4] = 0;
}
static const char CODE_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
static void newCode(char *out) {
  do { for (int i = 0; i < 4; i++) out[i] = CODE_CHARS[esp_random() % (sizeof(CODE_CHARS) - 1)]; out[4] = 0; }
  while (byCode(out));
}
static uint32_t newToken() { uint32_t t; do t = esp_random(); while (!t); return t; }
static void cleanName(const String &in, char *out) {
  int o = 0; bool sp = false;
  for (unsigned i = 0; i < in.length() && o < NAME_LEN; i++) {
    const char c = in[i];
    if ((uint8_t)c < 0x20 || c == 0x7f) continue;
    if (c == ' ' || c == '\t') { if (o) sp = true; continue; }
    if (sp && o < NAME_LEN) { out[o++] = ' '; sp = false; }
    if (o < NAME_LEN) out[o++] = c;
  }
  out[o] = 0;
}

static void seat(Room &r, int s, const char *name) {
  r.tok[s] = newToken();
  strncpy(r.name[s], name, NAME_LEN); r.name[s][NAME_LEN] = 0;
  r.seen[s] = millis();
  r.ver++;
}

// A seat empties. Before the game that is all; during it, the player drops out of the rule
// engine (their tokens stay where they fell but take no more turns), the turn moves on if it
// was theirs, and if fewer than two are left the game is over.
static void vacate(Room &r, int s) {
  r.tok[s] = 0; r.name[s][0] = 0; r.ver++;
  if (!seated(r)) { r.used = false; return; }
  if (r.phase != 1) return;
  r.g.active[s] = false;
  if (seated(r) < 2) { r.phase = 2; r.g.phase = LUDO_OVER; r.g.winner = hostOf(r); r.wins[r.g.winner]++; return; }
  if (r.g.turn == s) { ludoPass(r.g); r.turnAt = millis(); r.autoAt = 0; }   // ludoPass skips inactive seats
}

// ── the clock: the board plays for a seat that has run out of time ──────────
// A turn that is not finished within TURN_MS is played by the board: it rolls, and if a move
// is called for it makes a sensible one. This is what keeps a game from stalling on one person.

// Roll the die for whoever's turn it is. Leaves phase MOVE if there is a move, else the turn passes.
static void rollDie(Room &r) {
  const int die = 1 + esp_random() % 6;
  r.rollVal = die; r.rollSeat = r.g.turn; r.rollId++;
  r.autoAt = 0;
  ludoRoll(r.g, die);
  r.turnAt = millis(); r.ver++;
}

// Play a move and settle what follows: the win, the clock, the version.
static bool playMove(Room &r, int seat, int tk) {
  r.autoAt = 0;
  const LudoMove m = ludoMove(r.g, seat, tk);
  if (!m.ok) return false;
  if (m.win) { r.phase = 2; r.wins[seat]++; }
  r.turnAt = millis(); r.ver++;
  return true;
}

// How many genuinely different moves a roll allows. Tokens standing together — all waiting in
// base, or stacked on one square — are interchangeable, so moving any of them is the same move.
static int moveChoices(const LudoState &g, int seat, int *pick) {
  int mv[LUDO_TOKENS], where[LUDO_TOKENS], kinds = 0;
  const int n = ludoLegal(g, seat, g.die, mv);
  for (int i = 0; i < n; i++) {
    const int p = g.prog[seat][mv[i]];
    bool seen = false;
    for (int j = 0; j < kinds; j++) if (where[j] == p) seen = true;
    if (!seen) where[kinds++] = p;
  }
  if (n) *pick = mv[0];
  return kinds;
}

// After a roll with only one real choice, the board plays it itself after a beat, so the number
// is seen first and nobody has to tap the obvious.
static void afterRoll(Room &r) {
  int tk;
  if (r.g.phase == LUDO_MOVE && moveChoices(r.g, r.g.turn, &tk) == 1) {
    r.autoTok = tk;
    r.autoAt = millis() + AUTO_MS;
    if (!r.autoAt) r.autoAt = 1;                  // 0 means "nothing pending"
  }
}

static void checkClock(Room &r) {
  if (r.phase != 1) return;
  if (r.autoAt && (int32_t)(millis() - r.autoAt) >= 0) {   // the obvious move, due now
    if (r.g.phase == LUDO_MOVE) playMove(r, r.g.turn, r.autoTok);
    r.autoAt = 0;
    return;
  }
  if (millis() - r.turnAt < TURN_MS) return;
  // time is up: roll if they hadn't, then make a sensible move for them
  const int s = r.g.turn;
  r.autoed = true;
  if (r.g.phase == LUDO_ROLL) rollDie(r);
  if (r.g.phase == LUDO_MOVE && r.g.turn == s) {
    const int tk = ludoAutoPick(r.g, s, r.g.die);
    if (tk >= 0 && playMove(r, s, tk)) return;
  }
  r.turnAt = millis(); r.ver++;
}

static void sweep() {
  const uint32_t now = millis();
  for (int i = 0; i < MAX_ROOMS; i++) {
    Room &r = rooms[i];
    if (!r.used) continue;
    for (int s = 0; s < LUDO_SEATS; s++)
      if (r.tok[s] && now - r.seen[s] > SEAT_MS) vacate(r, s);
    if (r.used) checkClock(r);
  }
}

// ── the state a page draws ───────────────────────────────────────────────────

static void sendState(const Room &r, int me) {       // me: seat 0..3, or -1 for a viewer
  String o;
  o.reserve(900);
  o += "{\"ok\":true,\"code\":\"";
  o += r.code;
  o += "\",\"mode\":" + String(r.mode);
  o += ",\"phase\":" + String(r.phase);
  o += ",\"me\":" + String(me);
  o += ",\"host\":" + String(hostOf(r));
  o += ",\"names\":[";
  for (int s = 0; s < LUDO_SEATS; s++) { if (s) o += ','; o += "\"" + uiJson(r.name[s]) + "\""; }
  o += "],\"here\":[";
  for (int s = 0; s < LUDO_SEATS; s++) { if (s) o += ','; o += r.tok[s] ? '1' : '0'; }
  o += "],\"wins\":[";
  for (int s = 0; s < LUDO_SEATS; s++) { if (s) o += ','; o += String(r.wins[s]); }
  o += "],\"prog\":[";
  for (int s = 0; s < LUDO_SEATS; s++) {
    if (s) o += ',';
    o += "[";
    for (int t = 0; t < LUDO_TOKENS; t++) { if (t) o += ','; o += String(t < r.mode ? (int)r.g.prog[s][t] : (int)LUDO_FINISH); }
    o += "]";
  }
  o += "]";
  o += ",\"turn\":" + String(r.g.turn);
  o += ",\"die\":" + String(r.g.die);
  o += ",\"rv\":" + String(r.rollVal) + ",\"rs\":" + String(r.rollSeat) + ",\"rid\":" + String(r.rollId);
  o += ",\"gphase\":" + String(r.g.phase);     // 1 roll, 2 move, 3 over
  o += ",\"winner\":" + String(r.g.winner);
  o += ",\"auto\":" + String(r.autoed ? 1 : 0);
  o += ",\"pend\":" + String(r.autoAt && r.g.phase == LUDO_MOVE ? 1 : 0);   // the board is about to play the only move
  // the legal tokens for the viewer, only when it is their turn to move
  o += ",\"movable\":[";
  if (r.phase == 1 && me == r.g.turn && r.g.phase == LUDO_MOVE) {
    int mv[LUDO_TOKENS]; const int n = ludoLegal(r.g, me, r.g.die, mv);
    for (int i = 0; i < n; i++) { if (i) o += ','; o += String(mv[i]); }
  }
  o += "]";
  o += ",\"last\":{\"s\":" + String(r.g.lastSeat) + ",\"k\":" + String(r.g.lastToken)
     + ",\"from\":" + String(r.g.lastFrom) + ",\"to\":" + String(r.g.lastTo) + ",\"caps\":" + String(r.g.lastCaps) + "}";
  o += ",\"clock\":" + String(r.phase == 1 ? (long)(millis() - r.turnAt < TURN_MS ? TURN_MS - (millis() - r.turnAt) : 0) : -1L);
  o += ",\"limit\":" + String((long)TURN_MS);
  const bool liveEmoji = r.emojiSeat >= 0 && millis() - r.emojiAt < EMOJI_MS;
  o += ",\"emoji\":" + String(liveEmoji ? "{\"s\":" + String(r.emojiSeat) + ",\"e\":" + String(r.emojiIdx)
                                        + ",\"at\":" + String(r.emojiAt) + "}" : "null");
  o += ",\"ver\":" + String(r.ver) + "}";
  web.send(200, "application/json", o);
}

static void sendSeat(const Room &r, int s, uint32_t tk) {
  web.send(200, "application/json",
           String("{\"ok\":true,\"code\":\"") + r.code + "\",\"token\":\"" + String(tk, HEX) +
           "\",\"seat\":" + String(s) + "}");
}

// ── endpoints ────────────────────────────────────────────────────────────────

static void fail(const char *e) { web.send(200, "application/json", String("{\"ok\":false,\"err\":\"") + e + "\"}"); }

static void handleRooms() {
  sweep();
  String o = "{\"rooms\":[";
  bool first = true;
  for (int i = 0; i < MAX_ROOMS; i++) {
    const Room &r = rooms[i];
    if (!r.used || r.phase == 1 || seated(r) >= LUDO_SEATS) continue;
    if (!first) o += ','; first = false;
    o += "{\"c\":\"" + String(r.code) + "\",\"m\":" + String(r.mode) + ",\"n\":" + String(seated(r))
       + ",\"h\":\"" + uiJson(r.name[hostOf(r)]) + "\"}";
  }
  o += "]}";
  web.send(200, "application/json", o);
}

static void handleCreate() {
  sweep();
  char name[NAME_LEN + 1]; cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");
  Room *r = NULL;
  for (int i = 0; i < MAX_ROOMS; i++) if (!rooms[i].used) { r = &rooms[i]; break; }
  if (!r) return fail("full");
  char code[5]; newCode(code);
  memset(r, 0, sizeof(Room));
  r->used = true; memcpy(r->code, code, 5);
  r->mode = web.arg("m").toInt() == 1 ? 2 : 4;   // quick = 2 tokens, classic = 4
  r->phase = 0; r->emojiSeat = -1; r->first = 0; r->rollSeat = -1;
  seat(*r, 0, name);
  sendSeat(*r, 0, r->tok[0]);
}

static void handleJoin() {
  sweep();
  char code[5]; argCode(code);
  Room *r = byCode(code);
  if (!r) return fail("nocode");
  const int had = seatOf(*r, argToken());
  if (had >= 0) { r->seen[had] = millis(); return sendSeat(*r, had, r->tok[had]); }
  if (r->phase == 1) return fail("started");
  char name[NAME_LEN + 1]; cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");
  int s = -1;
  if (seated(*r) == 1) { const int across = (hostOf(*r) + 2) % LUDO_SEATS; if (!r->tok[across]) s = across; }
  if (s < 0) for (int i = 0; i < LUDO_SEATS; i++) if (!r->tok[i]) { s = i; break; }
  if (s < 0) return fail("busy");
  seat(*r, s, name);
  sendSeat(*r, s, r->tok[s]);
}

// a viewer gets a token that is not a seat; it only ever reads state
static void handleWatch() {
  sweep();
  char code[5]; argCode(code);
  Room *r = byCode(code);
  if (!r) return fail("nocode");
  const uint32_t v = newToken();
  r->viewers = v;
  web.send(200, "application/json",
           String("{\"ok\":true,\"code\":\"") + r->code + "\",\"token\":\"" + String(v, HEX) + "\",\"seat\":-1}");
}

// find the caller's room and seat (or -1 viewer); refresh a seat's heartbeat
static Room *mine(int &s) {
  sweep();
  char code[5]; argCode(code);
  Room *r = byCode(code);
  if (!r) { fail("gone"); return NULL; }
  s = seatOf(*r, argToken());
  if (s >= 0) r->seen[s] = millis();
  else if (r->viewers != argToken()) { fail("gone"); return NULL; }   // not a seat and not the viewer
  return r;
}

static void handleState() { int s; Room *r = mine(s); if (r) sendState(*r, s); }

static void handleStart() {
  int s; Room *r = mine(s);
  if (!r) return;
  if (r->phase != 0) return fail("started");
  if (s != hostOf(*r)) return fail("host");
  if (seated(*r) < 2) return fail("alone");
  seatAcross(*r);
  bool act[LUDO_SEATS]; activeMask(*r, act);
  ludoBegin(r->g, r->mode, act, s);      // the host opens (the host never moves seat)
  r->phase = 1; r->first = s; r->turnAt = millis(); r->autoed = false;
  r->rollVal = 0; r->rollSeat = -1; r->rollId++; r->autoAt = 0; r->ver++;
  sendState(*r, s);
}

static void handleRoll() {
  int s; Room *r = mine(s);
  if (!r) return;
  if (r->phase != 1 || s != r->g.turn || r->g.phase != LUDO_ROLL) return fail("notturn");
  r->autoed = false;
  rollDie(*r);
  afterRoll(*r);
  sendState(*r, s);
}

static void handleMove() {
  int s; Room *r = mine(s);
  if (!r) return;
  if (r->phase != 1 || s != r->g.turn || r->g.phase != LUDO_MOVE) return fail("notturn");
  const int k = web.arg("k").toInt();
  if (!playMove(*r, s, k)) return fail("illegal");
  r->autoed = false;
  sendState(*r, s);
}

static void handleAgain() {
  int s; Room *r = mine(s);
  if (!r) return;
  if (r->phase != 2) return fail("playing");
  if (seated(*r) < 2) return fail("alone");
  seatAcross(*r);
  s = seatOf(*r, argToken());            // the caller may have moved across
  bool act[LUDO_SEATS]; activeMask(*r, act);
  const int opener = ludoNextSeat(r->g, r->first >= 0 && r->tok[r->first] ? r->first : hostOf(*r));
  ludoBegin(r->g, r->mode, act, opener);
  r->phase = 1; r->first = opener; r->turnAt = millis(); r->autoed = false; r->emojiSeat = -1;
  r->rollVal = 0; r->rollSeat = -1; r->rollId++; r->autoAt = 0; r->ver++;
  sendState(*r, s);
}

static void handleEmoji() {
  int s; Room *r = mine(s);
  if (!r) return;
  if (s < 0) return fail("viewer");          // viewers watch, they do not throw
  const int e = web.arg("e").toInt();
  if (e < 0 || e >= EMOJI_COUNT) return fail("e");
  r->emojiSeat = s; r->emojiIdx = e; r->emojiAt = millis(); r->ver++;
  web.send(200, "application/json", "{\"ok\":true}");
}

static void handleLeave() {
  char code[5]; argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s >= 0) vacate(*r, s);
  web.send(200, "application/json", "{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nludo: starting");

  const wifisetup::Config cfg = {HOST, OWN_AP, "devkit-ludo", "8", true};
  wifisetup::begin(cfg, web);

  web.on("/", [] { web.send_P(200, "text/html; charset=utf-8", LUDO_BODY); });
  web.on("/rooms", handleRooms);
  web.on("/state", handleState);
  web.on("/create", HTTP_POST, handleCreate);
  web.on("/join", HTTP_POST, handleJoin);
  web.on("/watch", HTTP_POST, handleWatch);
  web.on("/start", HTTP_POST, handleStart);
  web.on("/roll", HTTP_POST, handleRoll);
  web.on("/move", HTTP_POST, handleMove);
  web.on("/again", HTTP_POST, handleAgain);
  web.on("/emoji", HTTP_POST, handleEmoji);
  web.on("/leave", HTTP_POST, handleLeave);
  web.onNotFound([] {
    const String home = (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString();
    web.sendHeader("Location", String("http://") + home + "/", true);
    web.send(302, "text/plain", "");
  });
  web.begin();

  Serial.printf("ludo: open http://%s.local or http://%s\n", HOST,
                (wifisetup::portal() ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
  Serial.printf("ludo: free heap %lu bytes, room %u bytes\n", (unsigned long)ESP.getFreeHeap(),
                (unsigned)sizeof(Room));
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  static uint32_t last = 0;
  if (millis() - last >= 200) { last = millis(); sweep(); }
}
