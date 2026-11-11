// A chat room that needs no internet, no router and no account.
//
// The board makes its own WiFi network. Phones join it and talk to each other. Nothing said
// in the room leaves the room, because there is nowhere for it to go.
//
// You pick a name once, on a join screen, and then it is yours. The board hands you a private
// token when you join and every message is posted with the token, never with a name — so a
// name cannot be changed after joining and cannot be borrowed by anyone else. A name is held
// for as long as you are in the room, and released a couple of minutes after you leave.
//
// Joining the network opens the page by itself on most phones — that is the captive-portal
// check every OS runs, answered with the chat instead of a login form.
//
// Board: ESP32 DevKit V1, nothing attached.
// Join the WiFi below, then open http://192.168.4.1

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>    // esp_random(); NOT reachable via Arduino.h on core 3.x
#include <esp_wifi.h>      // esp_wifi_ap_get_sta_list() — per-phone signal strength
#include <strings.h>       // strcasecmp
#include "ui.h"

const char *AP_NAME = "devkit-chat";
const char *AP_PASS = "";          // open; set 8+ characters to lock it

// How many phones may associate at once. The Arduino core defaults this to 4, which is the
// real reason a room of people cannot all get on — not the roster size below. 8 is the
// ceiling worth asking for: the core ships with CONFIG_LWIP_DHCPS_MAX_STATION_NUM=8, so a
// ninth phone would associate and then sit there with no DHCP lease, which looks like a
// broken network rather than a full one.
const int MAX_STATIONS = 8;

const int MAX_MSGS = 40;           // ring buffer — 520 KB of RAM is not a lot
const int MAX_LEN = 140;
const int MAX_USERS = 24;        // deliberately > MAX_STATIONS: a name is held for
                                 // IDLE_MS after a phone leaves, so names outlive radios
const int NAME_LEN = 15;
const uint32_t IDLE_MS = 120000;   // a name is released this long after a phone goes quiet

struct Msg {
  char who[NAME_LEN + 1];
  char text[MAX_LEN + 1];
  uint32_t at;
};

struct User {
  uint32_t token;                  // 0 = empty slot
  char name[NAME_LEN + 1];
  uint32_t seen;
};

Msg msgs[MAX_MSGS];
User users[MAX_USERS];
int count = 0;                     // total ever posted, and the id of the next one
uint32_t started = 0;

WebServer web(80);
DNSServer dns;

// ── the roster ───────────────────────────────────────────────────────────────

static bool expired(const User &u) { return millis() - u.seen > IDLE_MS; }

static void sweep() {
  for (int i = 0; i < MAX_USERS; i++)
    if (users[i].token && expired(users[i])) users[i].token = 0;
}

static User *byToken(uint32_t t) {
  if (!t) return NULL;
  for (int i = 0; i < MAX_USERS; i++)
    if (users[i].token == t) return &users[i];
  return NULL;
}

static bool sameName(const char *a, const char *b) {
  return strcasecmp(a, b) == 0;
}

static User *byName(const char *n) {
  for (int i = 0; i < MAX_USERS; i++)
    if (users[i].token && sameName(users[i].name, n)) return &users[i];
  return NULL;
}

static int activeCount() {
  int n = 0;
  for (int i = 0; i < MAX_USERS; i++) if (users[i].token) n++;
  return n;
}

// Trim, collapse whitespace, and drop control characters. A name is shown to everyone in the
// room, so it is cleaned here rather than trusted.
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

static uint32_t newToken() {
  uint32_t t = 0;
  do { t = esp_random(); } while (t == 0 || byToken(t));
  return t;
}

// ── endpoints ────────────────────────────────────────────────────────────────

// Signal strength of every associated phone, strongest first. The AP knows this for free —
// it is how "what is the range in this room" stops being a guess. Note these are radios, not
// names: the identity here is the token, and an HTTP request does not carry a MAC, so there
// is no honest way to label a bar with a person.
static void sendSignals(String &o) {
  wifi_sta_list_t list;
  o += ",\"sig\":[";
  if (esp_wifi_ap_get_sta_list(&list) == ESP_OK) {
    int8_t r[ESP_WIFI_MAX_CONN_NUM];
    int n = list.num;
    if (n > ESP_WIFI_MAX_CONN_NUM) n = ESP_WIFI_MAX_CONN_NUM;
    for (int i = 0; i < n; i++) r[i] = list.sta[i].rssi;
    for (int i = 1; i < n; i++) {                 // insertion sort, strongest first
      const int8_t k = r[i];
      int j = i - 1;
      while (j >= 0 && r[j] < k) { r[j + 1] = r[j]; j--; }
      r[j + 1] = k;
    }
    for (int i = 0; i < n; i++) { if (i) o += ','; o += String((int)r[i]); }
  }
  o += "]";
}

static void sendRoster(String &o) {
  o += ",\"who\":[";
  bool first = true;
  for (int i = 0; i < MAX_USERS; i++) {
    if (!users[i].token) continue;
    if (!first) o += ',';
    first = false;
    o += "\"" + uiJson(users[i].name) + "\"";
  }
  o += "]";
}

// POST /join   n=<name>          -> { ok, token, name } | { ok:false, err }
static void handleJoin() {
  sweep();
  char name[NAME_LEN + 1];
  cleanName(web.arg("n"), name);

  if (!name[0]) {
    web.send(200, "application/json", "{\"ok\":false,\"err\":\"empty\"}");
    return;
  }

  // Already holding a token? Then you are already in the room, and your name is settled.
  // We answer with the name you actually have and ignore whatever was asked for — so a
  // crafted request cannot rename you either, not just the page's own UI.
  User *mine = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (mine) {
    mine->seen = millis();
    String o = "{\"ok\":true,\"token\":\"" + String(mine->token, HEX)
             + "\",\"name\":\"" + uiJson(mine->name) + "\"}";
    web.send(200, "application/json", o);
    return;
  }

  if (byName(name)) {
    web.send(200, "application/json", "{\"ok\":false,\"err\":\"taken\"}");
    return;
  }

  int slot = -1;
  for (int i = 0; i < MAX_USERS; i++) if (!users[i].token) { slot = i; break; }
  if (slot < 0) {
    web.send(200, "application/json", "{\"ok\":false,\"err\":\"full\"}");
    return;
  }

  users[slot].token = newToken();
  strncpy(users[slot].name, name, NAME_LEN);
  users[slot].name[NAME_LEN] = 0;
  users[slot].seen = millis();

  String o = "{\"ok\":true,\"token\":\"" + String(users[slot].token, HEX)
           + "\",\"name\":\"" + uiJson(users[slot].name) + "\"}";
  web.send(200, "application/json", o);
}

// GET /msgs?since=<n>&t=<token>
static void handleMsgs() {
  sweep();
  const int since = web.arg("since").toInt();
  User *u = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (u) u->seen = millis();

  // Only what this client has not seen. Re-sending the whole log every second is what turns
  // twenty phones into a board that stops answering.
  const int from = max(since, count - MAX_MSGS);
  String o = "{\"ok\":" + String(u ? "true" : "false")
           + ",\"next\":" + String(count)
           + ",\"here\":" + String(activeCount())
           + ",\"devices\":" + String(WiFi.softAPgetStationNum())
           + ",\"up\":" + String((millis() - started) / 1000)
           + ",\"m\":[";
  for (int i = from; i < count; i++) {
    const Msg &m = msgs[i % MAX_MSGS];
    if (i > from) o += ',';
    o += "{\"w\":\"" + uiJson(m.who) + "\",\"x\":\"" + uiJson(m.text)
       + "\",\"t\":" + String(m.at / 1000) + "}";
  }
  o += "]";
  sendRoster(o);
  sendSignals(o);
  o += ",\"cap\":" + String(MAX_STATIONS);
  o += "}";
  web.send(200, "application/json", o);
}

// POST /say    t=<token>&x=<text>
// The name is looked up from the token. There is no way to post under another name.
static void handleSay() {
  sweep();
  User *u = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (!u) {
    web.send(200, "application/json", "{\"ok\":false,\"err\":\"unknown\"}");
    return;
  }
  u->seen = millis();

  String text = web.arg("x");
  text.trim();
  if (text.length()) {
    Msg &m = msgs[count % MAX_MSGS];
    strncpy(m.who, u->name, NAME_LEN);
    m.who[NAME_LEN] = 0;
    strncpy(m.text, text.c_str(), MAX_LEN);
    m.text[MAX_LEN] = 0;
    m.at = millis();
    count++;
  }
  web.send(200, "application/json", "{\"ok\":true}");
}

// POST /leave  t=<token>
static void handleLeave() {
  User *u = byToken((uint32_t)strtoul(web.arg("t").c_str(), NULL, 16));
  if (u) u->token = 0;
  web.send(200, "application/json", "{\"ok\":true}");
}

static const char BODY[] PROGMEM = R"HTML(
<style>
body{display:flex;flex-direction:column;min-height:100dvh;padding-bottom:0}
.wrap{display:flex;flex-direction:column;flex:1;width:100%}
#join{flex:1;display:flex;flex-direction:column;justify-content:center;gap:14px;padding-bottom:40px}
#chat{display:none;flex-direction:column;flex:1}
body.in #join{display:none}
body.in #chat{display:flex}
.jbig{font-size:26px;font-weight:800;letter-spacing:-.03em;margin:0 0 6px}
.err{color:var(--red);font-size:13px;min-height:18px;margin:2px 0 0}
#log{flex:1;overflow-y:auto;padding:4px 2px 10px;min-height:46vh}
.m{margin:0 0 11px;animation:fade .3s ease both;display:flex;gap:10px;align-items:flex-start}
.av{width:32px;height:32px;border-radius:11px;flex:none;display:flex;align-items:center;
  justify-content:center;font-size:13px;font-weight:800;color:#15171f}
.mb{background:var(--card);border:1px solid var(--line);border-radius:4px 16px 16px 16px;
  padding:9px 14px;max-width:78%}
.m.me{flex-direction:row-reverse}
.m.me .mb{background:linear-gradient(135deg,rgba(255,184,108,.17),rgba(255,143,171,.13));
  border-color:rgba(255,184,108,.3);border-radius:16px 4px 16px 16px}
.mb b{font-size:11.5px;font-weight:750;display:block;margin-bottom:2px;color:var(--amber)}
.mb span{font-size:14.5px;line-height:1.5;word-wrap:break-word;overflow-wrap:anywhere}
.youbar{display:flex;align-items:center;gap:9px;font-size:12.5px;color:var(--mute);
  padding:2px 2px 10px}
.youbar .me-dot{width:9px;height:9px;border-radius:50%;flex:none}
.youbar b{color:var(--ink);font-weight:750}
.youbar .ghost{margin-left:auto;padding:6px 12px;font-size:12px;width:auto;flex:none}
.roster{display:flex;flex-wrap:wrap;gap:6px;padding:0 2px 10px}
.sigbox{margin:0 2px 10px;font-size:12.5px;color:var(--mute)}
.sigbox summary{cursor:pointer;padding:4px 0;color:var(--mute)}
.sigrow{display:flex;align-items:center;gap:8px;padding:4px 0}
.sigbar{flex:1;height:6px;border-radius:999px;background:var(--card2);overflow:hidden}
.sigbar i{display:block;height:100%;border-radius:999px;transition:width .4s}
.sigrow b{flex:0 0 58px;text-align:right;font-variant-numeric:tabular-nums;font-size:11.5px}
.chip{font-size:11.5px;padding:4px 10px;border-radius:999px;background:var(--card2);
  border:1px solid var(--line);color:var(--mute)}
form{display:flex;gap:9px;padding:12px 0 calc(14px + env(safe-area-inset-bottom));
  position:sticky;bottom:0;background:linear-gradient(to top,var(--bg) 72%,transparent)}
#t{flex:1}
form button{flex:0 0 68px;border-radius:14px}
</style>

<header><span class="dot"></span><div>
  <h1>Chat</h1><p class="sub" id="sub">nothing said here leaves the room</p>
</div></header>

<!-- ── join screen ─────────────────────────────────────────────── -->
<div id="join">
  <div class="card">
    <p class="jbig">What should we call you?</p>
    <p class="sub" style="margin-bottom:14px">Pick a name once. After you join it is yours —
      you cannot change it, and nobody else in the room can use it.</p>
    <form id="jf" autocomplete="off">
      <div class="row">
        <input id="jn" placeholder="your name" maxlength="15" required autofocus>
        <button class="narrow" id="jb">join</button>
      </div>
    </form>
    <p class="err" id="jerr"></p>
  </div>
  <div class="card tight" id="jwho" style="display:none">
    <p class="label">already here</p>
    <div class="roster" id="jroster" style="padding:0"></div>
  </div>
</div>

<!-- ── chat ────────────────────────────────────────────────────── -->
<div id="chat">
  <div class="youbar">
    <span class="me-dot" id="medot"></span>
    <span>you are <b id="mename">—</b></span>
    <button class="ghost" onclick="leave()">leave</button>
  </div>
  <div class="roster" id="roster"></div>
  <details class="sigbox"><summary id="sigsum">signal —</summary>
    <div id="sigs"></div>
    <p class="sub" style="margin:8px 0 0">one row per connected radio, strongest first.
      walk away until a bar turns red and that is your range in this room.</p>
  </details>
  <div id="log"><p class="empty">no messages yet — say something</p></div>
  <form id="f" autocomplete="off">
    <input id="t" placeholder="message" maxlength="140" required autocomplete="off">
    <button>send</button>
  </form>
</div>

<script>
const $=i=>document.getElementById(i);
const log=$('log');
let seen=0,me='',token='',timer=null;
const COL=['#ff8f9b','#8ec5ff','#ffd97a','#8fe6b4','#c4b0ff','#7ee7c7'];
const tint=s=>COL[[...s].reduce((a,c)=>a+c.charCodeAt(0),0)%COL.length];
const post=(u,b)=>fetch(u,{method:'POST',
  headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());

function store(){try{localStorage.setItem('chat_t',token);localStorage.setItem('chat_n',me)}catch(e){}}
function forget(){try{localStorage.removeItem('chat_t');localStorage.removeItem('chat_n')}catch(e){}}

function enterChat(){
  document.body.classList.add('in');
  $('mename').textContent=me;
  $('medot').style.background=tint(me);
  $('t').focus();
}

function backToJoin(msg){
  clearTimeout(timer);
  document.body.classList.remove('in');
  token='';me='';seen=0;
  log.innerHTML='<p class="empty">no messages yet — say something</p>';
  forget();
  $('jerr').textContent=msg||'';
  $('jn').focus();
  roster();
}

const ERRS={taken:'Someone in the room is already using that name.',
            empty:'That name is empty once the spaces come off.',
            full:'The room is full — someone has to leave first.'};

$('jf').onsubmit=async e=>{
  e.preventDefault();
  const n=$('jn').value.trim();
  if(!n)return;
  $('jb').disabled=true;$('jerr').textContent='';
  try{
    const d=await post('/join','n='+encodeURIComponent(n)+'&t='+encodeURIComponent(token));
    if(!d.ok){$('jerr').textContent=ERRS[d.err]||'Could not join.';return}
    token=d.token;me=d.name;store();
    enterChat();poll();
  }catch(e){$('jerr').textContent='The board did not answer. Still on its WiFi?'}
  finally{$('jb').disabled=false}
};

async function leave(){
  try{await post('/leave','t='+encodeURIComponent(token))}catch(e){}
  backToJoin('');
}

// dBm -> a percentage and a colour. -50 and better is as good as it gets indoors;
// below about -80 a phone technically stays associated and in practice stops working.
function sigQuality(dbm){
  const pct=Math.max(4,Math.min(100,Math.round((dbm+95)*(100/45))));
  const col=dbm>-55?'var(--mint)':dbm>-67?'var(--amber)':dbm>-78?'#ffd97a':'var(--red)';
  const word=dbm>-55?'strong':dbm>-67?'good':dbm>-78?'weak':'about to drop';
  return {pct,col,word};
}

function drawSignals(list,cap){
  const box=$('sigs');
  $('sigsum').textContent='signal — '+list.length+' of '+cap+' radios'
    +(list.length?' · weakest '+list[list.length-1]+' dBm':'');
  box.innerHTML='';
  for(const dbm of list){
    const q=sigQuality(dbm);
    const row=document.createElement('div');row.className='sigrow';
    const bar=document.createElement('div');bar.className='sigbar';
    const fill=document.createElement('i');
    fill.style.width=q.pct+'%';fill.style.background=q.col;
    bar.appendChild(fill);
    const lab=document.createElement('b');lab.textContent=dbm+' dBm';
    const w=document.createElement('span');w.textContent=q.word;w.style.flex='0 0 86px';
    row.appendChild(bar);row.appendChild(lab);row.appendChild(w);
    box.appendChild(row);
  }
}

function drawRoster(el,names){
  el.innerHTML='';
  for(const n of names){
    const c=document.createElement('span');
    c.className='chip';c.textContent=n;
    if(n===me){c.style.color='var(--amber)';c.style.borderColor='rgba(255,184,108,.35)'}
    el.appendChild(c);
  }
}

// On the join screen, show who is already in the room so you can avoid a clash.
async function roster(){
  if(document.body.classList.contains('in'))return;
  try{
    const d=await (await fetch('/msgs?since=999999')).json();
    $('jwho').style.display=d.who.length?'block':'none';
    drawRoster($('jroster'),d.who);
  }catch(e){}
  setTimeout(roster,3000);
}

async function poll(){
  clearTimeout(timer);
  try{
    const d=await (await fetch('/msgs?since='+seen+'&t='+encodeURIComponent(token))).json();
    if(!d.ok){backToJoin('The board restarted, so names were released. Join again.');return}
    for(const m of d.m){
      const row=document.createElement('div');
      row.className='m'+(m.w===me?' me':'');
      const av=document.createElement('div');
      av.className='av';av.style.background=tint(m.w);
      av.textContent=(m.w[0]||'?').toUpperCase();
      const b=document.createElement('div');b.className='mb';
      const nm=document.createElement('b');nm.textContent=m.w;
      const tx=document.createElement('span');tx.textContent=m.x;
      b.appendChild(nm);b.appendChild(tx);
      row.appendChild(av);row.appendChild(b);
      const first=log.firstElementChild;
      if(first&&first.className==='empty')log.innerHTML='';
      log.appendChild(row);
    }
    if(d.m.length){seen=d.next;log.scrollTop=log.scrollHeight}
    drawRoster($('roster'),d.who);
    drawSignals(d.sig||[],d.cap||0);
    $('sub').textContent=d.here+(d.here===1?' person':' people')+' in the room'
      +(d.devices>=d.cap?' · room full':'');
  }catch(e){$('sub').textContent='lost the board'}
  timer=setTimeout(poll,900);
}

$('f').onsubmit=async e=>{
  e.preventDefault();
  const t=$('t');
  const text=t.value;
  if(!text.trim())return;
  t.value='';
  try{
    const d=await post('/say','t='+encodeURIComponent(token)+'&x='+encodeURIComponent(text));
    if(!d.ok){backToJoin('The board restarted, so names were released. Join again.');return}
  }catch(e){t.value=text}
  poll();
};

// A reload should not cost you your name: ask the board to confirm the token we kept.
(async function start(){
  try{token=localStorage.getItem('chat_t')||'';me=localStorage.getItem('chat_n')||''}catch(e){}
  if(token&&me){
    try{
      const d=await post('/join','n='+encodeURIComponent(me)+'&t='+encodeURIComponent(token));
      if(d.ok){token=d.token;me=d.name;store();enterChat();poll();return}
    }catch(e){}
    forget();token='';me='';
  }
  $('jn').value=me||'';
  roster();
})();
</script>
)HTML";

static void handlePage() { uiSend(web, "Chat", BODY); }

void setup() {
  Serial.begin(115200);
  delay(300);
  started = millis();

  WiFi.mode(WIFI_AP);
  // (ssid, pass, channel, hidden, max_connection) — the 4th positional default is the
  // one that silently caps the room at four phones.
  WiFi.softAP(AP_NAME, strlen(AP_PASS) ? AP_PASS : NULL, 1, 0, MAX_STATIONS);
  const IPAddress ip = WiFi.softAPIP();

  // Answer every DNS lookup with our own address. That is what makes a phone decide it is
  // behind a portal and open the page without being asked.
  dns.start(53, "*", ip);

  web.on("/", handlePage);
  web.on("/msgs", handleMsgs);
  web.on("/join", HTTP_POST, handleJoin);
  web.on("/say", HTTP_POST, handleSay);
  web.on("/leave", HTTP_POST, handleLeave);
  // Every OS probes a different URL to test for a portal; serving the page for all of them
  // is simpler than listing them.
  web.onNotFound(handlePage);
  web.begin();

  Serial.printf("\njoin \"%s\" then open http://%s\n", AP_NAME, ip.toString().c_str());
}

void loop() {
  dns.processNextRequest();
  web.handleClient();
}
