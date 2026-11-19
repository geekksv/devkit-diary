#pragma once
//
// The rules of Ludo, and nothing else — no radio, no web, no rooms. So the whole game can be
// played out thousands of times on a laptop and checked (ludo_test), which is the only way to
// trust a rule engine this fiddly.
//
// The board is the referee: a phone only ever says "roll" or "move token 2". Every rule below
// is enforced here, so a crafted request cannot move further than a die allows, leave base
// without a six, or capture across a safe square.
//
// How a token's journey is counted. Each token has a `prog` (progress) from 0 to 57:
//   0            in its base, not yet out
//   1 .. 51      on the shared 52-square ring; the actual ring square is
//                (START[seat] + prog - 1) mod 52
//   52 .. 56     the five squares of its own home column (private, no one else can be there)
//   57           home — the centre; the token is finished
// A six is needed to come out (base -> prog 1). The exact number is needed to land on 57; an
// overshoot is not allowed.

#include <stdint.h>
#include <string.h>

enum { LUDO_SEATS = 4, LUDO_TOKENS = 4, LUDO_RING = 52, LUDO_FINISH = 57 };

// Where each seat enters the ring. The four entries sit a quarter-turn apart.
static const int LUDO_START[LUDO_SEATS] = {0, 13, 26, 39};

// The eight protected squares: every seat's entry, and a star eight squares along from each.
static inline bool ludoSafe(int cell) {
  switch (cell) {
    case 0: case 13: case 26: case 39:          // the entry squares
    case 8: case 21: case 34: case 47:          // the stars
      return true;
    default:
      return false;
  }
}

// The ring square a token sits on, or -1 if it is in base, its home column, or finished.
static inline int ludoRingCell(int seat, int prog) {
  if (prog < 1 || prog > 51) return -1;
  return (LUDO_START[seat] + prog - 1) % LUDO_RING;
}

enum { LUDO_WAIT = 0, LUDO_ROLL = 1, LUDO_MOVE = 2, LUDO_OVER = 3 };

struct LudoState {
  uint8_t tokensPer;              // 4 in the classic game, 2 in quick mode
  bool active[LUDO_SEATS];        // which seats are in this game
  int8_t prog[LUDO_SEATS][LUDO_TOKENS];
  int8_t turn;                    // whose turn
  int8_t die;                     // the roll waiting to be used, 0 if none
  uint8_t phase;                  // WAIT / ROLL / MOVE / OVER
  uint8_t sixStreak;              // consecutive sixes this turn (three forfeits it)
  int8_t winner;                  // seat, or -1
  // the last move, for the page to animate and narrate
  int8_t lastSeat, lastToken, lastFrom, lastTo, lastCaps;
};

static inline int ludoSeated(const LudoState &g) {
  int n = 0;
  for (int s = 0; s < LUDO_SEATS; s++) if (g.active[s]) n++;
  return n;
}

static inline int ludoNextSeat(const LudoState &g, int s) {
  for (int k = 1; k <= LUDO_SEATS; k++) {
    const int t = (s + k) % LUDO_SEATS;
    if (g.active[t]) return t;
  }
  return s;
}

static inline bool ludoDone(const LudoState &g, int seat) {
  for (int t = 0; t < g.tokensPer; t++) if (g.prog[seat][t] != LUDO_FINISH) return false;
  return true;
}

// How many of `seat`'s tokens sit on ring square `cell` — a pair or more is a protected
// blockade, which cannot be captured.
static inline int ludoCountOn(const LudoState &g, int seat, int cell) {
  int n = 0;
  for (int t = 0; t < g.tokensPer; t++) if (ludoRingCell(seat, g.prog[seat][t]) == cell) n++;
  return n;
}

// The tokens of `seat` that this die could legally move. Returns how many, and fills `out`.
static inline int ludoLegal(const LudoState &g, int seat, int die, int out[LUDO_TOKENS]) {
  int n = 0;
  if (die < 1 || die > 6) return 0;
  for (int t = 0; t < g.tokensPer; t++) {
    const int p = g.prog[seat][t];
    if (p == LUDO_FINISH) continue;
    if (p == 0) {                                // in base: only a six brings it out
      if (die == 6) out[n++] = t;
      continue;
    }
    const int np = p + die;
    if (np > LUDO_FINISH) continue;              // must land exactly on home
    const int cell = ludoRingCell(seat, np);
    if (cell >= 0 && !ludoSafe(cell)) {          // cannot land on an enemy blockade
      bool blocked = false;
      for (int o = 0; o < LUDO_SEATS && !blocked; o++)
        if (o != seat && g.active[o] && ludoCountOn(g, o, cell) >= 2) blocked = true;
      if (blocked) continue;
    }
    out[n++] = t;
  }
  return n;
}

static inline void ludoBegin(LudoState &g, uint8_t tokensPer, const bool active[LUDO_SEATS], int first) {
  memset(&g, 0, sizeof(g));
  g.tokensPer = tokensPer;
  for (int s = 0; s < LUDO_SEATS; s++) g.active[s] = active[s];
  g.turn = active[first] ? first : ludoNextSeat(g, first);
  g.die = 0;
  g.phase = LUDO_ROLL;
  g.winner = -1;
  g.lastSeat = g.lastToken = g.lastFrom = g.lastTo = -1;
  g.lastCaps = 0;
}

// The seat whose turn it is gives up the die; move to the next seat and await a roll.
static inline void ludoPass(LudoState &g) {
  g.sixStreak = 0;
  g.die = 0;
  g.turn = ludoNextSeat(g, g.turn);
  g.phase = LUDO_ROLL;
}

// A die has been rolled for the seat to move. Decides what happens next: a third six forfeits
// the turn; no legal move passes it; otherwise the seat must now choose a token.
// Returns true if the roll left a move to make (phase == MOVE).
static inline bool ludoRoll(LudoState &g, int die) {
  if (g.phase != LUDO_ROLL) return false;
  g.die = die;
  if (die == 6) {
    if (g.sixStreak == 2) { ludoPass(g); return false; }   // the third six: whole turn void
    g.sixStreak++;
  }
  int out[LUDO_TOKENS];
  const int n = ludoLegal(g, g.turn, die, out);
  if (n == 0) {                                   // nothing to move: the die is wasted
    if (die == 6) g.die = 0;                       // (a six with no move still ends the turn)
    ludoPass(g);
    return false;
  }
  g.phase = LUDO_MOVE;
  return true;
}

struct LudoMove { bool ok; int captured; bool finished; bool bonus; bool win; };

// Move one of the seat's tokens by the pending die. Resolves captures, a finished token, the
// bonus roll (a six, a capture, or a finish earns another turn), and the win.
static inline LudoMove ludoMove(LudoState &g, int seat, int token) {
  LudoMove r = {false, 0, false, false, false};
  if (g.phase != LUDO_MOVE || seat != g.turn) return r;
  int out[LUDO_TOKENS];
  const int n = ludoLegal(g, seat, g.die, out);
  bool legal = false;
  for (int i = 0; i < n; i++) if (out[i] == token) legal = true;
  if (!legal) return r;

  const int from = g.prog[seat][token];
  const int np = from == 0 ? 1 : from + g.die;
  g.prog[seat][token] = np;

  // capture: on a non-safe ring square, any lone enemy token there goes home to base
  const int cell = ludoRingCell(seat, np);
  if (cell >= 0 && !ludoSafe(cell)) {
    for (int o = 0; o < LUDO_SEATS; o++) {
      if (o == seat || !g.active[o]) continue;
      if (ludoCountOn(g, o, cell) != 1) continue;            // a blockade is protected
      for (int t = 0; t < g.tokensPer; t++)
        if (ludoRingCell(o, g.prog[o][t]) == cell) { g.prog[o][t] = 0; r.captured++; }
    }
  }
  r.finished = (np == LUDO_FINISH);

  g.lastSeat = seat; g.lastToken = token; g.lastFrom = from; g.lastTo = np; g.lastCaps = r.captured;

  if (ludoDone(g, seat)) { g.phase = LUDO_OVER; g.winner = seat; r.ok = true; r.win = true; return r; }

  r.bonus = (g.die == 6) || (r.captured > 0) || r.finished;
  if (r.bonus) { g.die = 0; g.phase = LUDO_ROLL; }           // same seat rolls again
  else ludoPass(g);
  r.ok = true;
  return r;
}

// The token a timed-out or careless player is moved on: prefer finishing, then capturing, then
// the furthest-along token; a deterministic, reasonable choice.
static inline int ludoAutoPick(const LudoState &g, int seat, int die) {
  int out[LUDO_TOKENS];
  const int n = ludoLegal(g, seat, die, out);
  if (n == 0) return -1;
  int best = out[0], bestScore = -1;
  for (int i = 0; i < n; i++) {
    const int t = out[i], p = g.prog[seat][t];
    const int np = p == 0 ? 1 : p + die;
    int score = np;                                 // furthest wins ties
    if (np == LUDO_FINISH) score += 1000;           // finishing is best
    const int cell = ludoRingCell(seat, np);
    if (cell >= 0 && !ludoSafe(cell))               // then a capture
      for (int o = 0; o < LUDO_SEATS; o++)
        if (o != seat && g.active[o] && ludoCountOn(g, o, cell) == 1) score += 500;
    if (score > bestScore) { bestScore = score; best = t; }
  }
  return best;
}
