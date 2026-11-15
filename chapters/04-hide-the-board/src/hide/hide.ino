// Hide the board: a hot-and-cold hunt where the board is the thing you are looking for.
//
// Everyone joins the board's WiFi. One player hides the board — on a power bank — somewhere in
// the house, and everyone else hunts for it with their phone. Each phone shows a meter that
// warms up as it gets closer, read from how strongly the board hears *that* phone.
//
// Whoever finds it presses the BOOT button on the board. The board does not ask who pressed:
// it credits the hunter whose phone it hears loudest at that moment, so you can only win by
// actually holding it.
//
// Nothing is attached. The button and the blue LED are the two already on the DevKit.
//
// Board: ESP32 DevKit V1, run from a power bank (unplugging it restarts the game).
// Join the WiFi below, then open http://192.168.4.1

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>                   // esp_random(); NOT reachable via Arduino.h on core 3.x
#include <esp_wifi.h>                     // esp_wifi_ap_get_sta_list(): each phone's MAC and signal
#include <esp_wifi_ap_get_sta_list.h>     // ...and the DHCP lease that turns an IP into that MAC
#include <strings.h>                      // strcasecmp
#include "ui.h"

const char *AP_NAME = "devkit-hide";
const char *AP_PASS = "";                 // open; set 8+ characters to lock it

const int MAX_STATIONS = 8;               // the DHCP ceiling from chapter 1
const int MAX_PLAYERS = 8;
const int NAME_LEN = 12;
const uint32_t IDLE_MS = 60000;           // a player is dropped this long after their phone goes quiet
const uint32_t HIDE_MS = 60000;           // how long the hider has; BOOT starts the hunt sooner

// The meter. Signal strength in dBm, mapped onto 0–100 "heat". -90 is the edge of the board's
// range; -35 is a phone held against it. Both are starting guesses, written to be adjusted
// once a real house has been walked.
const float FAR_DBM = -90;
const float NEAR_DBM = -35;
const float CLAIM_DBM = -55;              // to win, the board must hear you at least this loud
const float SMOOTH = 0.3f;                // how much each new reading moves the meter (0–1)
const float TREND_DB = 2.0f;              // a change smaller than this is noise, not "warmer"

const int KEY_PIN = 0;                   // the BOOT button: pulled up, LOW while pressed
const int BLUE_LED = 2;                    // the blue LED

enum Phase { LOBBY, HIDING, HUNT, FOUND };
static const char *PHASE_NAME[] = {"lobby", "hiding", "hunt", "found"};

struct Player {
  uint32_t tok;                           // 0 = empty slot
  char name[NAME_LEN + 1];
  uint32_t seen;
  bool heard;                             // has the board measured this phone yet?
  int8_t raw;                             // the last reading, unsmoothed
  float ema;                              // the smoothed reading the meter shows
  float past[4];                          // ema once a second, oldest first — the trend looks
  uint8_t pastN;                          // back three seconds rather than one noisy reading
  uint16_t wins;
};

struct Game {
  Phase phase;
  uint32_t at;                            // when the current phase began
  int hider;                              // player index, -1 if none
  int winner;                             // player index, -1 if nobody found it
  uint32_t huntMs;                        // how long the last hunt took
  uint32_t ver;
};

Player players[MAX_PLAYERS];
Game g = {LOBBY, 0, -1, -1, 0, 0};
uint32_t flashUntil = 0, flashRate = 0;   // a short LED pattern, on top of the phase's own

WebServer web(80);
DNSServer dns;

// ── which phone is this, and how loud is it? ─────────────────────────────────

// A web request carries the phone's IP address. The radio knows each phone by MAC address and
// signal. The DHCP server's lease table is the bridge: IP -> MAC -> the signal the radio last
// measured from that MAC. 0 means "no reading" (no lease yet, or not associated).
static int8_t rssiOf(uint32_t ip) {
  wifi_sta_list_t radio;
  wifi_sta_mac_ip_list_t leases;
  if (esp_wifi_ap_get_sta_list(&radio) != ESP_OK) return 0;
  if (esp_wifi_ap_get_sta_list_with_ip(&radio, &leases) != ESP_OK) return 0;
  for (int i = 0; i < leases.num; i++) {
    if (leases.sta[i].ip.addr != ip) continue;
    for (int j = 0; j < radio.num; j++)
      if (!memcmp(radio.sta[j].mac, leases.sta[i].mac, 6)) return radio.sta[j].rssi;
  }
  return 0;
}

static void flash(uint32_t rate, uint32_t ms) { flashRate = rate; flashUntil = millis() + ms; }

// ── players ──────────────────────────────────────────────────────────────────

static Player *byToken(uint32_t t) {
  if (!t) return NULL;
  for (int i = 0; i < MAX_PLAYERS; i++) if (players[i].tok == t) return &players[i];
  return NULL;
}

static int indexOf(const Player *p) { return p ? (int)(p - players) : -1; }

static Player *byName(const char *n) {
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (players[i].tok && !strcasecmp(players[i].name, n)) return &players[i];
  return NULL;
}

static uint32_t newToken() {
  uint32_t t;
  do { t = esp_random(); } while (!t || byToken(t));
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

static bool isHunter(int i) { return players[i].tok && i != g.hider; }

static int hunters() {
  int n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (isHunter(i)) n++;
  return n;
}

// ── the meter ────────────────────────────────────────────────────────────────

// One reading from the radio, folded into the smoothed value. An exponential moving average:
// each reading moves the meter 30% of the way towards it. A phone's signal jumps by 5–10 dB
// when a hand moves or a body turns, and an unsmoothed meter flickers between hot and cold
// while you stand still — which is the whole problem this chapter is about.
static void hear(Player &p, int8_t dbm) {
  if (!dbm) return;
  p.raw = dbm;
  p.ema = p.heard ? p.ema + SMOOTH * (dbm - p.ema) : dbm;
  p.heard = true;
}

// Called once a second: remember where the meter was, so "warmer" compares against three
// seconds ago rather than against the last, noisy reading.
static void remember(Player &p) {
  if (!p.heard) return;
  if (p.pastN < 4) { p.past[p.pastN++] = p.ema; return; }
  for (int i = 0; i < 3; i++) p.past[i] = p.past[i + 1];
  p.past[3] = p.ema;
}

static int trend(const Player &p) {
  if (p.pastN < 4) return 0;
  const float d = p.ema - p.past[0];
  return d > TREND_DB ? 1 : d < -TREND_DB ? -1 : 0;
}

static int heat(const Player &p) {
  if (!p.heard) return -1;
  const float h = (p.ema - FAR_DBM) / (NEAR_DBM - FAR_DBM) * 100;
  return h < 0 ? 0 : h > 100 ? 100 : (int)(h + 0.5f);
}

// 1 = the hunter the board hears loudest. 0 if this phone has not been heard.
static int rankOf(int me) {
  if (!players[me].heard) return 0;
  int r = 1;
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (i != me && isHunter(i) && players[i].heard && players[i].ema > players[me].ema) r++;
  return r;
}

// ── the game ─────────────────────────────────────────────────────────────────

static void setPhase(Phase ph) {
  g.phase = ph;
  g.at = millis();
  g.ver++;
  Serial.printf("%7lu phase -> %s\n", (unsigned long)millis(), PHASE_NAME[ph]);
}

static void startHunt() {
  // The trend starts from scratch: three seconds of "where you were while your eyes were
  // closed" is not a useful thing to compare against.
  for (int i = 0; i < MAX_PLAYERS; i++) players[i].pastN = 0;
  setPhase(HUNT);
}

static void finish(int winner) {
  g.winner = winner;
  g.huntMs = millis() - g.at;
  if (winner >= 0) players[winner].wins++;
  setPhase(FOUND);
  flash(80, 3000);
}

// Who gets the credit when BOOT is pressed during a hunt: the hunter the board hears loudest,
// as long as that is loud enough to mean "holding it", and their phone has spoken in the last
// three seconds. Not the hider — they know where it is.
static int claimant() {
  int best = -1;
  const uint32_t now = millis();
  for (int i = 0; i < MAX_PLAYERS; i++) {
    const Player &p = players[i];
    if (!isHunter(i) || !p.heard || now - p.seen > 3000 || p.ema < CLAIM_DBM) continue;
    if (best < 0 || p.ema > players[best].ema) best = i;
  }
  return best;
}

static void pressed() {
  Serial.printf("%7lu BOOT pressed (%s)\n", (unsigned long)millis(), PHASE_NAME[g.phase]);
  if (g.phase == HIDING) { startHunt(); return; }      // the hider says "it's hidden"
  if (g.phase != HUNT) return;
  const int w = claimant();
  if (w < 0) {
    // Pressed, but no hunter is close enough. Two quick blinks: "not you".
    Serial.printf("%7lu   nobody close enough to claim it\n", (unsigned long)millis());
    flash(120, 480);
    return;
  }
  Serial.printf("%7lu   found by %s at %.0f dBm\n", (unsigned long)millis(), players[w].name,
                players[w].ema);
  finish(w);
}

static void dropPlayer(int i) {
  players[i].tok = 0;
  const bool live = g.phase == HIDING || g.phase == HUNT;
  if (i == g.winner) g.winner = -1;
  if (i == g.hider) {
    g.hider = -1;
    if (live) setPhase(LOBBY);           // nobody left who knows where it is
  }
  if ((g.phase == HIDING || g.phase == HUNT) && hunters() == 0) setPhase(LOBBY);
  g.ver++;
}

static void sweep() {
  const uint32_t now = millis();
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (players[i].tok && now - players[i].seen > IDLE_MS) dropPlayer(i);
  if (g.phase == HIDING && now - g.at >= HIDE_MS) startHunt();

  static uint32_t lastSecond = 0;
  if (now - lastSecond >= 1000) {
    lastSecond = now;
    for (int i = 0; i < MAX_PLAYERS; i++) if (players[i].tok) remember(players[i]);
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

static void ok() { web.send(200, "application/json", "{\"ok\":true}"); }

// The player making this request, with their phone's signal measured on the way in. Every
// request from a phone is also a reading — the page polls twice a second, so that is the rate
// the meter updates at.
static Player *caller() {
  Player *p = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (!p) return NULL;
  p->seen = millis();
  hear(*p, rssiOf((uint32_t)web.client().remoteIP()));
  return p;
}

// POST /join  n=<name>[&t=<token>]  -> { ok, token, name }
static void handleJoin() {
  sweep();
  Player *mine = caller();
  if (mine) {
    web.send(200, "application/json", "{\"ok\":true,\"token\":\"" + String(mine->tok, HEX)
             + "\",\"name\":\"" + uiJson(mine->name) + "\"}");
    return;
  }
  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);
  if (!name[0]) return fail("name");
  if (byName(name)) return fail("taken");
  int slot = -1;
  for (int i = 0; i < MAX_PLAYERS; i++) if (!players[i].tok) { slot = i; break; }
  if (slot < 0) return fail("full");

  Player &p = players[slot];
  memset(&p, 0, sizeof(Player));
  p.tok = newToken();
  strncpy(p.name, name, NAME_LEN);
  p.seen = millis();
  hear(p, rssiOf((uint32_t)web.client().remoteIP()));
  g.ver++;
  trace(p.name);
  web.send(200, "application/json", "{\"ok\":true,\"token\":\"" + String(p.tok, HEX)
           + "\",\"name\":\"" + uiJson(p.name) + "\"}");
}

// GET /me?t=<token>  -- the game as this player sees it, and their reading
static void handleMe() {
  sweep();
  Player *p = caller();
  if (!p) return fail("gone");
  const int me = indexOf(p);
  const uint32_t now = millis();
  // Other hunters' heat is hidden during a hunt — it would say where the board is. The hider
  // sees everyone; at the end everyone sees everyone.
  const bool seeAll = g.phase == FOUND || g.phase == LOBBY || me == g.hider;

  String o;
  o.reserve(420);
  o += "{\"ok\":true,\"phase\":\"";
  o += PHASE_NAME[g.phase];
  o += "\",\"me\":{\"name\":\"" + uiJson(p->name) + "\"";
  o += ",\"hider\":" + String(me == g.hider ? "true" : "false");
  o += ",\"heat\":" + String(heat(*p));
  o += ",\"dbm\":" + String(p->heard ? (int)lroundf(p->ema) : 0);
  o += ",\"raw\":" + String(p->raw);
  o += ",\"trend\":" + String(trend(*p));
  o += ",\"rank\":" + String(isHunter(me) ? rankOf(me) : 0);
  o += "},\"hunters\":" + String(hunters());
  o += ",\"left\":" + String(g.phase == HIDING ? (long)(HIDE_MS - (now - g.at)) : 0L);
  o += ",\"t\":" + String(g.phase == HUNT ? (long)(now - g.at) : (long)g.huntMs);
  o += ",\"hider\":\"" + String(g.hider >= 0 ? uiJson(players[g.hider].name) : String("")) + "\"";
  o += ",\"winner\":\"" + String(g.winner >= 0 ? uiJson(players[g.winner].name) : String("")) + "\"";
  o += ",\"claim\":" + String((int)CLAIM_DBM);
  o += ",\"players\":[";
  bool first = true;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].tok) continue;
    if (!first) o += ',';
    first = false;
    o += "{\"n\":\"" + uiJson(players[i].name) + "\",\"w\":" + String(players[i].wins)
       + ",\"h\":" + String(seeAll && i != g.hider ? heat(players[i]) : -1)
       + ",\"me\":" + String(i == me ? 1 : 0) + ",\"hd\":" + String(i == g.hider ? 1 : 0) + "}";
  }
  o += "],\"ver\":" + String(g.ver) + "}";
  web.send(200, "application/json", o);
}

// POST /hide  t=<token>  -- "I'll hide it": the caller becomes the hider
static void handleHide() {
  sweep();
  Player *p = caller();
  if (!p) return fail("gone");
  if (g.phase != LOBBY && g.phase != FOUND) return fail("busy");
  int n = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) if (players[i].tok) n++;
  if (n < 2) return fail("alone");         // a hider and at least one hunter
  g.hider = indexOf(p);
  g.winner = -1;
  trace(p->name);
  setPhase(HIDING);
  ok();
}

// POST /go  t=<token>  -- the hider starts the hunt from the phone instead of the button
static void handleGo() {
  sweep();
  Player *p = caller();
  if (!p) return fail("gone");
  if (g.phase != HIDING || indexOf(p) != g.hider) return fail("not");
  trace("go");
  startHunt();
  ok();
}

// POST /end  t=<token>  -- the hider gives up on the hunters: nobody wins this one
static void handleEnd() {
  sweep();
  Player *p = caller();
  if (!p) return fail("gone");
  if (g.phase != HUNT || indexOf(p) != g.hider) return fail("not");
  trace("end");
  finish(-1);
  ok();
}

// POST /lobby  t=<token>  -- back to the lobby after a round
static void handleLobby() {
  sweep();
  Player *p = caller();
  if (!p) return fail("gone");
  if (g.phase == FOUND) { g.hider = -1; setPhase(LOBBY); }
  ok();
}

// POST /leave  t=<token>
static void handleLeave() {
  Player *p = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (p) dropPlayer(indexOf(p));
  ok();
}

static const char BODY[] PROGMEM = R"HTML(
<style>
.view{display:none}
.err{color:var(--red);font-size:13px;min-height:18px;margin:8px 0 0}
.jbig{font-size:24px;font-weight:800;letter-spacing:-.03em;margin:0 0 6px}
.who{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid var(--line)}
.who:last-child{border-bottom:0}
.who b{flex:1;font-weight:700}
.who .w{color:var(--amber);font-weight:800;font-variant-numeric:tabular-nums}
.who .tag{font-size:11px;color:var(--mute);border:1px solid var(--line);border-radius:999px;padding:2px 8px}
.hb{flex:0 0 38%;height:8px;border-radius:999px;background:var(--card2);overflow:hidden}
.hb i{display:block;height:100%;border-radius:999px;transition:width .4s,background .4s}
.center{text-align:center}
.huge{font-size:64px;font-weight:900;letter-spacing:-.04em;line-height:1;font-variant-numeric:tabular-nums}
.meter{width:min(72vw,290px);aspect-ratio:1;border-radius:50%;margin:8px auto 18px;display:flex;
  flex-direction:column;align-items:center;justify-content:center;color:#10121a;
  transition:background .5s,box-shadow .5s}
.meter .word{font-size:30px;font-weight:900;letter-spacing:-.02em;text-transform:uppercase}
.meter .pct{font-size:14px;font-weight:700;opacity:.75;margin-top:2px}
.trend{font-size:22px;font-weight:800;min-height:32px;text-align:center}
.trend.up{color:var(--red)} .trend.down{color:#5ec8f0} .trend.same{color:var(--mute)}
.facts{display:flex;justify-content:center;gap:10px;flex-wrap:wrap;margin:6px 0 4px}
.tip{color:var(--faint);font-size:12.5px;text-align:center;margin:14px auto 0;max-width:380px;line-height:1.55}
.signal{font-size:13px;color:var(--mute)}
</style>

<header><span class="dot"></span><div>
  <h1>Hide the board</h1><p class="sub" id="sub">a hot-and-cold hunt for the board itself</p>
</div></header>

<!-- ── join ────────────────────────────────────────────────────── -->
<div class="view" id="v-join">
  <div class="card">
    <p class="jbig">Who's playing?</p>
    <p class="sub" style="margin-bottom:14px">One person hides this board somewhere nearby.
      Everyone else finds it with their phone — it gets hotter as you get closer.</p>
    <div class="row">
      <input id="jn" placeholder="your name" maxlength="12" autocomplete="off">
      <button class="narrow" id="jb">join</button>
    </div>
    <p class="err" id="jerr"></p>
  </div>
</div>

<!-- ── lobby ───────────────────────────────────────────────────── -->
<div class="view" id="v-lobby">
  <div class="card">
    <p class="label">players</p>
    <div id="roster"></div>
  </div>
  <button id="hide">I'll hide it</button>
  <p class="err" id="herr"></p>
  <p class="tip">Run the board from a <b>power bank</b> before you start — unplugging it
    restarts the game. <span class="signal" id="lsig"></span></p>
</div>

<!-- ── hiding ──────────────────────────────────────────────────── -->
<div class="view" id="v-hiding">
  <div class="card center">
    <p class="label" id="hidelabel">close your eyes</p>
    <p class="huge" id="hidecount">60</p>
    <p class="sub" id="hidetext" style="margin-top:10px"></p>
  </div>
  <button id="go" class="mint" style="display:none">it's hidden — start the hunt</button>
</div>

<!-- ── hunt ────────────────────────────────────────────────────── -->
<div class="view" id="v-hunt">
  <div id="hunter">
    <div class="meter" id="meter"><span class="word" id="word">—</span><span class="pct" id="pct"></span></div>
    <p class="trend" id="trend"></p>
    <div class="facts"><span class="pill" id="rank">—</span><span class="pill mono" id="dbm">—</span>
      <span class="pill mono" id="clock">0:00</span></div>
    <p class="tip">Found it? Hold your phone next to it and press the <b>BOOT</b> button on the
      board. It goes to whoever the board hears loudest — so it has to be you holding it.<br>
      Readings jump when you turn or move your hand: walk slowly and watch the arrow, not the number.</p>
  </div>
  <div id="watcher">
    <div class="card">
      <p class="label">who's getting close <span class="mono" id="wclock" style="float:right">0:00</span></p>
      <div id="heats"></div>
    </div>
    <button class="ghost" id="end">nobody's going to find it — end the round</button>
  </div>
</div>

<!-- ── found ───────────────────────────────────────────────────── -->
<div class="view" id="v-found">
  <div class="card center">
    <p class="label">round over</p>
    <p class="jbig" id="result">—</p>
    <p class="sub" id="resultsub"></p>
  </div>
  <div class="card"><p class="label">scores</p><div id="scores"></div></div>
  <button id="again" class="mint">back to the lobby</button>
</div>

<script>
const $=i=>document.getElementById(i);
function call(u,o){
  const a=new AbortController(),k=setTimeout(()=>a.abort(),5000);
  return fetch(u,Object.assign({signal:a.signal,cache:'no-store'},o||{}))
    .then(r=>r.json()).finally(()=>clearTimeout(k));
}
const post=(u,b)=>call(u,{method:'POST',
  headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});
const enc=encodeURIComponent;
const NOREPLY='The board did not answer. Turn off mobile data and stay on the devkit-hide WiFi.';
let token='',timer=null,lastBand=-1;

function view(v){document.querySelectorAll('.view').forEach(e=>e.style.display=e.id==='v-'+v?'block':'none')}
function store(){try{localStorage.setItem('hide_t',token)}catch(e){}}
function forget(){try{localStorage.removeItem('hide_t')}catch(e){}}
const mmss=ms=>{const s=Math.floor(ms/1000);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')};

// 0 = freezing … 4 = burning. Colour runs from ice blue (hue 200) to red (hue 0).
const WORDS=['freezing','cold','warm','hot','burning'];
const band=h=>h<0?-1:Math.min(4,Math.floor(h/20));
const hue=h=>Math.round(200-2*Math.max(0,h));

const ERRS={name:'Pick a name first.',taken:'Someone here already has that name.',
  full:'Eight players is the most this board can take.',busy:'A round is already going.',
  alone:'You need at least one other player to hunt for it.'};

$('jb').onclick=async()=>{
  const n=$('jn').value.trim();if(!n){$('jerr').textContent=ERRS.name;return}
  $('jb').disabled=true;$('jerr').textContent='';
  try{const d=await post('/join','n='+enc(n));
    if(d.ok){token=d.token;store();try{localStorage.setItem('hide_n',d.name)}catch(e){};poll()}
    else $('jerr').textContent=ERRS[d.err]||'Could not join.'}
  catch(e){$('jerr').textContent=NOREPLY}
  finally{$('jb').disabled=false}
};
const act=async(u,errEl)=>{
  try{const d=await post(u,'t='+enc(token));if(!d.ok&&errEl)errEl.textContent=ERRS[d.err]||'Not now.'}
  catch(e){if(errEl)errEl.textContent=NOREPLY}
  poll();
};
$('hide').onclick=()=>{$('herr').textContent='';act('/hide',$('herr'))};
$('go').onclick=()=>act('/go');
$('end').onclick=()=>act('/end');
$('again').onclick=()=>act('/lobby');

function bar(h){
  const b=document.createElement('div');b.className='hb';
  const i=document.createElement('i');
  i.style.width=Math.max(0,h)+'%';i.style.background='hsl('+hue(h)+' 85% 60%)';
  b.appendChild(i);return b;
}
function rows(el,list,showHeat){
  el.innerHTML='';
  for(const p of list){
    const r=document.createElement('div');r.className='who';
    const n=document.createElement('b');n.textContent=p.n+(p.me?' (you)':'');r.appendChild(n);
    if(p.hd){const t=document.createElement('span');t.className='tag';t.textContent='hider';r.appendChild(t)}
    if(showHeat&&p.h>=0)r.appendChild(bar(p.h));
    const w=document.createElement('span');w.className='w';w.textContent=p.w;r.appendChild(w);
    el.appendChild(r);
  }
}

function draw(d){
  const me=d.me;
  $('sub').textContent='you are '+me.name+' · '+d.players.length+(d.players.length===1?' player':' players');
  if(d.phase==='lobby'){
    view('lobby');rows($('roster'),d.players,false);
    $('lsig').textContent=me.dbm?'Board hears you at '+me.dbm+' dBm.':'';
  }else if(d.phase==='hiding'){
    view('hiding');
    $('hidecount').textContent=Math.ceil(d.left/1000);
    if(me.hider){
      $('hidelabel').textContent='hide it now';
      $('hidetext').textContent='The blue light blinks while you hide it. Press BOOT on the board when it is hidden — or tap below.';
      $('go').style.display='block';
    }else{
      $('hidelabel').textContent='close your eyes';
      $('hidetext').textContent=d.hider+' is hiding the board. The hunt starts when they are done.';
      $('go').style.display='none';
    }
  }else if(d.phase==='hunt'){
    view('hunt');
    $('hunter').style.display=me.hider?'none':'block';
    $('watcher').style.display=me.hider?'block':'none';
    if(me.hider){
      $('wclock').textContent=mmss(d.t);
      rows($('heats'),d.players.filter(p=>!p.hd).sort((a,b)=>b.h-a.h),true);
    }else{
      const h=me.heat,b=band(h);
      $('meter').style.background=h<0?'var(--card2)':'hsl('+hue(h)+' 85% 62%)';
      $('meter').style.boxShadow=h<0?'none':'0 0 '+(10+h*.6)+'px hsl('+hue(h)+' 90% 55% / .55)';
      $('word').textContent=b<0?'listening…':WORDS[b];
      $('pct').textContent=h<0?'':h+'°';
      $('trend').textContent=me.trend>0?'warmer ↑':me.trend<0?'colder ↓':'steady';
      $('trend').className='trend '+(me.trend>0?'up':me.trend<0?'down':'same');
      $('rank').textContent=me.rank?(me.rank===1?'closest of ':'#'+me.rank+' of ')+d.hunters:'—';
      $('dbm').textContent=me.dbm?me.dbm+' dBm':'—';
      $('clock').textContent=mmss(d.t);
      // A short buzz when you cross into a hotter band — eyes can stay on the room.
      if(b>lastBand&&lastBand>=0&&navigator.vibrate)navigator.vibrate(40);
      lastBand=b;
    }
  }else{
    view('found');lastBand=-1;
    $('result').textContent=d.winner?d.winner+' found it':'Nobody found it';
    $('resultsub').textContent=(d.winner?'in ':'after ')+mmss(d.t)+' · hidden by '+(d.hider||'someone');
    rows($('scores'),d.players.slice().sort((a,b)=>b.w-a.w),false);
  }
}

async function poll(){
  clearTimeout(timer);
  if(!token){view('join');return}
  try{
    const d=await call('/me?t='+enc(token));
    if(!d.ok){token='';forget();view('join');$('jerr').textContent='The board restarted. Join again.';return}
    draw(d);
  }catch(e){$('sub').textContent='cannot reach the board — mobile data off?'}
  timer=setTimeout(poll,500);
}

(async function start(){
  try{token=localStorage.getItem('hide_t')||'';$('jn').value=localStorage.getItem('hide_n')||''}catch(e){}
  poll();
})();
</script>
)HTML";

static void handlePage() {
  trace("page");
  uiSend(web, "Hide the board", BODY);
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

// The BOOT button, debounced. A press is a stable LOW for 30 ms; contact bounce on a cheap
// tactile switch would otherwise read as several presses.
static void readButton() {
  static int last = HIGH, stable = HIGH;
  static uint32_t changed = 0;
  const int r = digitalRead(KEY_PIN);
  if (r != last) { last = r; changed = millis(); }
  if (millis() - changed > 30 && r != stable) {
    stable = r;
    if (stable == LOW) pressed();
  }
}

// The LED says what the board is doing, without giving the hiding place away: a steady blink
// while being hidden, dark during the hunt, and short patterns for a find or a refused claim.
static void drawLed() {
  const uint32_t now = millis();
  bool on = false;
  if (now < flashUntil) on = (now / flashRate) % 2;
  else if (g.phase == HIDING) on = (now / 500) % 2;
  digitalWrite(BLUE_LED, on ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED, OUTPUT);

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
  web.on("/me", handleMe);
  web.on("/join", HTTP_POST, handleJoin);
  web.on("/hide", HTTP_POST, handleHide);
  web.on("/go", HTTP_POST, handleGo);
  web.on("/end", HTTP_POST, handleEnd);
  web.on("/lobby", HTTP_POST, handleLobby);
  web.on("/leave", HTTP_POST, handleLeave);
  web.onNotFound(handleElse);
  web.begin();

  Serial.printf("\njoin \"%s\" then open http://%s\n", AP_NAME, ip.toString().c_str());
  Serial.printf("free heap %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  dns.processNextRequest();
  web.handleClient();
  readButton();
  drawLed();
  static uint32_t lastSweep = 0;
  if (millis() - lastSweep >= 200) { lastSweep = millis(); sweep(); }
}
