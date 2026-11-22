#pragma once
// The page: start or join a game, the lobby, your tickets and claims, the host's controls, the
// big-screen caller (open it with #tv=CODE), and the results. The calls are spoken by the
// browser in English or Hindi.

static const char TAMBOLA_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><title>Tambola</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
:root{--bg:#0d1117;--card:#161b24;--card2:#1c2230;--line:#2a3140;--ink:#eef1f6;--mute:#8b95a7;--faint:#5b6476;
  --acc:#f59e0b;--acc2:#ef4444;--ok:#22c55e;--blue:#3b82f6;--tk:#fff7e6;--tkInk:#3b2a12;color-scheme:dark}
body{background:var(--bg);color:var(--ink);min-height:100vh;-webkit-font-smoothing:antialiased;
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Inter,system-ui,sans-serif;
  padding:max(14px,env(safe-area-inset-top)) 12px calc(24px + env(safe-area-inset-bottom))}
.wrap{max-width:560px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin:4px 0 14px}
.logo{width:34px;height:34px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#fde68a,#f59e0b 55%,#b45309);display:grid;place-items:center;font-weight:800;font-size:13px;color:#3b2a12;flex:none;box-shadow:inset 0 -2px 4px rgba(0,0,0,.25)}
h1{font-size:20px;font-weight:700;letter-spacing:-.02em;line-height:1.2}
.sub{color:var(--mute);font-size:13px}
header .r{margin-left:auto;display:flex;gap:6px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 12px}
.label{color:var(--mute);font-size:12px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;margin:0 0 10px;display:flex;align-items:center;gap:8px}
.label .r{margin-left:auto;text-transform:none;letter-spacing:0;font-weight:500}
input{width:100%;font:inherit;color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:12px 14px;outline:0}
input:focus{border-color:var(--acc)}
button{font:inherit;font-weight:650;cursor:pointer;border:1px solid transparent;border-radius:10px;padding:11px 14px;background:var(--acc);color:#1a1205;display:inline-flex;align-items:center;justify-content:center;gap:6px}
button:active{transform:translateY(1px)}button:disabled{opacity:.4;cursor:default}
.ghost{background:transparent;border-color:var(--line);color:var(--ink)}
.small{padding:6px 11px;font-size:13px}
.wide{width:100%}
.row{display:flex;gap:8px;align-items:center}.row>*{flex:1}
.seg{display:flex;gap:4px;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:3px}
.seg button{flex:1;background:transparent;color:var(--mute);padding:7px 6px;font-size:13px;border-radius:7px;font-weight:600}
.seg button.sel{background:var(--card2);color:var(--ink);box-shadow:0 0 0 1px #3a4458}
.step{display:flex;align-items:center;gap:10px}
.step button{width:38px;height:38px;padding:0;font-size:20px}
.step b{font-size:20px;min-width:24px;text-align:center;font-variant-numeric:tabular-nums}
.set{display:grid;grid-template-columns:96px 1fr;gap:8px;align-items:center;margin:0 0 10px;font-size:14px;color:var(--mute)}
.games{display:flex;flex-direction:column;gap:6px;margin-top:10px}
.games button{justify-content:space-between;background:var(--bg);border-color:var(--line);color:var(--ink);font-weight:500}
.games b{font-family:ui-monospace,Menlo,monospace;letter-spacing:.12em;color:var(--acc)}
.code{font:800 34px/1 ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.18em;color:var(--acc)}
.people{display:flex;flex-wrap:wrap;gap:6px}
.person{background:var(--bg);border:1px solid var(--line);border-radius:999px;padding:5px 11px;font-size:13px;display:flex;gap:6px;align-items:center}
.person i{width:7px;height:7px;border-radius:50%;background:var(--faint)}.person i.on{background:var(--ok)}
.person small{color:var(--mute)}
.toggles{display:flex;flex-wrap:wrap;gap:6px}
.tog{font-size:13px;padding:6px 10px;border-radius:999px;background:var(--bg);border:1px solid var(--line);color:var(--mute);font-weight:600}
.tog.on{border-color:var(--acc);color:var(--ink);background:rgba(245,158,11,.12)}
.tog.on:disabled{opacity:1}
.tvp{display:flex;align-items:center;gap:1vw;padding:.7vh 1vw;background:var(--bg);border:1px solid var(--line);border-radius:1vh;font-size:min(1.35vw,2.3vh)}
.tvp b{flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tvp span{color:var(--mute);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:55%}
.tvp.won span{color:#4ade80}.tvp.won{border-color:rgba(74,222,128,.35)}
/* the ball */
.caller{display:flex;align-items:center;gap:14px}
.ball{--p:0;position:relative;width:92px;height:92px;flex:none;border-radius:50%;display:grid;place-items:center;
  background:conic-gradient(var(--acc) calc(var(--p)*1turn),#232a37 0)}
.ball span{width:80px;height:80px;border-radius:50%;display:grid;place-items:center;font:800 40px/1 -apple-system,system-ui,sans-serif;color:#2a1a05;
  background:radial-gradient(circle at 35% 30%,#fffbeb,#fde68a 45%,#f59e0b 85%);box-shadow:inset 0 -4px 8px rgba(0,0,0,.25),0 6px 18px rgba(245,158,11,.25)}
.ball.pop span{animation:pop .5s cubic-bezier(.2,1.6,.4,1)}
@keyframes pop{from{transform:scale(.4) rotate(-90deg);opacity:0}to{transform:none;opacity:1}}
.say{flex:1;min-width:0}
.say b{display:block;font-size:17px;line-height:1.25}
.say small{color:var(--mute);font-size:13px}
.recent{display:flex;gap:6px;margin-top:8px;flex-wrap:wrap}
.recent span{width:30px;height:30px;border-radius:50%;display:grid;place-items:center;font-size:13px;font-weight:700;background:var(--card2);color:var(--ink)}
.hostbar{display:flex;gap:6px;margin-top:12px;flex-wrap:wrap}
.hostbar button{flex:1;min-width:0}
/* tickets */
.ticket{background:var(--tk);border-radius:10px;padding:6px;margin:0 0 10px;box-shadow:0 2px 0 #d6c7a6}
.ticket .tt{display:flex;justify-content:space-between;color:#8a6d3b;font-size:11px;font-weight:700;letter-spacing:.06em;padding:0 3px 4px}
.grid{display:grid;grid-template-columns:repeat(9,1fr);gap:3px}
.cell{aspect-ratio:1/1.05;border-radius:6px;display:grid;place-items:center;font-weight:750;font-size:clamp(13px,4.2vw,19px);color:var(--tkInk);background:#fff;border:1px solid #ecdcbc;position:relative;user-select:none;cursor:pointer;font-variant-numeric:tabular-nums}
.cell.blank{background:#f3e6c9;border-color:#f3e6c9;cursor:default}
.cell.called{box-shadow:inset 0 0 0 2px #f59e0b}
.cell.mark::after{content:"";position:absolute;inset:12%;border-radius:50%;background:rgba(220,38,38,.55);mix-blend-mode:multiply}
.cell.mark{color:#7f1d1d}
.bogey .grid{opacity:.9}
/* prizes */
.prizes{display:flex;flex-direction:column;gap:6px}
.prize{display:flex;align-items:center;gap:10px;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:8px 10px}
.prize .n{flex:1;min-width:0}
.prize .n b{display:block;font-size:14px}
.prize .n small{color:var(--mute);font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;display:block}
.prize.won{opacity:.75}
.prize.won .n small{color:#4ade80}
.prize.hot{border-color:var(--acc);box-shadow:0 0 0 1px var(--acc),0 0 16px rgba(245,158,11,.25)}
.prize button{padding:7px 14px;font-size:13px}
.prize .x{font-size:12px;color:#f87171;font-weight:700}
/* the 1-90 board */
.board{display:grid;grid-template-columns:repeat(10,1fr);gap:3px}
.board span{aspect-ratio:1;border-radius:6px;display:grid;place-items:center;font-size:12px;font-weight:650;background:var(--bg);color:var(--faint);font-variant-numeric:tabular-nums}
.board span.on{background:#3b2a12;color:#fde68a}
.board span.last{background:var(--acc);color:#1a1205}
details summary{cursor:pointer;list-style:none;color:var(--mute);font-size:13px;font-weight:600}
details summary::-webkit-details-marker{display:none}
.emojis{display:flex;justify-content:space-between;gap:4px}
.emojis button{background:var(--bg);border-color:var(--line);font-size:20px;padding:6px 0;flex:1}
.fly{position:fixed;font-size:34px;pointer-events:none;z-index:20;animation:fly 2.4s ease-out forwards}
@keyframes fly{from{transform:translateY(0) scale(.6);opacity:1}to{transform:translateY(-55vh) scale(1.3);opacity:0}}
.board2{display:flex;flex-direction:column;gap:6px}
.lead{display:flex;align-items:center;gap:10px;padding:8px 10px;background:var(--bg);border:1px solid var(--line);border-radius:10px}
.lead .pos{width:24px;text-align:center;font-weight:800;color:var(--mute)}
.lead .nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.lead b{font-variant-numeric:tabular-nums}
.lead.top{border-color:var(--acc)}
.toast{position:fixed;left:50%;top:calc(14px + env(safe-area-inset-top));transform:translate(-50%,-30px);opacity:0;background:#0b1220;border:1px solid var(--line);
  padding:11px 18px;border-radius:12px;font-size:15px;font-weight:600;transition:.25s;z-index:30;pointer-events:none;max-width:92vw;text-align:center}
.toast.show{opacity:1;transform:translate(-50%,0)}
.toast.win{border-color:var(--acc);background:#2a1d05}
.toast.bad{border-color:#ef4444;background:#2a0d0d}
.shake{animation:shake .5s}
@keyframes shake{20%,60%{transform:translateX(-6px)}40%,80%{transform:translateX(6px)}}
canvas#confetti{position:fixed;inset:0;pointer-events:none;z-index:25}
.note{color:var(--mute);font-size:13px;margin-top:8px}
.foot{text-align:center;color:var(--faint);font-size:12px;margin-top:12px}
/* the big screen */
body.tv{padding:2vh 2vw}
body.tv .wrap{max-width:none}
.tvgrid{display:grid;grid-template-columns:minmax(300px,34vw) 1fr;gap:2vw;height:96vh}
.tvl{display:flex;flex-direction:column;gap:2vh;justify-content:center}
.tvball{--p:0;width:min(30vw,44vh);aspect-ratio:1;border-radius:50%;display:grid;place-items:center;margin:0 auto;background:conic-gradient(var(--acc) calc(var(--p)*1turn),#232a37 0)}
.tvball span{width:90%;height:90%;border-radius:50%;display:grid;place-items:center;font:800 min(14vw,20vh)/1 -apple-system,system-ui,sans-serif;color:#2a1a05;
  background:radial-gradient(circle at 35% 30%,#fffbeb,#fde68a 45%,#f59e0b 85%);box-shadow:inset 0 -1vh 2vh rgba(0,0,0,.25),0 2vh 5vh rgba(245,158,11,.3)}
.tvball.pop span{animation:pop .6s cubic-bezier(.2,1.6,.4,1)}
.tvsay{text-align:center;font-size:min(2.6vw,4vh);font-weight:700;min-height:1.3em}
.tvjoin{font-size:min(1.4vw,2.4vh);color:var(--mute);text-align:center}
.tvjoin b{color:var(--acc);font-family:ui-monospace,Menlo,monospace;letter-spacing:.15em;font-size:1.5em}
.tvr{display:grid;grid-template-rows:auto 1fr;gap:2vh;min-height:0}
.tvboard{display:grid;grid-template-columns:repeat(10,1fr);gap:.6vh}
.tvboard span{aspect-ratio:1.75;border-radius:1vh;display:grid;place-items:center;font-size:min(2.1vw,3.4vh);font-weight:750;background:var(--card);color:#3a4458;font-variant-numeric:tabular-nums;transition:background .4s,color .4s}
.tvboard span.on{background:#3b2a12;color:#fde68a}
.tvboard span.last{background:var(--acc);color:#1a1205;transform:scale(1.06)}
.tvbottom{display:grid;grid-template-columns:1.3fr 1fr;gap:1.5vw;min-height:0;overflow:hidden}
.tvbottom .prizes,.tvbottom .board2{gap:.6vh}
.tvbottom .prize .n small{font-size:min(1.1vw,1.9vh)}
.tvbottom .prize,.tvbottom .lead{padding:.8vh 1vw;font-size:min(1.3vw,2.2vh)}
.tvbottom .prize .n b{font-size:min(1.3vw,2.2vh)}
.voiceon{position:fixed;left:50%;bottom:2vh;transform:translateX(-50%);z-index:40}
.voiceon button{font-size:16px;padding:12px 20px;box-shadow:0 8px 30px rgba(0,0,0,.5)}
@media (max-width:700px){.tvgrid{grid-template-columns:1fr}.tvbottom{grid-template-columns:1fr}}
</style></head>
<body><div class="wrap" id="app">

<header id="hdr"><span class="logo">90</span><div><h1>Tambola</h1><p class="sub" id="sub">Housie for the whole room</p></div>
  <div class="r"><button class="ghost small" id="spk" title="Voice">🔇</button></div></header>

<!-- start -->
<section id="home" hidden>
  <div class="card">
    <p class="label">Your name</p>
    <input id="name" maxlength="14" placeholder="e.g. Keshav" autocomplete="name">
    <div class="set" style="margin:14px 0 0"><span>Tickets</span><div class="step"><button class="ghost" data-d="-1">−</button><b id="kn">1</b><button class="ghost" data-d="1">+</button><span class="sub">up to 6</span></div></div>
  </div>
  <div class="card">
    <p class="label">Join a game</p>
    <div class="row"><input id="code" maxlength="4" placeholder="CODE" autocapitalize="characters" style="text-transform:uppercase;letter-spacing:.2em;font-weight:700"><button id="joinBtn" style="flex:0 0 96px">Join</button></div>
    <div class="games" id="games"></div>
  </div>
  <button class="wide" id="newBtn" style="padding:14px">Start a new game</button>
  <p class="note" style="text-align:center">Whoever starts the game is the host: they choose the settings and press Start.</p>
</section>

<!-- lobby -->
<section id="lobby" hidden>
  <div class="card" style="text-align:center">
    <p class="label" style="justify-content:center">Game code</p>
    <div class="code" id="lcode">····</div>
    <p class="note">Everyone opens <b id="addr">tambola.local</b> and types this code.<br><a href="#" id="tvlink" style="color:var(--acc)">Show the caller on a TV or laptop ↗</a></p>
  </div>
  <div class="card">
    <p class="label">Players <span class="r" id="pcount"></span></p>
    <div class="people" id="people"></div>
  </div>
  <div class="card" id="hostSet" hidden>
    <p class="label">Settings <span class="r">host</span></p>
    <div class="set"><span>Next number</span><div class="seg" id="paceSeg"><button data-v="5">5 s</button><button data-v="8">8 s</button><button data-v="12">12 s</button><button data-v="0">I tap</button></div></div>
    <div class="set"><span>Claims</span><div class="seg" id="strictSeg"><button data-v="0">Friendly</button><button data-v="1">Strict</button></div></div>
    <div class="set"><span>Calling</span><div class="seg" id="langSeg"><button data-v="0">English calls</button><button data-v="1">Numbers</button><button data-v="2">हिंदी</button></div></div>
    <p class="label" style="margin-top:12px">Prizes</p>
    <div class="toggles" id="prizeTog"></div>
    <p class="note" id="modeNote"></p>
    <button class="wide" id="startBtn" style="margin-top:14px;padding:14px">Start the game</button>
  </div>
  <p class="card sub" id="waitNote" hidden style="text-align:center"></p>
  <div class="card">
    <p class="label">Your tickets <span class="r"><span class="step"><button class="ghost small" data-t="-1">−</button><b id="tn">1</b><button class="ghost small" data-t="1">+</button></span></span></p>
    <div id="ltix"></div>
    <button class="ghost wide small" id="reshuffle">Give me different tickets</button>
  </div>
  <button class="ghost wide" id="leaveBtn">Leave this game</button>
</section>

<!-- game -->
<section id="game" hidden>
  <div class="card">
    <div class="caller"><div class="ball" id="ball"><span id="bnum">–</span></div>
      <div class="say"><b id="phrase">Get ready…</b><small id="count"></small><div class="recent" id="recent"></div></div></div>
    <div class="hostbar" id="hostbar" hidden>
      <button class="ghost small" id="pauseBtn">Pause</button>
      <button class="small" id="nextBtn" hidden>Next number</button>
      <button class="ghost small" id="endBtn">End round</button>
    </div>
  </div>
  <div class="card">
    <p class="label">Prizes <span class="r" id="modeTag"></span></p>
    <div class="prizes" id="prizes"></div>
  </div>
  <div id="gtix"></div>
  <div class="card" id="autoCard"><div class="row" style="justify-content:space-between"><span class="sub" style="flex:1">Mark called numbers for me</span><div class="seg" id="autoSeg" style="flex:0 0 120px"><button data-v="0">Off</button><button data-v="1">On</button></div></div></div>
  <div class="card"><details id="allDet"><summary>All numbers called (<span id="ncalled">0</span>) ▾</summary><div class="board" id="board" style="margin-top:10px"></div></details></div>
  <div class="card"><div class="emojis" id="emojis"></div></div>
</section>

<!-- results -->
<section id="over" hidden>
  <div class="card" style="text-align:center"><p class="label" style="justify-content:center">Round <span id="oround"></span> over</p><h1 id="champ" style="font-size:24px"></h1><p class="sub" id="champSub"></p></div>
  <div class="card"><p class="label">Winners</p><div class="prizes" id="oprizes"></div></div>
  <div class="card"><p class="label">Leaderboard</p><div class="board2" id="olead"></div></div>
  <button class="wide" id="againBtn" style="padding:14px" hidden>New round (new tickets)</button>
  <p class="note" id="againNote" style="text-align:center"></p>
  <button class="ghost wide" id="leave2" style="margin-top:8px">Leave this game</button>
</section>

<!-- the big screen -->
<section id="tv" hidden>
  <div class="tvgrid">
    <div class="tvl">
      <div class="tvjoin">Join at <b id="tvaddr">tambola.local</b> &nbsp;code&nbsp;<b id="tvcode">····</b></div>
      <div class="tvball" id="tvball"><span id="tvnum">–</span></div>
      <div class="tvsay" id="tvsay">Waiting for the host to start…</div>
      <div class="recent" id="tvrecent" style="justify-content:center"></div>
    </div>
    <div class="tvr">
      <div class="tvboard" id="tvboard"></div>
      <div class="tvbottom"><div class="prizes" id="tvprizes"></div><div class="board2" id="tvlead"></div></div>
    </div>
  </div>
</section>
<p class="foot" id="foot"></p>
</div>
<div class="voiceon" id="voiceon" hidden><button id="voiceBtn">🔊 Tap anywhere to hear the caller</button></div>
<canvas id="confetti"></canvas>
<div class="toast" id="toast"></div>

<script>
const $=i=>document.getElementById(i);
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const KEYS=['early','top','middle','bottom','corners','full','full2'];
const PNAME={early:'Early Five',top:'Top line',middle:'Middle line',bottom:'Bottom line',corners:'Four corners',full:'Full House',full2:'Second Full House'};
const PHI={early:'अर्ली फ़ाइव',top:'ऊपर की लाइन',middle:'बीच की लाइन',bottom:'नीचे की लाइन',corners:'चारों कोने',full:'फ़ुल हाउस',full2:'दूसरा फ़ुल हाउस'};
const PHOW={early:'any 5 numbers',top:'all 5 in the top row',middle:'all 5 in the middle row',bottom:'all 5 in the bottom row',corners:'first and last of the top and bottom rows',full:'all 15 numbers',full2:'all 15, after the first Full House'};
const EMO=['👏','😂','😮','🔥','🙏','😭','🎉','😎'];
// the traditional calls, 1–90
const CALLS=[,"Kelly's eye","One little duck","Cup of tea","Knock at the door","Man alive","Half a dozen","Lucky seven","Garden gate","Doctor's orders","Big fat hen",
"Legs eleven","One dozen","Unlucky for some","Valentine's day","Young and keen","Sweet sixteen","Dancing queen","Coming of age","Goodbye teens","One score",
"Key of the door","Two little ducks","Thee and me","Two dozen","Duck and dive","Pick and mix","Gateway to heaven","Overweight","Rise and shine","Dirty thirty",
"Get up and run","Buckle my shoe","All the threes","Ask for more","Jump and jive","Three dozen","More than eleven","Christmas cake","Thirty-nine steps","Naughty forty",
"Time for fun","Winnie the Pooh","Down on your knees","Droopy drawers","Halfway there","Up to tricks","Four and seven","Four dozen","Rise and shine","Half a century",
"Tweak of the thumb","Deck of cards","Stuck in a tree","Clean the floor","Snakes alive","Was she worth it","Fifty-seven, almost heaven","Make them wait","Just in time","Five dozen",
"Baker's bun","Tickety-boo","Tickle me","Red raw","Old age pension","Clickety click","Stairway to heaven","Saving grace","Anyway up","Three score and ten",
"Bang on the drum","Six dozen","Queen bee","Hit the floor","Strive and strive","Trombones","Sunset strip","Heaven's gate","One more time","Eight and blank",
"Stop and run","Straight on through","Time for tea","Seven dozen","Staying alive","Between the sticks","Torquay in Devon","Two fat ladies","Nearly there","Top of the shop"];
const HI=[,"एक","दो","तीन","चार","पाँच","छह","सात","आठ","नौ","दस","ग्यारह","बारह","तेरह","चौदह","पंद्रह","सोलह","सत्रह","अठारह","उन्नीस","बीस",
"इक्कीस","बाईस","तेईस","चौबीस","पच्चीस","छब्बीस","सत्ताईस","अट्ठाईस","उनतीस","तीस","इकतीस","बत्तीस","तैंतीस","चौंतीस","पैंतीस","छत्तीस","सैंतीस","अड़तीस","उनतालीस","चालीस",
"इकतालीस","बयालीस","तैंतालीस","चवालीस","पैंतालीस","छियालीस","सैंतालीस","अड़तालीस","उनचास","पचास","इक्यावन","बावन","तिरपन","चौवन","पचपन","छप्पन","सत्तावन","अट्ठावन","उनसठ","साठ",
"इकसठ","बासठ","तिरसठ","चौंसठ","पैंसठ","छियासठ","सड़सठ","अड़सठ","उनहत्तर","सत्तर","इकहत्तर","बहत्तर","तिहत्तर","चौहत्तर","पचहत्तर","छिहत्तर","सतहत्तर","अठहत्तर","उन्यासी","अस्सी",
"इक्यासी","बयासी","तिरासी","चौरासी","पचासी","छियासी","सत्तासी","अट्ठासी","नवासी","नब्बे"];
const HDIG=["शून्य","एक","दो","तीन","चार","पाँच","छह","सात","आठ","नौ"];
const EN=[,"one","two","three","four","five","six","seven","eight","nine"];

let S=null,code='',tok='',tix=[],ver=0,lastEv=-1,lastCalled=-1,timer=null,tv=false,want=1,autoMark=false,marks={},prevPhase=-1;
const store={get(k,d){try{const v=localStorage.getItem('tambola.'+k);return v==null?d:JSON.parse(v)}catch(e){return d}},
  set(k,v){try{localStorage.setItem('tambola.'+k,JSON.stringify(v))}catch(e){}}};
let speakOn=store.get('speak',null);

function toast(t,kind){const e=$('toast');e.textContent=t;e.className='toast show '+(kind||'');clearTimeout(e._t);e._t=setTimeout(()=>e.className='toast',3200)}
async function req(path,data){const b=new URLSearchParams(Object.assign({c:code,t:tok},data||{}));
  const r=await fetch(path,{method:'POST',body:b});return r.json()}
function show(id){for(const s of ['home','lobby','game','over','tv'])$(s).hidden=s!==id;$('hdr').hidden=id==='tv'}
function nm(i){return S&&S.players[i]?S.players[i].n:'?'}
function hindi(){return S&&S.lang===2}
function prizeName(k){return hindi()?PHI[k]+' · '+PNAME[k]:PNAME[k]}

// ── voice
function voiceFor(lang){const vs=speechSynthesis.getVoices();
  if(lang==='hi')return vs.find(v=>/^hi/i.test(v.lang));
  return vs.find(v=>/en-IN/i.test(v.lang))||vs.find(v=>/en-GB/i.test(v.lang))||vs.find(v=>/^en/i.test(v.lang))}
let warnedHi=false;
function say(en,hi){if(!speakOn||!window.speechSynthesis)return;
  let text=en,lang='en';if(hi&&hindi()){if(voiceFor('hi')){text=hi;lang='hi'}else if(!warnedHi){warnedHi=true;toast('This device has no Hindi voice — calling in English')}}
  const u=new SpeechSynthesisUtterance(text);const v=voiceFor(lang);if(v)u.voice=v;u.lang=v?v.lang:(lang==='hi'?'hi-IN':'en-IN');u.rate=lang==='hi'?.92:.95;
  speechSynthesis.speak(u)}
function callText(n){if(!S)return'';if(S.lang===0)return CALLS[n]+', '+n;if(S.lang===1)return n<10?'Single number '+n:EN[Math.floor(n/10)]+' and '+(EN[n%10]||'zero')+', '+n;
  return(n<10?'अकेला '+HI[n]:HDIG[Math.floor(n/10)]+' '+HDIG[n%10]+', '+HI[n])}
function callSpeech(n){if(S.lang===0)return[CALLS[n]+'. Number '+n+'.',null];if(S.lang===1)return[n<10?'Single number, '+n:EN[Math.floor(n/10)]+' and '+(EN[n%10]||'zero')+'. '+n+'.',null];
  return['Number '+n,(n<10?'अकेला '+HI[n]:HDIG[Math.floor(n/10)]+' '+HDIG[n%10]+'. '+HI[n])]}
function setSpeak(v){speakOn=v;store.set('speak',v);$('spk').textContent=v?'🔊':'🔇';if(!v&&window.speechSynthesis)speechSynthesis.cancel()}
$('spk').onclick=()=>{setSpeak(!speakOn);if(speakOn)say('Voice on','आवाज़ चालू')};

// ── start screen
$('name').value=store.get('name','');
want=store.get('want',1);$('kn').textContent=want;
document.querySelectorAll('#home .step button').forEach(b=>b.onclick=()=>{want=Math.max(1,Math.min(6,want+(+b.dataset.d)));$('kn').textContent=want;store.set('want',want)});
function myName(){const n=$('name').value.trim();store.set('name',n);return n}
async function begin(r){if(!r.ok){toast({nogame:'No game with that code',started:'That game has already started — ask the host for the next round',roomfull:'That game is full (30 players)',full:'The board is running 3 games already'}[r.err]||'Couldn’t do that','bad');return}
  code=r.code;tok=r.tok;store.set('seat',{c:code,t:tok});location.hash='';ver=0;apply(r);poll()}
$('newBtn').onclick=async()=>{if(!myName()){$('name').focus();toast('Type your name first');return}if(speakOn===null)setSpeak(true);begin(await req('/create',{n:myName(),k:want}))};
$('joinBtn').onclick=async()=>{const c=$('code').value.trim().toUpperCase();if(c.length!==4){$('code').focus();return}
  if(!myName()){$('name').focus();toast('Type your name first');return}if(speakOn===null)setSpeak(false);code=c;tok='';begin(await req('/join',{n:myName(),k:want}))};
async function listGames(){if(!$('home').hidden){try{const r=await (await fetch('/rooms')).json();
  $('games').innerHTML=r.rooms.map(g=>`<button data-c="${g.c}"><span>${esc(g.host)}’s game · ${g.n} playing${g.phase===1||g.phase===2?' · in progress':''}</span><b>${g.c}</b></button>`).join('');
  }catch(e){}setTimeout(listGames,3000)}}
$('games').onclick=e=>{const b=e.target.closest('button');if(b){$('code').value=b.dataset.c;$('joinBtn').click()}};

// ── tickets
function ticketHtml(t,i,live){const called=new Set(S.called),friendly=!S.strict,mk=marks[i]||[];
  let h=`<div class="ticket${live?'':' preview'}" data-i="${i}"><div class="tt"><span>TICKET ${i+1}</span><span>${esc(nm(S.me))}</span></div><div class="grid">`;
  for(let k=0;k<27;k++){const v=t[k];if(!v){h+='<div class="cell blank"></div>';continue}
    const c=live&&friendly&&called.has(v)?' called':'',m=live&&(mk.includes(v)||(autoMark&&friendly&&called.has(v)))?' mark':'';
    h+=`<div class="cell${c}${m}" data-v="${v}">${v}</div>`}
  return h+'</div></div>'}
function drawTix(el,live){el.innerHTML=tix.map((t,i)=>ticketHtml(t,i,live)).join('')}
function marksKey(){return 'marks.'+code+'.'+(S?S.round:0)+'.'+ver}
$('gtix').onclick=e=>{const c=e.target.closest('.cell');if(!c||c.classList.contains('blank'))return;
  const i=+c.closest('.ticket').dataset.i,v=+c.dataset.v;marks[i]=marks[i]||[];
  const k=marks[i].indexOf(v);if(k>=0)marks[i].splice(k,1);else marks[i].push(v);store.set(marksKey(),marks);c.classList.toggle('mark')};

// does one of my tickets have this prize with the numbers called so far? (the board checks again)
function has(k){const called=new Set(S.called);
  return tix.some(t=>{const rows=[t.slice(0,9),t.slice(9,18),t.slice(18)].map(r=>r.filter(v=>v));const all=rows.flat();
    const n=a=>a.filter(v=>called.has(v)).length;
    if(k==='early')return n(all)>=5;if(k==='top')return n(rows[0])===5;if(k==='middle')return n(rows[1])===5;if(k==='bottom')return n(rows[2])===5;
    if(k==='corners')return n([rows[0][0],rows[0][4],rows[2][0],rows[2][4]])===4;return n(all)===15})}

// ── drawing each screen
function apply(d){
  if(d.tix){tix=d.tix;ver=d.ver;marks=store.get(marksKey(),{})}
  S=d;draw()}
function draw(){
  if(tv)return drawTv();
  if(!S){show('home');return}
  const me=S.players[S.me]||{},host=S.me===S.host;
  $('sub').textContent=(S.me>=0?me.n+' · ':'')+'game '+S.code+(S.round>1?' · round '+S.round:'');
  $('foot').textContent=S.players.length+' playing';
  if(S.phase===0){show('lobby');drawLobby(host)}
  else if(S.phase===3){show('over');drawOver(host)}
  else{show('game');drawGame(host)}
  events();prevPhase=S.phase}

function drawLobby(host){
  $('lcode').textContent=S.code;$('addr').textContent=location.host||'tambola.local';
  $('people').innerHTML=S.players.map((p,i)=>`<span class="person"><i class="${p.on?'on':''}"></i>${esc(p.n)}${i===S.host?' 👑':''}<small>${p.t}🎟</small></span>`).join('');
  $('pcount').textContent=S.players.length+' / 30';
  $('hostSet').hidden=!host;$('waitNote').hidden=host;
  $('waitNote').textContent='Waiting for '+nm(S.host)+' to start the game…';
  const sel=(id,v)=>document.querySelectorAll('#'+id+' button').forEach(b=>b.classList.toggle('sel',+b.dataset.v===v));
  sel('paceSeg',S.pace);sel('strictSeg',S.strict);sel('langSeg',S.lang);
  $('prizeTog').innerHTML=KEYS.map((k,i)=>`<button class="tog${S.prizes>>i&1?' on':''}" data-i="${i}" ${k==='full'?'disabled title="Always on"':''}>${k==='full'?'🔒 ':''}${PNAME[k]}</button>`).join('');
  $('modeNote').textContent=S.strict?'Strict: spot it yourself. A wrong claim is a Bogey, and you’re out of that prize.':'Friendly: called numbers glow on your tickets, and Claim only lights up when you really have it.';
  const mine=S.players[S.me];$('tn').textContent=mine?mine.t:1;
  drawTix($('ltix'),false)}

function ballTo(el,num,n,p){if(num.textContent!==String(n||'–')){num.textContent=n||'–';el.classList.remove('pop');void el.offsetWidth;if(n)el.classList.add('pop')}el.style.setProperty('--p',p)}
let nextT=0,nextAt=0;
function progress(){return S&&S.next>=0&&S.pace?Math.max(0,Math.min(1,1-(nextAt-performance.now())/(S.pace*1000))):0}

function drawGame(host){
  const n=S.called.length?S.called[S.called.length-1]:0;
  ballTo($('ball'),$('bnum'),n,progress());
  $('phrase').textContent=n?callText(n):(S.phase===1?'First number coming up…':'');
  $('count').textContent=S.phase===2?'⏸ Paused by the host':(n?S.called.length+' of 90 called':'');
  $('recent').innerHTML=S.called.slice(-6,-1).reverse().map(v=>`<span>${v}</span>`).join('');
  $('hostbar').hidden=!host;$('pauseBtn').textContent=S.phase===2?'Resume':'Pause';$('nextBtn').hidden=S.pace!==0||S.phase!==1;
  $('modeTag').textContent=S.strict?'strict — Bogey if wrong':'friendly';
  $('autoCard').hidden=!!S.strict||S.me<0;
  document.querySelectorAll('#autoSeg button').forEach(b=>b.classList.toggle('sel',+b.dataset.v===(autoMark?1:0)));
  const meP=S.players[S.me]||{bog:0};
  $('prizes').innerHTML=S.pz.map((z,i)=>{if(!(S.prizes>>i&1))return'';const k=z.k;
    const who=z.who.map(nm).join(' & '),mine=z.who.includes(S.me),out=meP.bog>>i&1,can=S.me>=0&&!z.won&&!out&&(k!=='full2'||S.pz[5].won)&&!(k==='full2'&&S.pz[5].who.includes(S.me));
    const hot=can&&!S.strict&&has(k);
    return `<div class="prize${z.won?' won':''}${hot?' hot':''}"><div class="n"><b>${prizeName(k)} <span class="sub">· ${z.pts} pts</span></b><small>${z.won?'🏆 '+esc(who)+(mine?' (you!)':''):PHOW[k]}</small></div>
      ${out?'<span class="x">Bogey</span>':can?`<button data-k="${k}" ${!S.strict&&!hot?'disabled':''}>Claim</button>`:''}</div>`}).join('');
  if(S.me>=0)drawTix($('gtix'),true);else $('gtix').innerHTML='<p class="card sub">You’re watching. Join the next round from the start screen.</p>';
  $('ncalled').textContent=S.called.length;
  drawBoard($('board'),'span');
  $('emojis').innerHTML=EMO.map((e,i)=>`<button data-e="${i}">${e}</button>`).join('');$('emojis').hidden=S.me<0}
function drawBoard(el){const on=new Set(S.called),last=S.called[S.called.length-1];let h='';
  for(let v=1;v<=90;v++)h+=`<span class="${v===last?'last':on.has(v)?'on':''}">${v}</span>`;el.innerHTML=h}

function leaders(){return S.players.map((p,i)=>({...p,i})).sort((a,b)=>b.pts-a.pts||b.round-a.round)}
function drawOver(host){
  $('oround').textContent=S.round;const L=leaders();const best=L[0];
  const winners=S.pz.filter((z,i)=>S.prizes>>i&1&&z.won);
  $('champ').textContent=best&&best.pts?'🏆 '+best.n:'Round over';
  $('champSub').textContent=best&&best.pts?best.pts+' points'+(S.round>1?' over '+S.round+' rounds':''):'';
  $('oprizes').innerHTML=S.pz.map((z,i)=>!(S.prizes>>i&1)?'':`<div class="prize${z.won?' won':''}"><div class="n"><b>${prizeName(z.k)}</b><small>${z.won?'🏆 '+esc(z.who.map(nm).join(' & '))+' · on number '+S.called[z.at-1]:'nobody'}</small></div></div>`).join('');
  $('olead').innerHTML=L.map((p,k)=>`<div class="lead${k===0&&p.pts?' top':''}"><span class="pos">${k+1}</span><span class="nm">${esc(p.n)}${p.i===S.me?' (you)':''}</span><span class="sub">+${p.round}</span><b>${p.pts}</b></div>`).join('');
  $('againBtn').hidden=!host;$('againNote').textContent=host?'':'Waiting for '+nm(S.host)+' to start a new round…'}

function drawTv(){
  show('tv');if(!S)return;
  $('tvcode').textContent=S.code;$('tvaddr').textContent=location.host||'tambola.local';
  const n=S.called.length?S.called[S.called.length-1]:0;
  ballTo($('tvball'),$('tvnum'),n,progress());
  $('tvsay').textContent=S.phase===0?'Waiting for '+nm(S.host)+' to start · '+S.players.length+' playing':S.phase===3?'Round over — well played!':S.phase===2?'⏸ Paused':n?callText(n):'First number coming up…';
  $('tvrecent').innerHTML=S.called.slice(-6,-1).reverse().map(v=>`<span>${v}</span>`).join('');
  drawBoard($('tvboard'));
  $('tvprizes').innerHTML=S.pz.map((z,i)=>!(S.prizes>>i&1)?'':`<div class="tvp${z.won?' won':''}"><b>${prizeName(z.k)}</b><span>${z.won?'🏆 '+esc(z.who.map(nm).join(' & ')):z.pts+' pts'}</span></div>`).join('');
  $('tvlead').innerHTML=leaders().slice(0,7).map((p,k)=>`<div class="lead${k===0&&p.pts?' top':''}"><span class="pos">${k+1}</span><span class="nm">${esc(p.n)}</span><b>${p.pts}</b></div>`).join('');
  events()}

// ── what just happened: numbers, wins, bogeys
function events(){
  if(!S)return;
  const n=S.called.length;
  if(lastCalled<0)lastCalled=n;
  if(n>lastCalled){const v=S.called[n-1];const[en,hi]=callSpeech(v);say(en,hi);lastCalled=n;
    if(autoMark&&!S.strict)drawTix($('gtix'),true)}
  if(n<lastCalled)lastCalled=n;
  const evs=S.ev||[];
  if(lastEv<0){lastEv=evs.length?evs[evs.length-1][0]:0;return}
  for(const[id,kind,p,who,num]of evs){if(id<=lastEv)continue;lastEv=id;const k=KEYS[p],name=nm(who);
    if(kind===1){const mine=who===S.me;toast((mine?'🎉 You won ':'🏆 '+name+' won ')+PNAME[k]+'!','win');say(name+' wins '+PNAME[k]+'!',name+' ने '+PHI[k]+' जीत लिया!');confetti(mine?220:90)}
    else if(kind===2){toast('Bogey! '+name+' claimed '+PNAME[k]+' too early','bad');say('Bogey! '+name,'बोगी! '+name);document.body.classList.remove('shake');void document.body.offsetWidth;document.body.classList.add('shake')}
    else if(kind===3){toast('Game on! First number coming up');say("Let's play! Eyes down.",'खेल शुरू! आँखें नीचे।')}
    else if(kind===4){say('That’s the game! Well played, everyone.','खेल ख़त्म! सबने बहुत अच्छा खेला।')}
    else if(kind===5)toast('⏸ The host paused the game');
    else if(kind===6)toast('▶ Back on!');
    else if(kind===7)toast('👑 '+name+' is the host now')}
  for(const p of S.players)if(p.e!=null&&p.ea&&p.ea!==(p._seenEa)){const key=p.n+p.ea;if(!flown.has(key)){flown.add(key);fly(EMO[p.e])}}}
const flown=new Set();
function fly(e){const s=document.createElement('div');s.className='fly';s.textContent=e;s.style.left=(10+Math.random()*80)+'vw';s.style.bottom='8vh';document.body.appendChild(s);setTimeout(()=>s.remove(),2500)}

// ── confetti
function confetti(count){const cv=$('confetti'),g=cv.getContext('2d'),W=cv.width=innerWidth,H=cv.height=innerHeight;
  const cols=['#f59e0b','#ef4444','#22c55e','#3b82f6','#a855f7','#fde68a'];
  const ps=Array.from({length:count},()=>({x:W/2+(Math.random()-.5)*W*.3,y:H*.35,vx:(Math.random()-.5)*14,vy:-Math.random()*14-4,r:Math.random()*6+4,c:cols[Math.random()*6|0],a:Math.random()*6,s:(Math.random()-.5)*.3}));
  let t=0;(function f(){g.clearRect(0,0,W,H);for(const p of ps){p.vy+=.35;p.x+=p.vx;p.y+=p.vy;p.a+=p.s;g.save();g.translate(p.x,p.y);g.rotate(p.a);g.fillStyle=p.c;g.fillRect(-p.r/2,-p.r/4,p.r,p.r/2);g.restore()}
    if(++t<150)requestAnimationFrame(f);else g.clearRect(0,0,W,H)})()}

// ── controls
$('prizes').onclick=async e=>{const b=e.target.closest('button[data-k]');if(!b)return;const k=b.dataset.k;
  if(S.strict&&!confirm('Claim '+PNAME[k]+'? If you don’t have it, it’s a Bogey and you’re out of this prize.'))return;
  b.disabled=true;const r=await req('/claim',{p:k});
  if(r.r==='notyet')toast('Not yet — you need '+PHOW[k]);else if(r.r==='taken')toast('Someone already won that one');poll(true)};
$('paceSeg').onclick=e=>{const b=e.target.closest('button');if(b)req('/settings',{pace:b.dataset.v}).then(apply)};
$('strictSeg').onclick=e=>{const b=e.target.closest('button');if(b)req('/settings',{strict:b.dataset.v}).then(apply)};
$('langSeg').onclick=e=>{const b=e.target.closest('button');if(b)req('/settings',{lang:b.dataset.v}).then(d=>{apply(d);say('This is how I will call: '+callSpeech(88)[0],callSpeech(88)[1])})};
$('prizeTog').onclick=e=>{const b=e.target.closest('button');if(!b)return;req('/settings',{prizes:S.prizes^(1<<+b.dataset.i)}).then(apply)};
$('startBtn').onclick=async()=>{if(speakOn===null)setSpeak(true);if(window.speechSynthesis)speechSynthesis.speak(new SpeechSynthesisUtterance(''));apply(await req('/start'))};
$('pauseBtn').onclick=async()=>apply(await req('/pause'));
$('nextBtn').onclick=async()=>apply(await req('/next'));
$('endBtn').onclick=async()=>{if(confirm('End this round now? Prizes not yet won stay unwon.'))apply(await req('/end'))};
$('againBtn').onclick=async()=>{ver=0;apply(await req('/again'))};
document.querySelectorAll('#lobby .step button').forEach(b=>b.onclick=async()=>{const cur=(S.players[S.me]||{t:1}).t,k=Math.max(1,Math.min(6,cur+(+b.dataset.t)));
  if(k===cur)return;store.set('want',k);ver=0;apply(await req('/tickets',{k}))});
$('reshuffle').onclick=async()=>{ver=0;apply(await req('/tickets',{k:(S.players[S.me]||{t:1}).t}));toast('New tickets')};
$('autoSeg').onclick=e=>{const b=e.target.closest('button');if(!b)return;autoMark=b.dataset.v==='1';store.set('auto',autoMark);draw()};
$('emojis').onclick=e=>{const b=e.target.closest('button');if(b){req('/emoji',{e:b.dataset.e});fly(EMO[+b.dataset.e])}};
async function leave(){if(!confirm('Leave this game?'))return;await req('/leave');store.set('seat',null);S=null;code=tok='';tix=[];location.hash='';show('home');listGames()}
$('leaveBtn').onclick=leave;$('leave2').onclick=leave;
$('tvlink').onclick=e=>{e.preventDefault();window.open('#tv='+code,'_blank')};
function voiceOn(){if($('voiceon').hidden)return;setSpeak(true);$('voiceon').hidden=true;say('The caller is ready.','कॉलर तैयार है।')}
$('voiceBtn').onclick=voiceOn;document.addEventListener('pointerdown',()=>{if(tv)voiceOn()});

// ── keeping up with the board
async function poll(now){
  clearTimeout(timer);
  try{const r=await fetch('/state?c='+encodeURIComponent(code)+'&t='+encodeURIComponent(tok)+'&v='+ver);const d=await r.json();
    if(!d.ok){if(d.err==='nogame'){store.set('seat',null);S=null;toast('That game has ended');show('home');listGames();return}}
    else{if(d.next>=0)nextAt=performance.now()+d.next;apply(d)}}
  catch(e){$('sub').textContent='Reconnecting to the board…'}
  timer=setTimeout(poll,!S?2000:S.phase===1?600:S.phase===0?1500:1200)}
setInterval(()=>{if(!S||S.phase!==1)return;const p=progress();(tv?$('tvball'):$('ball')).style.setProperty('--p',p)},100);

// ── start up: the big screen, a seat we already have, or the start screen
autoMark=store.get('auto',false);
const h=new URLSearchParams(location.hash.slice(1));
if(h.get('tv')){tv=true;code=h.get('tv').toUpperCase();document.body.classList.add('tv');show('tv');
  if(speakOn!==false)$('voiceon').hidden=false;else setSpeak(false);poll()}
else{setSpeak(!!speakOn);const seat=h.get('c')?{c:h.get('c'),t:h.get('t')}:store.get('seat',null);
  if(seat&&seat.c){code=seat.c;tok=seat.t||'';poll()}else{show('home');listGames()}}
if(window.speechSynthesis)speechSynthesis.onvoiceschanged=()=>{};
</script></body></html>)HTML";
