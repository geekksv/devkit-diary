// Dots and boxes for two to four phones, refereed by the board.
//
// One phone creates a room, picks a board size and gets a four-letter code; up to three more
// join with the code or from the open list, and the host starts the game. Players take turns
// drawing one line between two neighbouring dots. Draw the fourth side of a box and it is
// yours — and you move again. When every line is drawn, most boxes wins.
//
// As in chapter 3, the board holds the only real copy of every game. A phone only ever says
// "I drew line 17"; whose turn it is, which boxes closed and who goes next are decided here.
//
// Board: ESP32 DevKit V1, nothing attached.
// Join the WiFi below, then open http://192.168.4.1

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>    // esp_random(); NOT reachable via Arduino.h on core 3.x
#include "ui.h"

const char *AP_NAME = "devkit-boxes";
const char *AP_PASS = "";          // open; set 8+ characters to lock it

const int MAX_STATIONS = 8;        // the DHCP ceiling from chapter 1
const int MAX_ROOMS = 6;
const int SEATS = 4;
const int NAME_LEN = 12;
const uint32_t SEAT_MS = 45000;    // a seat is released this long after its phone goes quiet
const uint32_t TURN_MS = 30000;    // time for each move; run it out and the board moves for you

// Three boards: 4×4 boxes (quick), 6×6 (classic) and 9×9 (long). The biggest has 10×10 dots,
// 90 horizontal and 90 vertical lines.
const int MAX_SIDE = 9;
const int MAX_EDGES = 2 * MAX_SIDE * (MAX_SIDE + 1);
const int MAX_BOXES = MAX_SIDE * MAX_SIDE;

enum { WAITING = 0, PLAYING = 1, OVER = 2 };
static const char *PHASE_NAME[] = {"waiting", "playing", "over"};

// No I and no O: on a phone screen they read as 1 and 0.
static const char CODE_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";

struct Room {
  bool used;
  char code[5];
  uint8_t side;                    // boxes per row and per column: 4, 6 or 9
  uint8_t phase;
  uint32_t tok[SEATS];             // 0 = empty seat
  char name[SEATS][NAME_LEN + 1];
  uint32_t seen[SEATS];
  int8_t edge[MAX_EDGES];          // -1 undrawn, else the seat that drew it
  int8_t box[MAX_BOXES];           // -1 unclaimed, else the seat that closed it
  uint8_t score[SEATS];            // boxes this game
  uint16_t wins[SEATS];            // games won while sitting here
  int8_t turn;                     // seat to move
  int8_t first;                    // seat that opened this game; moves on every rematch
  int8_t last;                     // the last line drawn, -1 if none
  uint8_t closed;                  // boxes the last line closed: 0, 1 or 2
  uint16_t moves;
  uint32_t turnAt;                 // when the current turn began
  int8_t timedOut;                 // seat whose clock ran out on the last move, -1 if none
  bool abandoned;                  // the game ended because everyone else left
  uint32_t ver;
};

Room rooms[MAX_ROOMS];

WebServer web(80);
DNSServer dns;

// ── the board's geometry ─────────────────────────────────────────────────────
//
// For a side of n boxes there are (n+1) rows of n horizontal lines, numbered first, then n
// rows of (n+1) vertical lines. Box (r, c) is bounded by:
//
//        top    = h(r,   c)
//        bottom = h(r+1, c)
//        left   = v(r,   c)
//        right  = v(r,   c+1)

static int hCount(int n) { return (n + 1) * n; }
static int edges(int n) { return 2 * n * (n + 1); }
static int hEdge(int n, int r, int c) { return r * n + c; }
static int vEdge(int n, int r, int c) { return hCount(n) + r * (n + 1) + c; }

// The one or two boxes a line borders. Returns how many.
static int boxesBeside(int n, int e, int out[2]) {
  int k = 0;
  if (e < hCount(n)) {
    const int r = e / n, c = e % n;
    if (r > 0) out[k++] = (r - 1) * n + c;   // the box above
    if (r < n) out[k++] = r * n + c;         // the box below
  } else {
    const int i = e - hCount(n), r = i / (n + 1), c = i % (n + 1);
    if (c > 0) out[k++] = r * n + c - 1;     // the box to the left
    if (c < n) out[k++] = r * n + c;         // the box to the right
  }
  return k;
}

static bool complete(const Room &rm, int b) {
  const int n = rm.side, r = b / n, c = b % n;
  return rm.edge[hEdge(n, r, c)] >= 0 && rm.edge[hEdge(n, r + 1, c)] >= 0 &&
         rm.edge[vEdge(n, r, c)] >= 0 && rm.edge[vEdge(n, r, c + 1)] >= 0;
}

// ── rooms and seats ──────────────────────────────────────────────────────────

static int seated(const Room &r) {
  int n = 0;
  for (int s = 0; s < SEATS; s++) if (r.tok[s]) n++;
  return n;
}

// The host is whoever holds the lowest seat — so if the creator leaves before the start, the
// next player in simply becomes the host.
static int hostOf(const Room &r) {
  for (int s = 0; s < SEATS; s++) if (r.tok[s]) return s;
  return -1;
}

// The next seated player after `s`, going round the table. -1 if nobody is seated.
static int nextSeat(const Room &r, int s) {
  for (int k = 1; k <= SEATS; k++) {
    const int t = (s + k) % SEATS;
    if (r.tok[t]) return t;
  }
  return -1;
}

static void clearBoard(Room &r) {
  for (int i = 0; i < MAX_EDGES; i++) r.edge[i] = -1;
  for (int i = 0; i < MAX_BOXES; i++) r.box[i] = -1;
  for (int s = 0; s < SEATS; s++) r.score[s] = 0;
  r.last = -1;
  r.closed = 0;
  r.moves = 0;
  r.timedOut = -1;
  r.abandoned = false;
  r.turnAt = millis();
}

static void begin(Room &r, int opener) {
  clearBoard(r);
  r.first = opener;
  r.turn = opener;
  r.phase = PLAYING;
  r.ver++;
}

// Ends the game: the most boxes wins, and a tie for the most is nobody's win.
static void finish(Room &r) {
  r.phase = OVER;
  int best = -1, top = -1, tied = 0;
  for (int s = 0; s < SEATS; s++) {
    if (!r.tok[s]) continue;
    if (r.score[s] > top) { top = r.score[s]; best = s; tied = 0; }
    else if (r.score[s] == top) tied++;
  }
  if (best >= 0 && !tied) r.wins[best]++;
  r.ver++;
}

// A seat empties when its phone leaves or goes quiet. Before the game, that is all. During
// it, the player's boxes stay theirs, their turns are skipped — and if only one player is
// left, the game is over and they have won it.
static void vacate(Room &r, int s) {
  r.tok[s] = 0;
  r.name[s][0] = 0;
  r.wins[s] = 0;
  r.ver++;
  if (!seated(r)) { r.used = false; return; }
  if (r.phase != PLAYING) return;
  if (seated(r) < 2) {
    const int left = hostOf(r);
    r.phase = OVER;
    r.abandoned = true;
    r.wins[left]++;
    return;
  }
  if (r.turn == s) {
    r.turn = nextSeat(r, s);
    r.closed = 0;
    r.turnAt = millis();
  }
}

static Room *byCode(const char *c) {
  for (int i = 0; i < MAX_ROOMS; i++)
    if (rooms[i].used && !strcmp(rooms[i].code, c)) return &rooms[i];
  return NULL;
}

static uint32_t argToken() { return (uint32_t)strtoul(web.arg("t").c_str(), NULL, 16); }

static int seatOf(const Room &r, uint32_t t) {
  if (!t) return -1;
  for (int s = 0; s < SEATS; s++) if (r.tok[s] == t) return s;
  return -1;
}

static void argCode(char *out) {
  String c = web.arg("c");
  c.trim();
  c.toUpperCase();
  strncpy(out, c.c_str(), 4);
  out[4] = 0;
}

// Draws into a buffer of its own. Chapter 3's first version drew straight into a room already
// marked used, matched that room against itself, and looped forever.
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

// Returns NULL if the line was drawn, or a short reason if it was refused.
static const char *draw(Room &r, int s, int e) {
  if (r.phase != PLAYING) return "over";
  if (r.turn != s) return "turn";
  if (e < 0 || e >= edges(r.side)) return "edge";
  if (r.edge[e] >= 0) return "taken";

  r.edge[e] = s;
  r.last = e;
  r.moves++;
  r.closed = 0;

  // Only the one or two boxes beside this line can have just been finished. Each that now has
  // four sides, and was not already someone's, is the mover's.
  int b[2];
  const int k = boxesBeside(r.side, e, b);
  for (int i = 0; i < k; i++)
    if (r.box[b[i]] < 0 && complete(r, b[i])) {
      r.box[b[i]] = s;
      r.score[s]++;
      r.closed++;
    }

  r.timedOut = -1;
  r.turnAt = millis();
  if (r.moves == edges(r.side)) { finish(r); return NULL; }
  // The rule that makes the game: close a box and you go again. Otherwise the turn passes.
  if (!r.closed) r.turn = nextSeat(r, s);
  r.ver++;
  return NULL;
}

// The clock ran out: the board draws a random free line for whoever was too slow. On a
// crowded board that usually hands the next player a box, which is the point.
static void autoMove(Room &r) {
  int free[MAX_EDGES], n = 0;
  for (int e = 0; e < edges(r.side); e++) if (r.edge[e] < 0) free[n++] = e;
  if (!n) return;
  const int s = r.turn;
  draw(r, s, free[esp_random() % n]);
  r.timedOut = s;
  Serial.printf("%7lu room %s: clock ran out, the board drew for %s\n",
                (unsigned long)millis(), r.code, r.name[s]);
}

static uint32_t timeLeft(const Room &r) {
  const uint32_t used = millis() - r.turnAt;
  return used >= TURN_MS ? 0 : TURN_MS - used;
}

// Releases seats whose phones went quiet, and runs the move clock. Called on every request and
// from loop(), so a clock runs out on time even when nobody is asking.
static void sweep() {
  const uint32_t now = millis();
  for (int i = 0; i < MAX_ROOMS; i++) {
    Room &r = rooms[i];
    if (!r.used) continue;
    for (int s = 0; s < SEATS; s++)
      if (r.tok[s] && now - r.seen[s] > SEAT_MS) vacate(r, s);
    if (r.used && r.phase == PLAYING && timeLeft(r) == 0) autoMove(r);
  }
}

// ── endpoints ────────────────────────────────────────────────────────────────

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

// The room as one seat sees it. Lines and boxes go as strings — one character each, '.' for
// nobody's or the seat number of whoever drew or closed it. On 9×9 that is 261 characters of
// board, and about 650 bytes in all.
static void sendState(const Room &r, int me) {
  String o;
  o.reserve(720);
  o += "{\"ok\":true,\"code\":\"";
  o += r.code;
  o += "\",\"side\":" + String(r.side);
  o += ",\"phase\":\"" + String(PHASE_NAME[r.phase]) + "\"";
  o += ",\"me\":" + String(me);
  o += ",\"host\":" + String(hostOf(r));
  o += ",\"names\":[";
  for (int s = 0; s < SEATS; s++) { if (s) o += ','; o += "\"" + uiJson(r.name[s]) + "\""; }
  o += "],\"here\":[";
  for (int s = 0; s < SEATS; s++) { if (s) o += ','; o += r.tok[s] ? '1' : '0'; }
  o += "],\"score\":[";
  for (int s = 0; s < SEATS; s++) { if (s) o += ','; o += String(r.score[s]); }
  o += "],\"wins\":[";
  for (int s = 0; s < SEATS; s++) { if (s) o += ','; o += String(r.wins[s]); }
  o += "],\"e\":\"";
  for (int e = 0; e < edges(r.side); e++) o += r.edge[e] < 0 ? '.' : (char)('0' + r.edge[e]);
  o += "\",\"b\":\"";
  for (int b = 0; b < r.side * r.side; b++) o += r.box[b] < 0 ? '.' : (char)('0' + r.box[b]);
  o += "\",\"turn\":" + String(r.turn);
  o += ",\"last\":" + String(r.last);
  o += ",\"closed\":" + String(r.closed);
  o += ",\"moves\":" + String(r.moves);
  o += ",\"clock\":" + String(r.phase == PLAYING ? (long)timeLeft(r) : -1L);
  o += ",\"limit\":" + String((long)TURN_MS);
  o += ",\"auto\":" + String(r.timedOut);
  o += ",\"abandoned\":" + String(r.abandoned ? "true" : "false");
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
  r.wins[s] = 0;
  r.ver++;
}

// Finds the room and the caller's seat in it, and marks the seat as alive. NULL if either is
// missing — the answer has already been sent.
static Room *mine(int &s) {
  sweep();
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  s = r ? seatOf(*r, argToken()) : -1;
  if (s < 0) { fail("gone"); return NULL; }
  r->seen[s] = millis();
  return r;
}

// GET /rooms  -> rooms that can still be joined
static void handleRooms() {
  sweep();
  String o = "{\"rooms\":[";
  bool first = true;
  for (int i = 0; i < MAX_ROOMS; i++) {
    const Room &r = rooms[i];
    if (!r.used || r.phase == PLAYING || seated(r) >= SEATS) continue;
    if (!first) o += ',';
    first = false;
    o += "{\"c\":\"" + String(r.code) + "\",\"s\":" + String(r.side)
       + ",\"n\":" + String(seated(r)) + ",\"h\":\"" + uiJson(r.name[hostOf(r)]) + "\"}";
  }
  o += "],\"devices\":" + String(WiFi.softAPgetStationNum())
     + ",\"cap\":" + String(MAX_STATIONS) + "}";
  web.send(200, "application/json", o);
}

// POST /create  n=<name>&s=<4|6|9>   -> { ok, code, token, seat }
static void handleCreate() {
  sweep();
  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");

  Room *r = NULL;
  for (int i = 0; i < MAX_ROOMS; i++) if (!rooms[i].used) { r = &rooms[i]; break; }
  if (!r) return fail("full");

  char code[5];
  newCode(code);                   // before the slot is marked used — see newCode()
  memset(r, 0, sizeof(Room));
  memcpy(r->code, code, sizeof(code));
  r->used = true;
  const int side = web.arg("s").toInt();
  r->side = side == 4 || side == 9 ? side : 6;
  r->phase = WAITING;
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

  const int had = seatOf(*r, argToken());          // a reload: the same seat back
  if (had >= 0) { r->seen[had] = millis(); return sendSeat(*r, had); }

  if (r->phase == PLAYING) return fail("started");
  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");
  int s = -1;
  for (int i = 0; i < SEATS; i++) if (!r->tok[i]) { s = i; break; }
  if (s < 0) return fail("busy");
  seat(*r, s, name);
  sendSeat(*r, s);
}

// GET /state?c=<code>&t=<token>   -- also the heartbeat that keeps a seat held
static void handleState() {
  int s;
  Room *r = mine(s);
  if (r) sendState(*r, s);
}

// POST /start  c=<code>&t=<token>   -- the host, with at least two players seated
static void handleStart() {
  int s;
  Room *r = mine(s);
  if (!r) return;
  if (r->phase != WAITING) return fail("started");
  if (s != hostOf(*r)) return fail("host");
  if (seated(*r) < 2) return fail("alone");
  trace("start");
  begin(*r, s);
  sendState(*r, s);
}

// POST /move  c=<code>&t=<token>&e=<line>
static void handleMove() {
  int s;
  Room *r = mine(s);
  if (!r) return;
  const String e = web.arg("e");
  const char *err = draw(*r, s, e.length() ? e.toInt() : -1);
  if (err) return fail(err);
  trace(r->closed ? "closed a box" : "ok");
  sendState(*r, s);
}

// POST /again  c=<code>&t=<token>   -- once a game is over, anyone may start the next
static void handleAgain() {
  int s;
  Room *r = mine(s);
  if (!r) return;
  if (r->phase != OVER) return fail("playing");
  if (seated(*r) < 2) return fail("alone");
  trace("again");
  // The opener moves one seat round the table, so nobody always goes first.
  const int opener = nextSeat(*r, r->tok[r->first] ? r->first : hostOf(*r));
  begin(*r, opener);
  sendState(*r, s);
}

// POST /leave  c=<code>&t=<token>
static void handleLeave() {
  char code[5];
  argCode(code);
  Room *r = byCode(code);
  const int s = r ? seatOf(*r, argToken()) : -1;
  if (s >= 0) { trace(r->name[s]); vacate(*r, s); }
  web.send(200, "application/json", "{\"ok\":true}");
}

static const char BODY[] PROGMEM = R"HTML(
<style>
#lobby,#game{display:none}
body.lobby #lobby,body.game #game{display:block}
.jbig{font-size:24px;font-weight:800;letter-spacing:-.03em;margin:0 0 6px}
.err{color:var(--red);font-size:13px;min-height:18px;margin:8px 0 0}
.sizes{display:grid;grid-template-columns:repeat(3,1fr);gap:9px;margin:12px 0 14px}
.size{background:var(--card2);border:1px solid var(--line);border-radius:14px;padding:12px 6px;
  color:var(--ink);font-weight:700;text-align:center}
.size span{display:block;font-size:11.5px;color:var(--mute);font-weight:500;margin-top:2px}
.size.sel{border-color:rgba(255,184,108,.6);box-shadow:0 0 0 4px rgba(255,184,108,.10)}
.open{display:flex;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid var(--line)}
.open:last-child{border-bottom:0}
.open .code{font-weight:800;letter-spacing:.12em}
.open .who{flex:1;color:var(--mute);font-size:13px}
.open button{width:auto;padding:8px 16px}
#codein{text-transform:uppercase;letter-spacing:.3em;font-weight:800;text-align:center}

/* The game screen never scrolls: a fixed column the height of the screen, with the board
   taking whatever is left after the bars above and below it. */
body.game{height:100dvh;overflow:hidden;padding:max(8px,env(safe-area-inset-top)) 10px max(8px,env(safe-area-inset-bottom))}
body.game header{display:none}
body.game .wrap{height:100%}
#game{flex-direction:column;height:100%;gap:6px}
body.game #game{display:flex}
.top{display:flex;align-items:center;gap:8px;flex:none}
.top .ghost{margin-left:auto;width:auto;padding:6px 12px;font-size:12px;border-radius:11px}
.codebig{font-size:12px;color:var(--mute)}
.codebig b{color:var(--ink);font-size:15px;letter-spacing:.16em;margin-left:3px}
.top .pill{padding:4px 10px;font-size:11.5px}
.players{display:grid;gap:6px;flex:none}
.pl{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:6px 8px;
  display:flex;align-items:center;gap:6px;min-width:0;transition:box-shadow .2s}
.pl .sw{width:9px;height:9px;border-radius:3px;flex:none}
.pl .nm{flex:1;font-weight:700;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0}
.pl .sc{font-size:17px;font-weight:800;font-variant-numeric:tabular-nums;line-height:1}
.pl .wn{font-size:10px;color:var(--faint)}
.pl.turn{box-shadow:0 0 0 2px var(--pc)}
.pl.empty{opacity:.35}
.line2{display:flex;align-items:center;gap:10px;flex:none;min-height:22px}
#status{flex:1;font-weight:750;font-size:14.5px;margin:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#note{color:var(--amber);font-size:12px;margin:0;flex:none;text-align:center}
#note:empty{display:none}
.clock{display:none;align-items:center;gap:7px;flex:0 0 116px}
.clock.on{display:flex}
.clock .track{flex:1;height:6px;border-radius:999px;background:var(--card2);overflow:hidden}
.clock i{display:block;height:100%;border-radius:999px;background:linear-gradient(90deg,var(--mint),#5ec8f0)}
.clock span{font-weight:800;font-variant-numeric:tabular-nums;font-size:12px;min-width:26px;text-align:right}
.clock.low i{background:var(--red)} .clock.low span{color:var(--red)}
#stagewrap{flex:1;min-height:0;position:relative}
#board{position:absolute;inset:0;width:100%;height:100%;display:block;touch-action:manipulation;
  user-select:none;-webkit-user-select:none}
#board .ln{stroke-linecap:round;transition:stroke .2s}
#board .hit{stroke:transparent;cursor:pointer}
#board .bx{transition:fill .35s}
#board .lt{font-weight:900;text-anchor:middle;dominant-baseline:central;pointer-events:none}
/* The last line drawn: a halo in the mover's colour around a white core. It stays until the
   next move and does not depend on animation — phones with "reduce motion" switched on turn
   every animation off, and a blink that never happens marks nothing. A new move pulses twice
   to catch the eye, then holds still: a pulse that never stops is noise. */
#board .halo{opacity:.85;animation:halo .55s ease-in-out 2;pointer-events:none}
#board .core{pointer-events:none}
#board .won{fill:none;stroke:#fff;pointer-events:none}
@keyframes halo{50%{opacity:.3}}
.foot2{flex:none}
.foot2:empty{display:none}
#startb,#again{display:none}
.rule{text-align:center;color:var(--faint);font-size:12px;margin:0;flex:none}
</style>

<header><span class="dot"></span><div>
  <h1>Dots and boxes</h1><p class="sub" id="sub">two to four players, refereed by the board</p>
</div></header>

<div id="lobby">
  <div class="card">
    <p class="jbig">New game</p>
    <input id="nm" placeholder="your name" maxlength="12" autocomplete="off">
    <div class="sizes">
      <button class="size" data-s="4" type="button">4 × 4<span>quick</span></button>
      <button class="size sel" data-s="6" type="button">6 × 6<span>classic</span></button>
      <button class="size" data-s="9" type="button">9 × 9<span>long game</span></button>
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

<div id="game">
  <div class="top">
    <span class="codebig">room<b id="code">----</b></span>
    <span class="pill" id="sizepill">6 × 6</span>
    <button class="ghost" onclick="leave()">leave</button>
  </div>
  <div class="players" id="players"></div>
  <div class="line2">
    <p id="status">—</p>
    <div class="clock" id="clock"><div class="track"><i id="ck"></i></div><span id="cks"></span></div>
  </div>
  <p id="note"></p>
  <div id="stagewrap"><svg id="board" preserveAspectRatio="xMidYMid meet"></svg></div>
  <div class="foot2">
    <button id="startb" onclick="start()">start the game</button>
    <button id="again" class="mint" onclick="again()">play again</button>
  </div>
  <p class="rule" id="rule">Draw a line between two dots. Close a box and it is yours — and you go again.</p>
</div>

<script>
const $=i=>document.getElementById(i);
function call(u,o){
  const a=new AbortController(),k=setTimeout(()=>a.abort(),5000);
  return fetch(u,Object.assign({signal:a.signal,cache:'no-store'},o||{}))
    .then(r=>r.json()).finally(()=>clearTimeout(k));
}
const get=u=>call(u);
const post=(u,b)=>call(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});
const enc=encodeURIComponent;
const NOREPLY='The board did not answer. Turn off mobile data and stay on the devkit-boxes WiFi.';
const COL=['#7ee7c7','#ff8fab','#ffb86c','#a78bfa'];
let code='',token='',size=6,timer=null,last=null,busy=false,built=0;
const NS='http://www.w3.org/2000/svg';

document.querySelectorAll('.size').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.size').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');size=+b.dataset.s});

function store(){try{localStorage.setItem('boxes',JSON.stringify({code,token}))}catch(e){}}
function forget(){try{localStorage.removeItem('boxes')}catch(e){}}
function myName(){const n=$('nm').value.trim();try{localStorage.setItem('boxes_n',n)}catch(e){};return n}
const ERRS={name:'Pick a name first.',full:'Every room on the board is in use.',
  nocode:'No room with that code. Check the letters?',busy:'That room already has four players.',
  started:'That game has already started — wait for the next one.',gone:'That game has ended.'};
function show(v){document.body.className=v}

function toLobby(msg){
  clearTimeout(timer);code='';token='';last=null;built=0;forget();
  show('lobby');$('jerr').textContent=msg||'';history.replaceState(null,'','/');lobby();
}
function enter(d){code=d.code;token=d.token;store();last=null;history.replaceState(null,'','/?c='+code);show('game');poll()}

$('mk').onclick=async()=>{
  const n=myName();if(!n){$('mkerr').textContent=ERRS.name;return}
  $('mk').disabled=true;$('mkerr').textContent='';
  try{const d=await post('/create','n='+enc(n)+'&s='+size);d.ok?enter(d):$('mkerr').textContent=ERRS[d.err]||'Could not create a room.'}
  catch(e){$('mkerr').textContent=NOREPLY}finally{$('mk').disabled=false}
};
async function join(c){
  const n=myName();if(!n){$('jerr').textContent=ERRS.name;return}
  $('jerr').textContent='';
  try{const d=await post('/join','c='+enc(c)+'&n='+enc(n));d.ok?enter(d):$('jerr').textContent=ERRS[d.err]||'Could not join.'}
  catch(e){$('jerr').textContent=NOREPLY}
}
$('jb').onclick=()=>{const c=$('codein').value.trim().toUpperCase();if(c.length===4)join(c)};

async function lobby(){
  if(document.body.className!=='lobby')return;
  try{
    const d=await get('/rooms');const box=$('opens');box.innerHTML='';
    if(d.rooms.length){const l=document.createElement('p');l.className='label';l.style.marginTop='16px';
      l.textContent='rooms you can join';box.appendChild(l)}
    for(const r of d.rooms){
      const row=document.createElement('div');row.className='open';
      const c=document.createElement('span');c.className='code';c.textContent=r.c;
      const w=document.createElement('span');w.className='who';w.textContent=r.h+' · '+r.s+'×'+r.s+' · '+r.n+'/4';
      const b=document.createElement('button');b.className='mint';b.textContent='join';b.onclick=()=>join(r.c);
      row.appendChild(c);row.appendChild(w);row.appendChild(b);box.appendChild(row);
    }
    $('sub').textContent=d.devices+' of '+d.cap+' phones connected';
  }catch(e){$('sub').textContent='cannot reach the board — mobile data off?'}
  setTimeout(lobby,2500);
}

// ── the board picture ────────────────────────────────────────────────────────
// Dots 100 units apart. Lines are numbered exactly as the board numbers them: every
// horizontal line row by row, then every vertical line row by row.
const G=100,M=24;
let lines=[],hits=[],boxes=[],letters=[],mark=null,marked='';
function el(t,a){const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e}
function edgeEnds(n,e){
  const H=(n+1)*n;
  if(e<H){const r=Math.floor(e/n),c=e%n;return [c*G+M,r*G+M,(c+1)*G+M,r*G+M]}
  const i=e-H,r=Math.floor(i/(n+1)),c=i%(n+1);return [c*G+M,r*G+M,c*G+M,(r+1)*G+M];
}
// Line, dot and tap-target sizes are in board units, so they shrink with the picture. On a 9×9
// board a box is about 36 px wide on a phone; these keep the lines visible and the targets
// thumb-sized at every size.
let LW=8,DR=9,HW=40,FS=36;
function build(n){
  const svg=$('board');svg.innerHTML='';const W=n*G+2*M;
  svg.setAttribute('viewBox','0 0 '+W+' '+W);
  LW=n>=9?13:n>=6?10:8;DR=n>=9?14:n>=6?11:9;HW=n>=9?70:n>=6?56:44;FS=n>=9?46:40;
  lines=[];hits=[];boxes=[];letters=[];marked='';
  for(let b=0;b<n*n;b++){const r=Math.floor(b/n),c=b%n;
    const x=el('rect',{x:c*G+M+6,y:r*G+M+6,width:G-12,height:G-12,rx:10,class:'bx',fill:'transparent'});
    svg.appendChild(x);boxes.push(x);
    const t=el('text',{x:c*G+M+G/2,y:r*G+M+G/2,class:'lt',fill:'#0e1016','font-size':FS});svg.appendChild(t);letters.push(t)}
  const E=2*n*(n+1);
  for(let e=0;e<E;e++){const [x1,y1,x2,y2]=edgeEnds(n,e);
    const l=el('line',{x1,y1,x2,y2,class:'ln','stroke-width':LW});svg.appendChild(l);lines.push(l)}
  mark=el('g',{});svg.appendChild(mark);
  for(let e=0;e<E;e++){const [x1,y1,x2,y2]=edgeEnds(n,e),hz=y1===y2,s=20;
    const h=el('line',{x1:hz?x1+s:x1,y1:hz?y1:y1+s,x2:hz?x2-s:x2,y2:hz?y2:y2-s,class:'hit','stroke-width':HW});
    h.addEventListener('click',()=>move(e));svg.appendChild(h);hits.push(h)}
  for(let r=0;r<=n;r++)for(let c=0;c<=n;c++)svg.appendChild(el('circle',{cx:c*G+M,cy:r*G+M,r:DR,fill:'#eef1f7'}));
  built=n;
}

function draw(d){
  const prev=last;last=d;
  if(built!==d.side)build(d.side);
  $('code').textContent=d.code;$('sizepill').textContent=d.side+' × '+d.side;
  const playing=d.phase==='playing',mine=playing&&d.turn===d.me;

  const P=$('players');P.innerHTML='';
  const seats=[0,1,2,3].filter(s=>d.phase==='waiting'||d.here[s]||d.score[s]);
  // Three or four chips on a phone would cut every name to a letter; two to a row keeps them
  // readable, and a phone has height to spare — its board is limited by the screen's width.
  const cols=seats.length>2&&innerWidth<560?2:seats.length;
  P.style.gridTemplateColumns='repeat('+cols+',minmax(0,1fr))';
  for(const s of seats){
    const p=document.createElement('div');p.className='pl'+(d.here[s]?'':' empty')+(playing&&d.turn===s&&d.here[s]?' turn':'');
    p.style.setProperty('--pc',COL[s]);
    const sw=document.createElement('span');sw.className='sw';sw.style.background=COL[s];
    const nm=document.createElement('span');nm.className='nm';
    nm.textContent=d.here[s]?d.names[s]+(s===d.me?' (you)':''):'open seat';
    const sc=document.createElement('span');sc.className='sc';sc.textContent=d.here[s]?d.score[s]:'';
    const wn=document.createElement('span');wn.className='wn';wn.textContent=d.here[s]&&d.wins[s]?'★'+d.wins[s]:'';
    p.appendChild(sw);p.appendChild(nm);p.appendChild(sc);p.appendChild(wn);P.appendChild(p);
  }

  for(let e=0;e<lines.length;e++){
    const o=d.e[e],l=lines[e];
    l.setAttribute('stroke',o==='.'?'#262c3b':COL[+o]);
    l.setAttribute('stroke-dasharray',o==='.'?'1 '+(LW*2.4).toFixed(0):'');
    hits[e].style.display=mine&&o==='.'?'':'none';
  }
  for(let b=0;b<boxes.length;b++){
    const o=d.b[b];
    boxes[b].setAttribute('fill',o==='.'?'transparent':COL[+o]);
    boxes[b].setAttribute('fill-opacity',o==='.'?0:.85);
    letters[b].textContent=o==='.'?'':(d.names[+o]||'?').charAt(0).toUpperCase();
  }

  markLast(d);
  const nm=s=>s===d.me?'you':(d.names[s]||'someone');
  let st='',note='';
  if(d.phase==='waiting'){
    const n=d.here.filter(Boolean).length;
    st=d.me===d.host?(n<2?'waiting for players — share code '+d.code:n+' players in — start when ready')
                    :'waiting for '+d.names[d.host]+' to start';
  }else if(playing){
    if(mine)st=d.closed&&d.last>=0&&d.e[d.last]===String(d.me)?'You closed '+(d.closed>1?'two boxes':'a box')+' — go again!':'Your turn';
    else st=(d.closed?d.names[d.turn]+' closed '+(d.closed>1?'two boxes':'a box')+' and goes again':d.names[d.turn]+"'s turn");
    if(d.auto>=0)note='Time ran out — the board drew a line for '+nm(d.auto)+'.';
  }else{
    if(d.abandoned)st='Everyone else left — '+nm(d.host)+(d.host===d.me?' win':' wins');
    else{
      let top=-1,who=[];for(let s=0;s<4;s++){if(!d.here[s])continue;if(d.score[s]>top){top=d.score[s];who=[s]}else if(d.score[s]===top)who.push(s)}
      st=who.length>1?'A draw at '+top+' boxes each':(who[0]===d.me?'You win':d.names[who[0]]+' wins')+' with '+top+' boxes';
    }
  }
  $('status').textContent=st;$('note').textContent=note;
  $('rule').style.display=d.phase==='waiting'?'block':'none';
  $('startb').style.display=d.phase==='waiting'&&d.me===d.host?'block':'none';
  $('startb').disabled=d.here.filter(Boolean).length<2;
  $('again').style.display=d.phase==='over'?'block':'none';
  if(d.clock>=0){deadline=Date.now()+d.clock;limit=d.limit;$('clock').classList.add('on')}
  else{deadline=0;$('clock').classList.remove('on')}
}

// The boxes on either side of a line — the same rule as boxesBeside() on the board.
function beside(n,e){
  const H=(n+1)*n,out=[];
  if(e<H){const r=Math.floor(e/n),c=e%n;if(r>0)out.push((r-1)*n+c);if(r<n)out.push(r*n+c)}
  else{const i=e-H,r=Math.floor(i/(n+1)),c=i%(n+1);if(c>0)out.push(r*n+c-1);if(c<n)out.push(r*n+c)}
  return out;
}
// Redrawn only when the last move changes, so the pulse is not restarted on every poll; built
// fresh each time, so a new move always starts its own pulse.
function markLast(d){
  const key=d.last+':'+d.moves+':'+d.side;
  if(key===marked)return;marked=key;mark.innerHTML='';
  if(d.last<0||d.e[d.last]==='.')return;
  const [x1,y1,x2,y2]=edgeEnds(d.side,d.last),col=COL[+d.e[d.last]];
  mark.appendChild(el('line',{x1,y1,x2,y2,class:'halo',stroke:col,'stroke-width':LW*3.6,'stroke-linecap':'round'}));
  mark.appendChild(el('line',{x1,y1,x2,y2,class:'core',stroke:'#fff','stroke-width':Math.max(5,LW*.75),'stroke-linecap':'round'}));
  // and the boxes that line just won
  if(d.closed)for(const b of beside(d.side,d.last)){
    if(d.b[b]!==d.e[d.last])continue;
    const r=Math.floor(b/d.side),c=b%d.side;
    mark.appendChild(el('rect',{x:c*G+M+6,y:r*G+M+6,width:G-12,height:G-12,rx:10,class:'won','stroke-width':LW*.7}));
  }
}

let deadline=0,limit=30000;
setInterval(()=>{if(!deadline)return;const left=Math.max(0,deadline-Date.now());
  $('ck').style.width=(left/limit*100)+'%';$('cks').textContent=Math.ceil(left/1000)+'s';
  $('clock').classList.toggle('low',left<8000)},100);

async function poll(){
  clearTimeout(timer);if(!code)return;
  try{const d=await get('/state?c='+enc(code)+'&t='+enc(token));
    if(!d.ok){toLobby('The game ended — the board restarted, or you timed out.');return}
    if(!busy)draw(d);
  }catch(e){$('sub').textContent='lost the board — retrying'}
  timer=setTimeout(poll,700);
}
async function move(e){
  if(busy)return;busy=true;hits.forEach(h=>h.style.display='none');
  try{const d=await post('/move','c='+enc(code)+'&t='+enc(token)+'&e='+e);if(d.ok)draw(d);
    else if(d.err==='gone'){busy=false;toLobby(ERRS.gone);return}}catch(x){}
  busy=false;poll();
}
async function start(){try{const d=await post('/start','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function again(){try{const d=await post('/again','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function leave(){try{await post('/leave','c='+enc(code)+'&t='+enc(token))}catch(e){};toLobby('')}

(async function boot(){
  try{$('nm').value=localStorage.getItem('boxes_n')||''}catch(e){}
  let saved=null;try{saved=JSON.parse(localStorage.getItem('boxes')||'null')}catch(e){}
  if(saved&&saved.code&&saved.token){
    try{const d=await post('/join','c='+enc(saved.code)+'&t='+enc(saved.token));if(d.ok){enter(d);return}}catch(e){}
    forget();
  }
  const q=new URLSearchParams(location.search).get('c');if(q)$('codein').value=q.toUpperCase();
  show('lobby');lobby();
})();
</script>
)HTML";

static void handlePage() {
  trace("page");
  uiSend(web, "Dots and boxes", BODY);
}

// Anything not asked of 192.168.4.1/ is redirected there — chapter 3's fix. Serving the page
// in answer to a phone's connectivity probe makes it live at the probe's hostname, and its
// requests then leave over the phone's own DNS and never reach the board.
static void handleElse() {
  const String home = WiFi.softAPIP().toString();
  if (web.hostHeader() == home && web.uri() == "/") return handlePage();
  web.sendHeader("Location", String("http://") + home + "/", true);
  web.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  delay(300);

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
  web.on("/start", HTTP_POST, handleStart);
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
