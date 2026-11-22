// The rules, on a laptop:  c++ -std=c++17 -I../src/tambola tambola_test.cpp -o t && ./t
#include "tambola.h"
#include <random>
#include <string>
#include <map>

static std::mt19937 rng(12345);
static uint32_t hostRand() { return rng(); }
static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

struct Args {                         // a request's arguments, as the board's web server would give them
  std::map<std::string, std::string> m;
  const char *operator()(const char *k) const { auto i = m.find(k); return i == m.end() ? "" : i->second.c_str(); }
};
static char buf[16384];
static std::string call(const char *path, Args a, uint32_t now) {
  Out o = {buf, sizeof buf, 0};
  route(path, a, now, o);
  return std::string(buf, o.n);
}
static std::string field(const std::string &j, const char *key) {   // the raw value after "key":
  const std::string k = std::string("\"") + key + "\":";
  const size_t p = j.find(k);
  if (p == std::string::npos) return "";
  size_t e = p + k.size();
  if (j[e] == '"') return j.substr(e + 1, j.find('"', e + 1) - e - 1);
  size_t f = e;
  while (f < j.size() && j[f] != ',' && j[f] != '}' && j[f] != ']') f++;
  return j.substr(e, f - e);
}

int main() {
  tRand = hostRand;

  // ── tickets: thousands of strips, every rule, every number once per strip
  int stripsOk = 0, ticketsOk = 0;
  const int STRIPS = 5000;
  std::map<int, int> colCounts;
  for (int s = 0; s < STRIPS; s++) {
    Ticket t[6];
    if (!makeStrip(t)) continue;
    stripsOk++;
    int seen[91] = {};
    bool every = true;
    for (int k = 0; k < 6; k++) {
      ticketsOk += ticketOk(t[k]);
      for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) if (t[k].n[r][c]) seen[t[k].n[r][c]]++;
      for (int c = 0; c < 9; c++) { int n = 0; for (int r = 0; r < 3; r++) n += t[k].n[r][c] != 0; colCounts[n]++; }
    }
    for (int v = 1; v <= 90; v++) every &= seen[v] == 1;
    if (!every) { CHECK(!"each number once per strip"); break; }
  }
  CHECK(stripsOk == STRIPS);
  CHECK(ticketsOk == STRIPS * 6);
  CHECK(colCounts[1] > 0 && colCounts[2] > 0 && colCounts[3] > 0 && colCounts[0] == 0);   // varied, never an empty column
  printf("  %d strips, %d tickets: all valid; columns holding 1/2/3 numbers: %d/%d/%d\n", stripsOk, ticketsOk,
         colCounts[1], colCounts[2], colCounts[3]);
  { Ticket t; memset(&t, 0, sizeof t); CHECK(!ticketOk(t)); }
  { Ticket s[6]; makeStrip(s); Ticket b = s[0]; std::swap(b.n[0][0], b.n[0][1]); CHECK(!ticketOk(b) || b.n[0][0] == b.n[0][1]); }

  // ── the draw: all 90, each once, in a different order each game
  Draw d; d.init();
  int got[91] = {}, n, k = 0;
  while ((n = d.next())) { got[n]++; k++; CHECK(d.at[n] == k); }
  bool once = k == 90; for (int v = 1; v <= 90; v++) once &= got[v] == 1;
  CHECK(once && d.count == 90 && d.next() == 0);
  Draw d2; d2.init(); CHECK(memcmp(d.seq, d2.seq, 90) != 0);

  // ── prize patterns on a known ticket
  Ticket t = {{{4, 0, 23, 0, 41, 0, 62, 0, 85}, {0, 12, 25, 0, 44, 58, 0, 77, 0}, {9, 0, 0, 36, 0, 59, 68, 0, 90}}};
  CHECK(ticketOk(t));
  auto callOnly = [&](std::initializer_list<int> nums) { Draw x; x.count = 0; memset(x.at, 0, sizeof x.at); for (int v : nums) { x.seq[x.count] = v; x.at[v] = ++x.count; } return x; };
  Draw x = callOnly({4, 23, 41, 62, 85});
  CHECK(qualifies(t, P_TOP, x, x.count) && qualifies(t, P_EARLY, x, x.count) && !qualifies(t, P_MIDDLE, x, x.count));
  CHECK(!qualifies(t, P_TOP, x, 4));                                 // the fifth wasn't called yet at that point
  x = callOnly({4, 85, 9, 90});
  CHECK(qualifies(t, P_CORNERS, x, x.count) && !qualifies(t, P_EARLY, x, x.count));
  x = callOnly({4, 85, 9});
  CHECK(!qualifies(t, P_CORNERS, x, x.count));
  x = callOnly({12, 25, 44, 58, 77, 1, 2});
  CHECK(qualifies(t, P_MIDDLE, x, x.count) && qualifies(t, P_EARLY, x, x.count) && !qualifies(t, P_BOTTOM, x, x.count));
  x = callOnly({4, 23, 41, 62, 85, 12, 25, 44, 58, 77, 9, 36, 59, 68});
  CHECK(!qualifies(t, P_FULL, x, x.count));
  x.seq[x.count] = 90; x.at[90] = ++x.count;
  CHECK(qualifies(t, P_FULL, x, x.count) && qualifies(t, P_BOTTOM, x, x.count));

  // ── a room, through the same requests the phones send
  uint32_t now = 1000;
  std::string j = call("/create", Args{{{"n", "Keshav"}, {"k", "2"}}}, now);
  const std::string code = field(j, "code"), hostTok = field(j, "tok");
  CHECK(code.size() == 4 && hostTok.size() == 8 && field(j, "me") == "0" && field(j, "host") == "0");
  Room &r = *byCode(code.c_str());
  CHECK(r.np == 1 && r.pl[0].ntix == 2 && ticketOk(r.tk[r.pl[0].tix[0]]) && ticketOk(r.tk[r.pl[0].tix[1]]));
  j = call("/join", Args{{{"c", code}, {"n", "  Dadi   ji  "}, {"k", "6"}}}, now);
  const std::string dadiTok = field(j, "tok");
  CHECK(!strcmp(r.pl[1].name, "Dadi ji") && r.pl[1].ntix == 6);
  {   // one player's tickets never share a number
    int seen[91] = {}; bool clash = false;
    for (int i = 0; i < 6; i++) for (int a = 0; a < 3; a++) for (int b = 0; b < 9; b++) { int v = r.tk[r.pl[1].tix[i]].n[a][b]; if (v && seen[v]++) clash = true; }
    CHECK(!clash);
  }
  j = call("/join", Args{{{"c", code}, {"n", "अनन्या बहुत लंबा नाम"}, {"k", "1"}}}, now);
  const std::string anuTok = field(j, "tok");
  CHECK(strlen(r.pl[2].name) <= (size_t)NAME_LEN && r.pl[2].name[0]);
  { const unsigned char *nm = (const unsigned char *)r.pl[2].name; size_t L = strlen(r.pl[2].name); CHECK((nm[L - 1] & 0xC0) != 0xC0); }   // not cut mid-letter
  j = call("/join", Args{{{"c", code}, {"n", "<b>\"x\"</b>"}}}, now);
  CHECK(!strchr(r.pl[3].name, '<') && !strchr(r.pl[3].name, '"') && r.pl[3].ntix == 1);
  CHECK(field(call("/join", Args{{{"c", "ZZZZ"}, {"n", "a"}}}, now), "err") == "nogame");
  CHECK(field(call("/join", Args{{{"c", code.substr(0, 4)}, {"t", dadiTok}}}, now), "me") == "1");     // rejoin = same seat
  CHECK(field(call("/state", Args{{{"c", code}}}, now), "me") == "-1");                                  // a watcher
  CHECK(field(call("/start", Args{{{"c", code}, {"t", dadiTok}}}, now), "err") == "nothost");
  CHECK(field(call("/tickets", Args{{{"c", code}, {"t", anuTok}, {"k", "3"}}}, now), "ok") == "true" && r.pl[2].ntix == 3);
  call("/settings", Args{{{"c", code}, {"t", hostTok}, {"pace", "5"}, {"strict", "1"}, {"lang", "2"}, {"prizes", "127"}}}, now);
  CHECK(r.pace == 5 && r.strict == 1 && r.lang == 2 && r.prizes == 127);
  call("/settings", Args{{{"c", code}, {"t", hostTok}, {"prizes", "1"}}}, now);
  CHECK(r.prizes == (1 | 1 << P_FULL));                                   // Full House can't be turned off
  call("/settings", Args{{{"c", code}, {"t", hostTok}, {"prizes", "127"}}}, now);

  // start: the first number after a countdown, then one every 5 s
  CHECK(field(call("/start", Args{{{"c", code}, {"t", hostTok}}}, now), "phase") == "1");
  CHECK(field(call("/join", Args{{{"c", code}, {"n", "late"}}}, now), "err") == "started");
  tick(now + FIRST_MS - 1); CHECK(r.d.count == 0);
  tick(now + FIRST_MS); CHECK(r.d.count == 1);
  now += FIRST_MS;
  tick(now + 4999); CHECK(r.d.count == 1);
  now += 5000; tick(now); CHECK(r.d.count == 2);

  // claims: call numbers until somebody has Early Five; strict, so a wrong claim is a Bogey
  auto anyHas = [&](int p, int *who) { for (int i = 0; i < r.np; i++) for (int k2 = 0; k2 < r.pl[i].ntix; k2++) if (qualifies(r.tk[r.pl[i].tix[k2]], p, r.d, r.d.count)) { *who = i; return true; } return false; };
  auto tokOf = [&](int i) { char b[9]; snprintf(b, sizeof b, "%08lx", (unsigned long)r.pl[i].tok); return std::string(b); };
  int w = -1;
  while (!anyHas(P_EARLY, &w)) { now += 5000; tick(now); }
  int loser = -1;
  for (int i = 0; i < r.np; i++) { bool h = false; for (int k2 = 0; k2 < r.pl[i].ntix; k2++) h |= qualifies(r.tk[r.pl[i].tix[k2]], P_TOP, r.d, r.d.count); if (!h) { loser = i; break; } }
  CHECK(loser >= 0);
  CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(loser)}, {"p", "top"}}}, now), "r") == "bogey");
  CHECK(r.pl[loser].bogey >> P_TOP & 1);
  CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(loser)}, {"p", "top"}}}, now), "r") == "out");   // out of that prize
  const uint8_t before = r.d.count;
  CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(w)}, {"p", "early"}}}, now), "r") == "won");
  CHECK(r.pz[P_EARLY].won && r.pz[P_EARLY].at == before && r.pz[P_EARLY].who[0] == w);
  CHECK(r.nextAt >= now + GRACE_MS);                                      // a pause, for anyone sharing it
  CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(w)}, {"p", "early"}}}, now), "r") == "taken");
  // someone else who also has it on this same number shares it
  int other = -1;
  for (int i = 0; i < r.np && other < 0; i++) if (i != w) for (int k2 = 0; k2 < r.pl[i].ntix; k2++) if (qualifies(r.tk[r.pl[i].tix[k2]], P_EARLY, r.d, r.d.count)) other = i;
  if (other >= 0) CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(other)}, {"p", "early"}}}, now), "r") == "shared");
  // after the next number, too late to share
  now += GRACE_MS; tick(now);
  CHECK(r.d.count == before + 1);
  for (int i = 0; i < r.np; i++) {
    bool listed = false; for (int q = 0; q < r.pz[P_EARLY].nw; q++) listed |= r.pz[P_EARLY].who[q] == i;
    if (!listed) { CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(i)}, {"p", "early"}}}, now), "r") == "taken"); break; }
  }
  CHECK(field(call("/claim", Args{{{"c", code}, {"t", tokOf(0)}, {"p", "full2"}}}, now), "r") == "notyet");

  // pause and resume; manual pace
  call("/pause", Args{{{"c", code}, {"t", hostTok}}}, now);
  const uint8_t held = r.d.count;
  tick(now + 60000); CHECK(r.phase == PAUSED && r.d.count == held);
  now += 60000;
  call("/pause", Args{{{"c", code}, {"t", hostTok}}}, now);
  CHECK(r.phase == CALLING);
  call("/settings", Args{{{"c", code}, {"t", hostTok}, {"pace", "0"}}}, now);
  tick(now + 100000); CHECK(r.d.count == held);
  call("/next", Args{{{"c", code}, {"t", hostTok}}}, now); CHECK(r.d.count == held + 1);
  call("/settings", Args{{{"c", code}, {"t", hostTok}, {"pace", "5"}}}, now);

  // play it out: everyone claims whatever they have, as soon as they have it
  int safety = 0;
  while (r.phase == CALLING && safety++ < 200) {
    for (int i = 0; i < r.np; i++)
      for (int p = 0; p < P_COUNT; p++) {
        bool h = false; for (int k2 = 0; k2 < r.pl[i].ntix; k2++) h |= qualifies(r.tk[r.pl[i].tix[k2]], p, r.d, r.d.count);
        if (h && !(r.pl[i].bogey >> p & 1)) call("/claim", Args{{{"c", code}, {"t", tokOf(i)}, {"p", PRIZE_KEYS[p]}}}, now);
      }
    now += 5000; tick(now);
  }
  CHECK(r.phase == OVER);
  for (int p = 0; p < P_COUNT; p++) CHECK(r.pz[p].won);
  CHECK(r.pz[P_FULL2].at >= r.pz[P_FULL].at);
  for (int q = 0; q < r.pz[P_FULL2].nw; q++) for (int z = 0; z < r.pz[P_FULL].nw; z++) CHECK(r.pz[P_FULL2].who[q] != r.pz[P_FULL].who[z]);
  int pts = 0; for (int i = 0; i < r.np; i++) pts += roundPoints(r, i);
  CHECK(pts >= 10 + 15 * 4 + 40 + 25);
  // a new round: points carried, new tickets, same people
  Ticket oldFirst = r.tk[r.pl[0].tix[0]];
  const int carried = roundPoints(r, 0);
  CHECK(field(call("/again", Args{{{"c", code}, {"t", hostTok}}}, now), "phase") == "0");
  CHECK(r.round == 2 && r.pl[0].total == carried && !r.pz[P_EARLY].won && memcmp(&oldFirst, &r.tk[r.pl[0].tix[0]], sizeof(Ticket)));

  // ── many whole games, friendly and strict, with random players: every one ends, prizes add up
  int games = 0, ended = 0, allPrizes = 0, sharedSeen = 0, bogeys = 0;
  for (int g = 0; g < 2000; g++) {
    memset(rooms, 0, sizeof rooms);
    uint32_t t0 = 5000;
    std::string h = call("/create", Args{{{"n", "Host"}, {"k", std::to_string(1 + g % 6)}}}, t0);
    std::string c = field(h, "code"), ht = field(h, "tok");
    Room &R = *byCode(c.c_str());
    const int players = 2 + g % 25;
    for (int i = 1; i < players; i++) call("/join", Args{{{"c", c}, {"n", "P" + std::to_string(i)}, {"k", std::to_string(1 + (g + i) % 6)}}}, t0);
    call("/settings", Args{{{"c", c}, {"t", ht}, {"strict", g % 2 ? "1" : "0"}, {"prizes", g % 3 ? "127" : "63"}}}, t0);
    call("/start", Args{{{"c", c}, {"t", ht}}}, t0);
    games++;
    int guard = 0, gameBogeys = 0;
    while (R.phase == CALLING && guard++ < 400) {
      for (int i = 0; i < R.np; i++) {
        char tk[9]; snprintf(tk, sizeof tk, "%08lx", (unsigned long)R.pl[i].tok);
        for (int p = 0; p < P_COUNT; p++) {
          bool hv = false; for (int k2 = 0; k2 < R.pl[i].ntix; k2++) hv |= qualifies(R.tk[R.pl[i].tix[k2]], p, R.d, R.d.count);
          if (hv || rnd(200) == 0) {                         // now and then, a mistaken claim
            const std::string res = field(call("/claim", Args{{{"c", c}, {"t", tk}, {"p", PRIZE_KEYS[p]}}}, t0), "r");
            if (res == "bogey") { bogeys++; gameBogeys++; }
            if (res == "bogey") CHECK(!hv);
            if (res == "notyet") CHECK(!hv || (p == P_FULL2 && !R.pz[P_FULL].won));   // Second waits for the first
          }
        }
      }
      t0 += 8000; tick(t0);
    }
    if (R.phase == OVER) ended++;
    bool all = true;
    for (int p = 0; p < P_COUNT; p++) if (R.prizes >> p & 1) { all &= R.pz[p].won; if (R.pz[p].nw > 1) sharedSeen++; }
    allPrizes += all || gameBogeys > 0;      // a Bogey can put everyone out of a prize; then it goes unwon
    if (!gameBogeys) CHECK(all);
  }
  CHECK(ended == games);
  CHECK(allPrizes == games);
  CHECK(sharedSeen > 0 && bogeys > 0);
  printf("  %d whole games (2–26 players, 1–6 tickets each): all ended, every prize won; %d shared prizes, %d bogeys\n",
         games, sharedSeen, bogeys);

  // ── the room list, leaving, host hand-over, rooms closing
  memset(rooms, 0, sizeof rooms);
  now = 1000;
  j = call("/create", Args{{{"n", "A"}}}, now);
  std::string c = field(j, "code"), ta = field(j, "tok");
  std::string tb = field(call("/join", Args{{{"c", c}, {"n", "B"}}}, now), "tok");
  CHECK(call("/rooms", Args{}, now).find(c) != std::string::npos);
  Room &R = *byCode(c.c_str());
  tick(now + HOST_GONE_MS + 1);                 // A went quiet, B didn't either: nobody to hand to
  CHECK(R.host == 0);
  call("/state", Args{{{"c", c}, {"t", tb}}}, now + HOST_GONE_MS + 2);
  tick(now + HOST_GONE_MS + 3);
  CHECK(R.host == 1);
  call("/leave", Args{{{"c", c}, {"t", ta}}}, now + HOST_GONE_MS + 4);
  CHECK(R.np == 1 && !strcmp(R.pl[0].name, "B") && R.host == 0);
  tick(now + HOST_GONE_MS + 4 + ROOM_GONE_MS + 1);
  CHECK(!R.used);
  for (int i = 0; i < MAX_ROOMS; i++) call("/create", Args{{{"n", "x"}}}, now);
  CHECK(field(call("/create", Args{{{"n", "x"}}}, now), "err") == "full");

  // ── JSON stays well-formed with the biggest room
  memset(rooms, 0, sizeof rooms);
  j = call("/create", Args{{{"n", "Host"}, {"k", "6"}}}, now);
  c = field(j, "code");
  for (int i = 1; i < MAX_PLAYERS; i++) call("/join", Args{{{"c", c}, {"n", "Player with a long name"}, {"k", "6"}}}, now);
  CHECK(field(call("/join", Args{{{"c", c}, {"n", "one too many"}}}, now), "err") == "roomfull");
  Room &B = *byCode(c.c_str());
  for (int i = 0; i < 90; i++) B.d.next();
  j = call("/state", Args{{{"c", c}, {"t", field(call("/join", Args{{{"c", c}, {"t", ""}}}, now), "tok")}}}, now);
  std::string full = call("/state", Args{{{"c", c}, {"t", ([&] { char b2[9]; snprintf(b2, 9, "%08lx", (unsigned long)B.pl[0].tok); return std::string(b2); })()}}}, now);
  CHECK(full.size() < sizeof buf - 100 && full.back() == '}');
  int depth = 0; bool inStr = false; for (char ch : full) { if (ch == '"') inStr = !inStr; else if (!inStr) depth += (ch == '{' || ch == '[') - (ch == '}' || ch == ']'); }
  CHECK(depth == 0 && !inStr);
  printf("  biggest state reply (30 players, 6 tickets, all 90 called): %zu bytes\n", full.size());

  printf("%d passed, %d failed\n", passes, fails);
  if (!fails) printf("ALL PASS\n");
  return fails ? 1 : 0;
}
