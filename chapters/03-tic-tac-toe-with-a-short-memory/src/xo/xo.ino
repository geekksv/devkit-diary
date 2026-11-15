// Tic-tac-toe for two phones, refereed by the board.
//
// The board makes its own WiFi network and serves the game. One phone creates a room and gets
// a four-letter code; a second phone joins with the code, or taps the room in the open list.
// The board holds every board in RAM and is the only one allowed to decide anything: whose
// turn it is, whether a cell is free, who won. A phone only ever says "I tapped cell 4".
//
// Two variants, picked when the room is created:
//
//   classic    the game everyone knows. Nine cells, three in a row, usually a draw.
//   vanishing  each player may have at most three marks on the board. Placing a fourth wipes
//              your oldest one. The board never fills, so there are no draws: play goes on
//              until someone lines up three. The mark about to go is marked on both phones,
//              and each move has a 15-second clock — run it out and the board moves for you.
//
// Board: ESP32 DevKit V1, nothing attached.
// Join the WiFi below, then open http://192.168.4.1

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>    // esp_random(); NOT reachable via Arduino.h on core 3.x
#include "ui.h"

const char *AP_NAME = "devkit-xo";
const char *AP_PASS = "";          // open; set 8+ characters to lock it

// Same ceiling as chapter 1, for the same reason: the core's DHCP server hands out eight
// leases, so a ninth phone would associate and never get an address. Eight phones is four
// games at once.
const int MAX_STATIONS = 8;
const int MAX_ROOMS = 8;           // > MAX_STATIONS / 2, so a room waiting for its second
                                   // player never blocks a full set of games
const int NAME_LEN = 12;
const uint32_t SEAT_MS = 45000;    // a seat is released this long after its phone goes quiet
const bool LOG_POLLS = false;      // print /rooms and /state too — noisy, for debugging
const uint32_t TURN_MS = 15000;    // vanishing only: the time a player has for each move.
                                   // With no draws possible, this is what stops a game that
                                   // nobody wants to lose from never ending.

enum { CLASSIC = 0, VANISH = 1 };
enum { PLAYING = -1, DRAW = 2 };

static const uint8_t LINES[8][3] = {
  {0, 1, 2}, {3, 4, 5}, {6, 7, 8},   // rows
  {0, 3, 6}, {1, 4, 7}, {2, 5, 8},   // columns
  {0, 4, 8}, {2, 4, 6}               // diagonals
};

// No I and no O: on a phone screen they read as 1 and 0, and in a tic-tac-toe game O is
// already a player. 24^4 is 331,776 codes for at most eight rooms.
static const char CODE_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";

struct Room {
  bool used;
  char code[5];
  uint8_t mode;
  uint32_t tok[2];                 // seat 0 plays X, seat 1 plays O; 0 = empty seat
  char name[2][NAME_LEN + 1];
  uint32_t seen[2];
  int8_t cell[9];                  // -1 empty, 0 X, 1 O
  uint16_t at[9];                  // the move number each mark was placed on — the
                                   // smallest of a player's three is the one that vanishes
  uint16_t ply;                    // moves made this round
  uint8_t starter;                 // who opened this round; alternates every rematch
  int8_t result;                   // PLAYING, 0 or 1 (the winner), or DRAW
  int8_t line;                     // index into LINES of the winning row, -1 if none
  int8_t gone;                     // cell that vanished on the last move, -1 if none
  uint32_t turnAt;                 // when the current turn began — the clock counts from here
  int8_t timedOut;                 // seat whose clock ran out on the last move, -1 if none
  uint16_t wins[2], draws;
  uint32_t ver;                    // bumps on every change, so a page can tell what is new
};

Room rooms[MAX_ROOMS];
uint32_t started = 0;

WebServer web(80);
DNSServer dns;

// ── rooms ────────────────────────────────────────────────────────────────────

static void clearBoard(Room &r) {
  for (int i = 0; i < 9; i++) { r.cell[i] = -1; r.at[i] = 0; }
  r.ply = 0;
  r.result = PLAYING;
  r.line = -1;
  r.gone = -1;
  r.turnAt = millis();
  r.timedOut = -1;
}

static int turnOf(const Room &r) { return (r.starter + r.ply) % 2; }

static int seated(const Room &r) { return (r.tok[0] ? 1 : 0) + (r.tok[1] ? 1 : 0); }

// A seat empties when its phone leaves or stops polling. The round in progress is abandoned —
// a half-played game against a new stranger is not the same game — and the score goes with it.
static void vacate(Room &r, int s) {
  r.tok[s] = 0;
  r.name[s][0] = 0;
  clearBoard(r);
  r.wins[0] = r.wins[1] = r.draws = 0;
  r.starter = 0;
  r.ver++;
  if (!seated(r)) r.used = false;
}

static void checkClock(Room &r);

// Releases seats whose phones went quiet, and runs the move clock. Called on every request
// and from loop(), so a clock runs out on time even when nobody is asking.
static void sweep() {
  const uint32_t now = millis();
  for (int i = 0; i < MAX_ROOMS; i++) {
    Room &r = rooms[i];
    if (!r.used) continue;
    for (int s = 0; s < 2; s++)
      if (r.tok[s] && now - r.seen[s] > SEAT_MS) vacate(r, s);
    if (r.used) checkClock(r);
  }
}

static Room *byCode(const char *c) {
  for (int i = 0; i < MAX_ROOMS; i++)
    if (rooms[i].used && !strcmp(rooms[i].code, c)) return &rooms[i];
  return NULL;
}

static uint32_t argToken() { return (uint32_t)strtoul(web.arg("t").c_str(), NULL, 16); }

// Which seat this token holds in this room, or -1. Every request that changes anything is
// answered by looking the token up here, never by trusting a seat number the page sends.
static int seatOf(const Room &r, uint32_t t) {
  if (!t) return -1;
  if (r.tok[0] == t) return 0;
  if (r.tok[1] == t) return 1;
  return -1;
}

static void argCode(char *out) {
  String c = web.arg("c");
  c.trim();
  c.toUpperCase();
  strncpy(out, c.c_str(), 4);
  out[4] = 0;
}

static void newCode(char *out) {
  do {
    for (int i = 0; i < 4; i++) out[i] = CODE_CHARS[esp_random() % (sizeof(CODE_CHARS) - 1)];
    out[4] = 0;
  } while (byCode(out));
}

static uint32_t newToken() {
  uint32_t t;
  do { t = esp_random(); } while (!t);
  return t;
}

// Trim, collapse whitespace, drop control characters. The other phone shows this name.
static void cleanName(const String &in, char *out) {
  int o = 0;
  bool space = false;
  for (unsigned i = 0; i < in.length() && o < NAME_LEN; i++) {
    const char c = in[i];
    if ((uint8_t)c < 0x20 || c == 0x7f) continue;
    if (c == ' ' || c == '\t') { if (o) space = true; continue; }
    if (space && o < NAME_LEN) { out[o++] = ' '; space = false; }
    if (o < NAME_LEN) out[o++] = c;
  }
  out[o] = 0;
}

// ── the rules ────────────────────────────────────────────────────────────────

// In the vanishing game, the cell a player will lose on their next move — the oldest of their
// marks, once they have three. -1 if nothing goes. This is the whole variant: one comparison
// of move numbers.
static int doomed(const Room &r, int p) {
  if (r.mode != VANISH) return -1;
  int n = 0, oldest = -1;
  for (int i = 0; i < 9; i++) {
    if (r.cell[i] != p) continue;
    n++;
    if (oldest < 0 || r.at[i] < r.at[oldest]) oldest = i;
  }
  return n >= 3 ? oldest : -1;
}

static int winningLine(const Room &r, int p) {
  for (int l = 0; l < 8; l++)
    if (r.cell[LINES[l][0]] == p && r.cell[LINES[l][1]] == p && r.cell[LINES[l][2]] == p)
      return l;
  return -1;
}

// Returns NULL if the move was played, or a short reason if it was refused.
static const char *play(Room &r, int p, int i) {
  if (seated(r) < 2) return "alone";
  if (r.result != PLAYING) return "over";
  if (turnOf(r) != p) return "turn";
  if (i < 0 || i > 8) return "cell";
  if (r.cell[i] != -1) return "taken";

  // Order matters. The old mark goes first, then the new one lands, then the line is
  // checked — so a row that relied on the vanished mark does not count. You cannot win with
  // a mark you no longer have.
  r.gone = doomed(r, p);
  if (r.gone >= 0) r.cell[r.gone] = -1;

  r.cell[i] = p;
  r.at[i] = ++r.ply;
  r.turnAt = millis();
  r.timedOut = -1;

  r.line = winningLine(r, p);
  if (r.line >= 0) {
    r.result = p;
    r.wins[p]++;
  } else if (r.mode == CLASSIC && r.ply == 9) {     // vanishing has no draw: it cannot fill
    r.result = DRAW;
    r.draws++;
  }
  r.ver++;
  return NULL;
}

// ── the clock (vanishing only) ───────────────────────────────────────────────

static bool clockRuns(const Room &r) {
  return r.mode == VANISH && seated(r) == 2 && r.result == PLAYING;
}

static uint32_t timeLeft(const Room &r) {
  const uint32_t used = millis() - r.turnAt;
  return used >= TURN_MS ? 0 : TURN_MS - used;
}

// The clock ran out: the board plays a random free cell for whoever was too slow. A random
// move rather than a forfeit, because the usual reason is a phone that went to sleep for a
// moment, and losing a whole round to that is harsher than the stall deserves. There is always
// a free cell — at most five marks are on the board while someone is choosing.
static void autoMove(Room &r) {
  const int p = turnOf(r);
  int open[9], n = 0;
  for (int i = 0; i < 9; i++) if (r.cell[i] == -1) open[n++] = i;
  if (!n) return;
  play(r, p, open[esp_random() % n]);
  r.timedOut = p;
  Serial.printf("%7lu room %s: clock ran out, the board moved for %c\n",
                (unsigned long)millis(), r.code, p ? 'O' : 'X');
}

static void checkClock(Room &r) {
  if (clockRuns(r) && timeLeft(r) == 0) autoMove(r);
}

// ── endpoints ────────────────────────────────────────────────────────────────

// Every request that changes something is printed on the serial line, with its arguments.
// Polls are left out — two phones would print three lines a second — unless they fail.
static void trace(const char *note) {
  Serial.printf("%7lu %s %s", (unsigned long)millis(),
                web.method() == HTTP_POST ? "POST" : "GET ", web.uri().c_str());
  for (int i = 0; i < web.args(); i++)
    Serial.printf(" %s=%s", web.argName(i).c_str(), web.arg(i).c_str());
  Serial.printf("  -> %s\n", note);
}

static void fail(const char *err) {
  trace(err);
  web.send(200, "application/json", String("{\"ok\":false,\"err\":\"") + err + "\"}");
}

// Everything a page needs to draw the room, from one seat's point of view. About 300 bytes —
// small enough to send whole on every poll rather than working out what changed.
static void sendState(const Room &r, int me) {
  String o;
  o.reserve(360);
  o += "{\"ok\":true,\"code\":\"";
  o += r.code;
  o += "\",\"mode\":" + String(r.mode);
  o += ",\"me\":" + String(me);
  o += ",\"names\":[\"" + uiJson(r.name[0]) + "\",\"" + uiJson(r.name[1]) + "\"]";
  o += ",\"here\":[" + String(r.tok[0] ? 1 : 0) + "," + String(r.tok[1] ? 1 : 0) + "]";
  o += ",\"b\":\"";
  for (int i = 0; i < 9; i++) o += r.cell[i] < 0 ? '.' : (r.cell[i] ? 'o' : 'x');
  o += "\",\"turn\":" + String(turnOf(r));
  o += ",\"res\":" + String(r.result);
  o += ",\"line\":" + String(r.line);
  o += ",\"fade\":[" + String(doomed(r, 0)) + "," + String(doomed(r, 1)) + "]";
  o += ",\"gone\":" + String(r.gone);
  o += ",\"ply\":" + String(r.ply);
  o += ",\"clock\":" + String(clockRuns(r) ? (long)timeLeft(r) : -1L);
  o += ",\"limit\":" + String((long)TURN_MS);
  o += ",\"auto\":" + String(r.timedOut);
  o += ",\"wins\":[" + String(r.wins[0]) + "," + String(r.wins[1]) + "]";
  o += ",\"draws\":" + String(r.draws);
  o += ",\"ver\":" + String(r.ver);
  o += "}";
  web.send(200, "application/json", o);
}

static void sendSeat(const Room &r, int s) {
  trace(r.code);
  web.send(200, "application/json",
           String("{\"ok\":true,\"code\":\"") + r.code + "\",\"token\":\""
           + String(r.tok[s], HEX) + "\",\"seat\":" + String(s) + "}");
}

static void seat(Room &r, int s, const char *name) {
  r.tok[s] = newToken();
  strncpy(r.name[s], name, NAME_LEN);
  r.name[s][NAME_LEN] = 0;
  r.seen[s] = millis();
  r.turnAt = millis();             // the first move's clock starts when the opponent sits down
  r.ver++;
}

// GET /rooms  -> rooms with one player waiting, so a second phone can tap instead of typing
static void handleRooms() {
  if (LOG_POLLS) trace("rooms");
  sweep();
  String o = "{\"rooms\":[";
  bool first = true;
  int open = 0;
  for (int i = 0; i < MAX_ROOMS; i++) {
    const Room &r = rooms[i];
    if (!r.used) { open++; continue; }
    if (seated(r) != 1) continue;
    if (!first) o += ',';
    first = false;
    const int h = r.tok[0] ? 0 : 1;
    o += "{\"c\":\"" + String(r.code) + "\",\"m\":" + String(r.mode)
       + ",\"h\":\"" + uiJson(r.name[h]) + "\"}";
  }
  o += "],\"free\":" + String(open) + ",\"devices\":" + String(WiFi.softAPgetStationNum())
     + ",\"cap\":" + String(MAX_STATIONS) + "}";
  web.send(200, "application/json", o);
}

// POST /create  n=<name>&m=<0|1>   -> { ok, code, token, seat }
static void handleCreate() {
  trace("create requested");
  sweep();
  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");

  Room *r = NULL;
  for (int i = 0; i < MAX_ROOMS; i++) if (!rooms[i].used) { r = &rooms[i]; break; }
  if (!r) return fail("full");

  // The code is picked *before* the slot is marked used, and into a separate buffer. The first
  // version wrote it straight into r->code with r already marked used — so the uniqueness check
  // found this very room holding that very code, every time, and looped forever. The board
  // stopped answering anyone the first time someone pressed "create a room" on real hardware.
  char code[5];
  newCode(code);
  memset(r, 0, sizeof(Room));
  memcpy(r->code, code, sizeof(code));
  r->used = true;
  r->mode = web.arg("m").toInt() == VANISH ? VANISH : CLASSIC;
  clearBoard(*r);
  seat(*r, 0, name);
  sendSeat(*r, 0);
}

// POST /join  c=<code>&n=<name>[&t=<token>]   -> { ok, code, token, seat }
static void handleJoin() {
  sweep();
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  if (!r) return fail("nocode");

  // Already sitting here (a reload, a phone that slept)? Hand the same seat back.
  const int mine = seatOf(*r, argToken());
  if (mine >= 0) { r->seen[mine] = millis(); return sendSeat(*r, mine); }

  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");

  const int s = !r->tok[0] ? 0 : !r->tok[1] ? 1 : -1;
  if (s < 0) return fail("busy");
  seat(*r, s, name);
  sendSeat(*r, s);
}

// GET /state?c=<code>&t=<token>   -- also the heartbeat that keeps a seat held
static void handleState() {
  sweep();
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s < 0) return fail("gone");
  r->seen[s] = millis();
  if (LOG_POLLS) trace("state");
  sendState(*r, s);
}

// POST /move  c=<code>&t=<token>&i=<0..8>
static void handleMove() {
  sweep();
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s < 0) return fail("gone");
  r->seen[s] = millis();
  const String cell = web.arg("i");
  const char *err = play(*r, s, cell.length() ? cell.toInt() : -1);
  if (err) return fail(err);
  trace("ok");
  sendState(*r, s);
}

// POST /again  c=<code>&t=<token>   -- either player may start the next round once one ends
static void handleAgain() {
  sweep();
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s < 0) return fail("gone");
  r->seen[s] = millis();
  if (r->result != PLAYING) {
    r->starter ^= 1;                 // the loser of a coin toss should not lose it forever
    clearBoard(*r);
    r->ver++;
  }
  sendState(*r, s);
}

// POST /leave  c=<code>&t=<token>
static void handleLeave() {
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s >= 0) vacate(*r, s);
  web.send(200, "application/json", "{\"ok\":true}");
}

static const char BODY[] PROGMEM = R"HTML(
<style>
#lobby,#game{display:none}
body.lobby #lobby,body.game #game{display:block}
.jbig{font-size:24px;font-weight:800;letter-spacing:-.03em;margin:0 0 6px}
.err{color:var(--red);font-size:13px;min-height:18px;margin:8px 0 0}
.modes{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:12px 0 14px}
.mode{background:var(--card2);border:1px solid var(--line);border-radius:16px;padding:13px;
  cursor:pointer;text-align:left;color:var(--ink);font-weight:500}
.mode b{display:block;font-size:15px;margin-bottom:3px}
.mode span{font-size:12px;color:var(--mute);line-height:1.45;display:block}
.mode.sel{border-color:rgba(255,184,108,.6);box-shadow:0 0 0 4px rgba(255,184,108,.10)}
.open{display:flex;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid var(--line)}
.open:last-child{border-bottom:0}
.open .code{font-weight:800;letter-spacing:.12em}
.open .who{flex:1;color:var(--mute);font-size:13px}
.open button{width:auto;padding:8px 16px}
#codein{text-transform:uppercase;letter-spacing:.3em;font-weight:800;text-align:center}

.top{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin:0 0 12px}
.top .ghost{margin-left:auto;width:auto;padding:7px 14px;font-size:12.5px}
.codebig{font-size:13px;color:var(--mute)}
.codebig b{color:var(--ink);font-size:17px;letter-spacing:.18em;margin-left:4px}
.score{display:grid;grid-template-columns:1fr auto 1fr;gap:8px;align-items:center;margin:0 0 14px}
.pl{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:10px 12px;
  transition:border-color .2s,box-shadow .2s;min-width:0}
.pl.r{text-align:right}
.pl.turn{border-color:rgba(255,184,108,.55);box-shadow:0 0 0 4px rgba(255,184,108,.10)}
.pl .nm{font-weight:750;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:14px}
.pl .sym{font-weight:900;font-size:13px}
.pl .w{font-size:24px;font-weight:800;line-height:1.1;font-variant-numeric:tabular-nums}
.dr{text-align:center;color:var(--faint);font-size:11px}
.dr b{display:block;font-size:17px;color:var(--mute)}
.x{color:var(--mint)} .o{color:var(--pink)}
#status{text-align:center;font-weight:750;font-size:16px;min-height:26px;margin:0 0 12px}
#grid{display:grid;grid-template-columns:repeat(3,1fr);gap:9px;max-width:380px;margin:0 auto;
  aspect-ratio:1}
.c{aspect-ratio:1;border-radius:18px;background:var(--card);border:1px solid var(--line);
  display:flex;align-items:center;justify-content:center;font-size:min(17vw,72px);font-weight:900;
  padding:0;color:var(--ink);line-height:1;transition:opacity .25s,transform .12s,background .2s}
.c:disabled{background:var(--card);border:1px solid var(--line);cursor:default}
.c.x{color:var(--mint)} .c.o{color:var(--pink)}
.c{position:relative}
.c.fade{opacity:.4;border-style:dashed}
.c.next{border:2px dashed var(--red);animation:throb 1.1s ease-in-out infinite}
.c.next::after{content:'next';position:absolute;top:7px;right:9px;font-size:10px;font-weight:800;
  letter-spacing:.08em;text-transform:uppercase;color:var(--red)}
@keyframes throb{0%,100%{opacity:.75}50%{opacity:.28}}
.clock{display:none;align-items:center;gap:10px;max-width:380px;margin:0 auto 12px}
.clock.on{display:flex}
.clock .track{flex:1;height:8px;border-radius:999px;background:var(--card2);overflow:hidden}
.clock i{display:block;height:100%;border-radius:999px;background:linear-gradient(90deg,var(--mint),#5ec8f0)}
.clock span{font-weight:800;font-variant-numeric:tabular-nums;font-size:13px;min-width:34px;text-align:right}
.clock.low i{background:var(--red)} .clock.low span{color:var(--red)}
#note{text-align:center;color:var(--amber);font-size:13px;min-height:0;margin:-6px 0 10px}
.c.win{background:linear-gradient(135deg,rgba(255,184,108,.22),rgba(255,143,171,.18));
  border-color:rgba(255,184,108,.5)}
.c.pop{animation:pop .25s ease both}
@keyframes pop{from{transform:scale(.6);opacity:0}to{transform:none;opacity:1}}
.c.poof{animation:poof .45s ease both}
@keyframes poof{from{background:rgba(255,107,122,.25)}to{background:var(--card)}}
.rule{text-align:center;color:var(--faint);font-size:12.5px;margin:14px auto 0;max-width:380px}
#again{display:none;max-width:380px;margin:14px auto 0}
#again.show{display:block}
</style>

<header><span class="dot"></span><div>
  <h1>Tic-tac-toe</h1><p class="sub" id="sub">refereed by the board, no internet involved</p>
</div></header>

<!-- ── lobby ───────────────────────────────────────────────────── -->
<div id="lobby">
  <div class="card">
    <p class="jbig">New game</p>
    <input id="nm" placeholder="your name" maxlength="12" autocomplete="off">
    <div class="modes">
      <button class="mode sel" data-m="0" type="button"><b>Classic</b>
        <span>Three in a row. Fill the board and it is a draw.</span></button>
      <button class="mode" data-m="1" type="button"><b>Vanishing</b>
        <span>Three marks each — your fourth wipes your oldest. No draws, 15 s a move.</span></button>
    </div>
    <button id="mk">create a room</button>
    <p class="err" id="mkerr"></p>
  </div>

  <div class="card">
    <p class="label">join a friend</p>
    <div class="row">
      <input id="codein" placeholder="CODE" maxlength="4" autocomplete="off">
      <button class="narrow mint" id="jb">join</button>
    </div>
    <p class="err" id="jerr"></p>
    <div id="opens"></div>
  </div>
</div>

<!-- ── game ────────────────────────────────────────────────────── -->
<div id="game">
  <div class="top">
    <span class="codebig">room<b id="code">----</b></span>
    <span class="pill" id="modepill">classic</span>
    <button class="ghost" onclick="leave()">leave</button>
  </div>
  <div class="score">
    <div class="pl" id="p0"><div class="nm"><span class="sym x">X</span> <span id="n0">—</span></div>
      <div class="w" id="w0">0</div></div>
    <div class="dr"><b id="dr">0</b>draws</div>
    <div class="pl r" id="p1"><div class="nm"><span id="n1">—</span> <span class="sym o">O</span></div>
      <div class="w" id="w1">0</div></div>
  </div>
  <p id="status">—</p>
  <p id="note"></p>
  <div class="clock" id="clock"><div class="track"><i id="ck"></i></div><span id="cks"></span></div>
  <div id="grid"></div>
  <button id="again" class="mint" onclick="again()">play again</button>
  <p class="rule" id="rule"></p>
</div>

<script>
const $=i=>document.getElementById(i);
// Every request gives up after 5 s. Without a limit, a request that never reaches the board —
// a phone sending it over mobile data because this WiFi has no internet — just hangs, and
// the button looks broken rather than telling you why.
function call(u,o){
  const a=new AbortController(),k=setTimeout(()=>a.abort(),5000);
  return fetch(u,Object.assign({signal:a.signal,cache:'no-store'},o||{}))
    .then(r=>r.json()).finally(()=>clearTimeout(k));
}
const get=u=>call(u);
const post=(u,b)=>call(u,{method:'POST',
  headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});
const NOREPLY='The board did not answer. Turn off mobile data and stay on the devkit-xo WiFi.';
const enc=encodeURIComponent;
let code='',token='',mode=0,timer=null,last=null,busy=false;
const WHERE=['top-left','top','top-right','left','centre','right','bottom-left','bottom','bottom-right'];

// The move clock. The board owns it and sends what is left on every poll; in between, the
// page counts down on its own so the bar moves smoothly instead of jumping every 0.7 s.
let deadline=0,limit=15000;
setInterval(()=>{
  if(!deadline)return;
  const left=Math.max(0,deadline-Date.now());
  $('ck').style.width=(left/limit*100)+'%';
  $('cks').textContent=Math.ceil(left/1000)+'s';
  $('clock').classList.toggle('low',left<5000);
},100);

const cells=[];
for(let i=0;i<9;i++){
  const b=document.createElement('button');b.className='c';b.onclick=()=>move(i);
  $('grid').appendChild(b);cells.push(b);
}
document.querySelectorAll('.mode').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.mode').forEach(x=>x.classList.remove('sel'));
  b.classList.add('sel');mode=+b.dataset.m;
});

function store(){try{localStorage.setItem('xo',JSON.stringify({code,token}))}catch(e){}}
function forget(){try{localStorage.removeItem('xo')}catch(e){}}
function myName(){const n=$('nm').value.trim();try{localStorage.setItem('xo_n',n)}catch(e){};return n}

const ERRS={name:'Pick a name first.',full:'Every room on the board is in use.',
  nocode:'No room with that code. Check the letters?',busy:'That room already has two players.',
  gone:'That game has ended.'};

function show(view){document.body.className=view}

function toLobby(msg){
  clearTimeout(timer);code='';token='';last=null;forget();
  show('lobby');$('jerr').textContent=msg||'';
  history.replaceState(null,'','/');lobby();
}

function enter(d){
  code=d.code;token=d.token;store();last=null;
  history.replaceState(null,'','/?c='+code);
  show('game');poll();
}

$('mk').onclick=async()=>{
  const n=myName();if(!n){$('mkerr').textContent=ERRS.name;$('nm').focus();return}
  $('mk').disabled=true;$('mkerr').textContent='';
  try{const d=await post('/create','n='+enc(n)+'&m='+mode);
    d.ok?enter(d):$('mkerr').textContent=ERRS[d.err]||'Could not create a room.'}
  catch(e){$('mkerr').textContent=NOREPLY}
  finally{$('mk').disabled=false}
};

async function join(c){
  const n=myName();if(!n){$('jerr').textContent=ERRS.name;$('nm').focus();return}
  $('jerr').textContent='';
  try{const d=await post('/join','c='+enc(c)+'&n='+enc(n));
    d.ok?enter(d):$('jerr').textContent=ERRS[d.err]||'Could not join.'}
  catch(e){$('jerr').textContent=NOREPLY}
}
$('jb').onclick=()=>{const c=$('codein').value.trim().toUpperCase();if(c.length===4)join(c)};

// The open-room list, refreshed while you sit in the lobby.
async function lobby(){
  if(document.body.className!=='lobby')return;
  try{
    const d=await get('/rooms');
    const box=$('opens');box.innerHTML='';
    if(d.rooms.length){
      const l=document.createElement('p');l.className='label';l.style.marginTop='16px';
      l.textContent='waiting for an opponent';box.appendChild(l);
    }
    for(const r of d.rooms){
      const row=document.createElement('div');row.className='open';
      const c=document.createElement('span');c.className='code';c.textContent=r.c;
      const w=document.createElement('span');w.className='who';
      w.textContent=r.h+' · '+(r.m?'vanishing':'classic');
      const b=document.createElement('button');b.className='mint';b.textContent='join';
      b.onclick=()=>join(r.c);
      row.appendChild(c);row.appendChild(w);row.appendChild(b);box.appendChild(row);
    }
    $('sub').textContent=d.devices+' of '+d.cap+' phones connected';
  }catch(e){$('sub').textContent='cannot reach the board — mobile data off?'}
  setTimeout(lobby,2500);
}

function draw(d){
  const prev=last;last=d;
  const vanish=d.mode===1;
  $('code').textContent=d.code;
  $('modepill').textContent=vanish?'vanishing':'classic';
  for(const s of [0,1]){
    $('n'+s).textContent=d.here[s]?d.names[s]+(d.me===s?' (you)':''):'waiting…';
    $('w'+s).textContent=d.wins[s];
    $('p'+s).classList.toggle('turn',d.res===-1&&d.here[0]&&d.here[1]&&d.turn===s);
  }
  $('dr').textContent=d.draws;
  $('dr').parentNode.style.visibility=vanish?'hidden':'';    // vanishing cannot draw

  const L=[[0,1,2],[3,4,5],[6,7,8],[0,3,6],[1,4,7],[2,5,8],[0,4,8],[2,4,6]];
  const win=d.line>=0?L[d.line]:[];
  const both=d.here[0]&&d.here[1];
  const mine=both&&d.res===-1&&d.turn===d.me;
  const live=both&&d.res===-1;
  // The mark that goes on the *next* move belongs to whoever is about to move: that one is
  // marked loudly. The other player's next-to-go mark is only faded — it matters one move later.
  const next=live?d.fade[d.turn]:-1, later=live?d.fade[1-d.turn]:-1;
  for(let i=0;i<9;i++){
    const ch=d.b[i],c=cells[i];
    const was=prev&&prev.code===d.code?prev.b[i]:ch;
    c.textContent=ch==='.'?'':ch.toUpperCase();
    c.className='c'+(ch==='.'?'':' '+ch)
      +(i===next?' next':i===later?' fade':'')
      +(win.includes(i)?' win':'')
      +(ch!=='.'&&was!==ch?' pop':'')
      +(ch==='.'&&was!=='.'&&d.ver!==prev.ver&&d.ply>0?' poof':'');
    c.disabled=!mine||ch!=='.';
  }

  const opp=d.me?0:1,oname=d.names[opp]||'them';
  let s;
  if(!both)s='waiting for an opponent — room code '+d.code;
  else if(d.res===d.me)s='you win';
  else if(d.res===opp)s=oname+' wins';
  else if(d.res===2)s='draw';
  else if(mine)s='your turn — you are '+(d.me?'O':'X');
  else s=oname+' is thinking…';
  $('status').textContent=s;
  $('again').classList.toggle('show',both&&d.res!==-1);
  const sym=d.me?'O':'X',osym=d.me?'X':'O';
  let rule;
  if(!vanish)rule='Three in a row wins. A full board is a draw.';
  else if(live&&mine&&next>=0)rule='Your '+sym+' in the '+WHERE[next]+' (marked next) vanishes when you move.';
  else if(live&&!mine&&next>=0)rule='Their '+osym+' in the '+WHERE[next]+' vanishes on their move.';
  else rule='Three marks each — your fourth wipes your oldest. No draws. 15 s a move, or the board moves for you.';
  $('rule').textContent=rule;

  $('note').textContent=live&&d.auto>=0
    ?(d.auto===d.me?'Time ran out — the board moved for you.':oname+' ran out of time — the board moved for them.'):'';
  if(d.clock>=0){deadline=Date.now()+d.clock;limit=d.limit;$('clock').classList.add('on')}
  else{deadline=0;$('clock').classList.remove('on')}
  $('sub').textContent=both?'two players · board-refereed':'share the code, or they can tap it in the open list';
}

async function poll(){
  clearTimeout(timer);
  if(!code)return;
  try{
    const d=await get('/state?c='+enc(code)+'&t='+enc(token));
    if(!d.ok){toLobby('The game ended — the board restarted, or the room timed out.');return}
    if(!busy)draw(d);
  }catch(e){$('sub').textContent='lost the board — retrying'}
  timer=setTimeout(poll,700);
}

async function move(i){
  if(busy)return;busy=true;
  cells.forEach(c=>c.disabled=true);
  try{
    const d=await post('/move','c='+enc(code)+'&t='+enc(token)+'&i='+i);
    if(d.ok)draw(d);else if(d.err==='gone'){busy=false;toLobby(ERRS.gone);return}
  }catch(e){}
  busy=false;poll();
}

async function again(){
  try{const d=await post('/again','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}
}

async function leave(){
  try{await post('/leave','c='+enc(code)+'&t='+enc(token))}catch(e){}
  toLobby('');
}

// A reload or a phone waking from sleep should land back in the same seat.
(async function start(){
  try{$('nm').value=localStorage.getItem('xo_n')||''}catch(e){}
  let saved=null;try{saved=JSON.parse(localStorage.getItem('xo')||'null')}catch(e){}
  if(saved&&saved.code&&saved.token){
    try{
      const d=await post('/join','c='+enc(saved.code)+'&t='+enc(saved.token));
      if(d.ok){enter(d);return}
    }catch(e){}
    forget();
  }
  const q=new URLSearchParams(location.search).get('c');
  if(q)$('codein').value=q.toUpperCase();
  show('lobby');lobby();
})();
</script>
)HTML";

static void handlePage() {
  trace("page");
  uiSend(web, "Tic-tac-toe", BODY);
}

// Every other address — a phone's "is there internet?" probe, a site typed by habit — is sent
// to the board's own address rather than answered in place.
//
// This was a real bug, found on the first flash. Serving the page *as* Android's probe URL
// made the page believe it lived at connectivitycheck.gstatic.com, so its requests went to
// that name — which the phone then resolved to Google's real address, through its own secure
// DNS, not ours. The page loaded and the button did nothing: "create a room" never reached
// the board. Redirected, the page lives at a bare IP, and no name lookup is involved at all.
static void handleElse() {
  const String home = WiFi.softAPIP().toString();
  if (web.hostHeader() == home && web.uri() == "/") return handlePage();
  trace("redirect");
  web.sendHeader("Location", String("http://") + home + "/", true);
  web.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  started = millis();

  // Phones arriving, getting an address, and leaving, on the serial line. When "I joined the
  // WiFi but the page will not open", this says which of those three never happened.
  WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t info) {
    if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED)
      Serial.printf("%7lu phone joined  (%d connected)\n", (unsigned long)millis(),
                    WiFi.softAPgetStationNum());
    else if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED)
      Serial.printf("%7lu phone left    (%d connected)\n", (unsigned long)millis(),
                    WiFi.softAPgetStationNum());
    else if (e == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
      Serial.printf("%7lu phone got address " IPSTR "\n", (unsigned long)millis(),
                    IP2STR(&info.wifi_ap_staipassigned.ip));
  });

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, strlen(AP_PASS) ? AP_PASS : NULL, 1, 0, MAX_STATIONS);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  web.on("/", handleElse);
  web.on("/rooms", handleRooms);
  web.on("/state", handleState);
  web.on("/create", HTTP_POST, handleCreate);
  web.on("/join", HTTP_POST, handleJoin);
  web.on("/move", HTTP_POST, handleMove);
  web.on("/again", HTTP_POST, handleAgain);
  web.on("/leave", HTTP_POST, handleLeave);
  web.onNotFound(handleElse);
  web.begin();

  Serial.printf("\njoin \"%s\" then open http://%s\n", AP_NAME, ip.toString().c_str());
  Serial.printf("free heap %lu bytes, %d rooms of %u bytes each\n",
                (unsigned long)ESP.getFreeHeap(), MAX_ROOMS, (unsigned)sizeof(Room));
}

void loop() {
  dns.processNextRequest();
  web.handleClient();
  static uint32_t lastSweep = 0;
  if (millis() - lastSweep >= 200) { lastSweep = millis(); sweep(); }
}
