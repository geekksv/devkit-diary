#pragma once
// The phone page: adopt the pet, look after it, see which computer it's on and call it to another,
// and the one-line command that adds a computer.

static const char PET_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><title>Pet</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
:root{--bg:#0d1117;--card:#161b24;--line:#2a3140;--ink:#eef1f6;--mute:#8b95a7;--faint:#5b6476;--acc:#3b82f6;--ok:#22c55e;color-scheme:dark}
body{background:var(--bg);color:var(--ink);min-height:100vh;-webkit-font-smoothing:antialiased;
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Inter,system-ui,sans-serif;
  padding:max(14px,env(safe-area-inset-top)) 14px calc(20px + env(safe-area-inset-bottom))}
.wrap{max-width:460px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin:4px 0 16px}
.logo{width:30px;height:30px;border-radius:8px;background:#f0a04b;display:grid;place-items:center;font-size:17px}
h1{font-size:20px;font-weight:700;letter-spacing:-.02em;line-height:1.2}
.sub{color:var(--mute);font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 12px}
.label{color:var(--mute);font-size:12px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;margin:0 0 10px}
input{width:100%;font:inherit;color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:12px 14px;outline:0}
input:focus{border-color:var(--acc)}
button{font:inherit;font-weight:600;cursor:pointer;border:1px solid transparent;border-radius:10px;padding:11px 14px;background:var(--acc);color:#fff}
button:active{transform:translateY(1px)}button:disabled{opacity:.45;cursor:default}
button.ghost{background:transparent;border-color:var(--line);color:var(--ink)}
.kinds{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:0 0 12px}
.kind{background:var(--bg);border:1px solid var(--line);color:var(--ink);padding:12px 8px 10px;display:flex;flex-direction:column;align-items:center;gap:6px}
.kind.sel{border-color:var(--acc);box-shadow:inset 0 0 0 1px var(--acc)}
.kind canvas{display:block}
#adopt button.go{width:100%;margin-top:12px}
.stage{position:relative;height:210px;border-radius:12px;background:linear-gradient(#1b2230,#141922);overflow:hidden;margin:0 0 14px}
.stage .floor{position:absolute;left:0;right:0;bottom:0;height:26px;background:#1f2633;border-top:1px solid #2a3343}
#big{position:absolute;left:50%;bottom:6px;transform:translateX(-50%)}
.size{display:grid;grid-template-columns:auto 1fr 58px;gap:12px;align-items:center;font-size:13px;color:var(--mute);margin:0 0 14px}
.size input{padding:0;border:0;background:none;accent-color:var(--acc)}
#szv{text-align:right;color:var(--ink);font-weight:600}
.fx{position:absolute;pointer-events:none;font-size:20px;animation:up 1.6s ease-out forwards}
@keyframes up{from{transform:translateY(0);opacity:1}to{transform:translateY(-60px);opacity:0}}
.zz{position:absolute;left:62%;top:22px;color:#7d8aa6;font-weight:800;font-size:18px;animation:zz 2.4s ease-in-out infinite}
@keyframes zz{0%{transform:translate(0,10px);opacity:0}30%{opacity:1}100%{transform:translate(16px,-14px);opacity:0}}
.name{display:flex;align-items:baseline;gap:8px;justify-content:space-between}
.name b{font-size:22px;letter-spacing:-.02em}
.mood{color:var(--mute);font-size:14px;margin:2px 0 14px}
.stat{display:grid;grid-template-columns:86px 1fr 34px;align-items:center;gap:10px;margin:0 0 9px;font-size:13px;color:var(--mute)}
.stat .bar{height:8px;border-radius:999px;background:#232a37;overflow:hidden}
.stat .bar i{display:block;height:100%;border-radius:999px;transition:width .4s}
.stat span:last-child{text-align:right;font-variant-numeric:tabular-nums;color:var(--ink)}
.acts{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:14px}
.acts button{padding:11px 4px;font-size:14px}
.toast{min-height:20px;margin-top:10px;font-size:13px;color:#fbbf24;text-align:center}
.dev{display:flex;align-items:center;gap:10px;padding:10px 0;border-top:1px solid var(--line)}
.dev:first-of-type{border-top:0}
.dot{width:8px;height:8px;border-radius:50%;background:var(--faint);flex:none}.dot.on{background:var(--ok)}
.dev .n{flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev .tag{font-size:12px;color:var(--mute)}
.dev button{padding:7px 12px;font-size:13px}
.empty{color:var(--mute);font-size:14px}
.tabs{display:flex;gap:6px;margin:0 0 10px}
.tabs button{padding:7px 12px;font-size:13px;background:transparent;border-color:var(--line);color:var(--mute)}
.tabs button.sel{background:#1f2633;color:var(--ink);border-color:#3a4458}
.cmd{display:flex;gap:8px;align-items:stretch}
.cmd code{flex:1;min-width:0;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:11px 12px;
  font:13px ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;overflow-x:auto;white-space:nowrap}
.cmd button{flex:none}
.note{color:var(--mute);font-size:13px;margin-top:10px}
.note code{font:12px ui-monospace,Menlo,Consolas,monospace;color:var(--ink)}
.bye{display:block;margin:6px auto 0;background:none;border:0;color:var(--faint);font-weight:500;font-size:13px}
#adopt,#home{display:none}
</style></head>
<body><div class="wrap">
<header><span class="logo">🐾</span><div><h1>Pet</h1><p class="sub" id="sub">A pet that lives on the WiFi</p></div></header>

<div id="adopt" class="card">
  <p class="label">Adopt a pet</p>
  <div class="kinds">
    <button class="kind sel" data-k="0" type="button"><canvas id="kc0"></canvas>Cat</button>
    <button class="kind" data-k="1" type="button"><canvas id="kc1"></canvas>Dog</button>
  </div>
  <input id="nm" placeholder="Give it a name" maxlength="12" autocomplete="off">
  <button class="go" id="adoptb">Adopt</button>
  <p class="toast" id="aerr"></p>
</div>

<div id="home">
  <div class="card">
    <div class="stage" id="stage"><div class="floor"></div><canvas id="big"></canvas></div>
    <div class="name"><b id="pname"></b><span class="sub" id="where"></span></div>
    <p class="mood" id="pmood"></p>
    <div class="stat"><span>Fullness</span><div class="bar"><i id="bhun"></i></div><span id="vhun"></span></div>
    <div class="stat"><span>Happiness</span><div class="bar"><i id="bhap"></i></div><span id="vhap"></span></div>
    <div class="stat"><span>Energy</span><div class="bar"><i id="ben"></i></div><span id="ven"></span></div>
    <div class="acts">
      <button data-a="feed">Feed</button><button data-a="pet">Pet</button>
      <button data-a="play">Play</button><button data-a="sleep" id="sleepb">Sleep</button>
    </div>
    <p class="toast" id="toast"></p>
  </div>

  <div class="card">
    <p class="label">Computers</p>
    <div class="size"><span>Size on screen</span><input type="range" id="sz" min="70" max="240" step="10" value="120"><span id="szv">Medium</span></div>
    <div id="devs"></div>
  </div>

  <div class="card">
    <p class="label">Add a computer</p>
    <div class="tabs"><button class="sel" data-os="sh">Mac / Linux</button><button data-os="ps">Windows</button></div>
    <div class="cmd"><code id="cmd"></code><button class="ghost" id="copyb">Copy</button></div>
    <p class="note" id="cmdnote"></p>
    <p class="note" id="alt"></p>
  </div>
  <button class="bye" id="byeb">Say goodbye to this pet…</button>
</div>
</div>

<script>
const $=i=>document.getElementById(i);
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());
const get=u=>fetch(u,{cache:'no-store'}).then(r=>r.json());
const enc=encodeURIComponent;
const RIG={"box":[200,140],"pets":{"cat":{"walk":{"groups":{"all":[{"ty":[1.2,0.31,0]}],"head":[{"ty":[1.0,0.31,1.2]}]},"parts":[{"d":"M 52 80 C 34 80, 20 66, 20 44 C 20 36, 30 34, 31 41 C 31 58, 40 70, 54 72 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[52,78],"anim":[{"rot":[9,1.8,0]}],"fill":{"lg":[20,40,54,80,[[0.0,"#c2672a"],[1.0,"#ec9a4e"]]]}},{"d":"M 22 50 Q 27 50 30 46","stroke":"#c2672a","sw":2.4,"cap":1,"pivot":[52,78],"anim":[{"rot":[9,1.8,0]}]},{"d":"M 26 62 Q 31 61 33 56","stroke":"#c2672a","sw":2.4,"cap":1,"pivot":[52,78],"anim":[{"rot":[9,1.8,0]}]},{"d":"M 120 90 C 120 111.45, 121.68 117, 121.68 125 Q 121.68 129 126 129 Q 130.32 129 130.32 125 C 130.32 117, 132 111.45, 132 90 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[126,92],"anim":[{"rot":[22,0.62,3.141592653589793]}],"fill":"#cf7a36"},{"e":[127.5,126.4,6.52,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[126,92],"anim":[{"rot":[22,0.62,3.141592653589793]}],"fill":"#fbe3c4"},{"d":"M 66.84 96 C 64.84 106, 61.84 112, 62.84 116 C 63.84 121, 66.84 125, 67.84 126 Q 67.84 129 73 129 Q 77.16 129 77.16 125 C 76.16 118, 77.16 110, 79.16 98 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[74,86],"anim":[{"rot":[18,0.62,0]}],"fill":"#cf7a36"},{"e":[74,126.4,6.76,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[74,86],"anim":[{"rot":[18,0.62,0]}],"fill":"#fbe3c4"},{"e":[74,91,12.35,14.95],"stroke":"#8a4a1d","sw":1.4,"pivot":[74,86],"anim":[{"rot":[18,0.62,0]}],"fill":"#cf7a36"},{"d":"M 46 84 C 46 63, 72 57, 100 58 C 128 59, 150 63, 151 83 C 152 101, 135 108, 100 108 C 70 108, 46 104, 46 84 Z","stroke":"#8a4a1d","sw":1.4,"fill":{"lg":[0,58,0,108,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"d":"M 72 104 C 90 110, 118 110, 136 101 C 124 106, 90 107, 72 104 Z","fill":"#fbe3c4"},{"d":"M 82 60 Q 86 67 83 73","stroke":"#c2672a","sw":3.2,"cap":1},{"d":"M 98 59 Q 102 67 99 74","stroke":"#c2672a","sw":3.2,"cap":1},{"d":"M 114 60 Q 118 67 115 73","stroke":"#c2672a","sw":3.2,"cap":1},{"e":[141,86,12,17],"fill":"#fbe3c4"},{"d":"M 52.52 94 C 50.52 104, 47.52 112, 48.52 116 C 49.52 121, 52.52 125, 53.52 126 Q 53.52 129 59 129 Q 63.48 129 63.48 125 C 62.48 118, 63.48 110, 65.48 96 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[60,84],"anim":[{"rot":[18,0.62,3.141592653589793]}],"fill":"#ec9a4e"},{"e":[60,126.4,7.08,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[60,84],"anim":[{"rot":[18,0.62,3.141592653589793]}],"fill":"#fbe3c4"},{"e":[60,89,13.3,16.1],"stroke":"#8a4a1d","sw":1.4,"pivot":[60,84],"anim":[{"rot":[18,0.62,3.141592653589793]}],"fill":"#ec9a4e"},{"d":"M 131.75 92 C 131.75 112.35, 133.5 117, 133.5 125 Q 133.5 129 138 129 Q 142.5 129 142.5 125 C 142.5 117, 144.25 112.35, 144.25 92 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[138,94],"anim":[{"rot":[22,0.62,0]}],"fill":"#ec9a4e"},{"e":[139.5,126.4,6.7,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[138,94],"anim":[{"rot":[22,0.62,0]}],"fill":"#fbe3c4"},{"d":"M 141 48 L 139 25 L 156 39 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 163 39 L 179 24 L 180 48 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 143 45 L 142 31 L 152 40 Z","g":"head","fill":"#f3a7a2"},{"d":"M 166 40 L 176 30 L 177 45 Z","g":"head","fill":"#f3a7a2"},{"e":[160,56,24,21],"stroke":"#8a4a1d","sw":1.4,"g":"head","fill":{"lg":[160,35,160,77,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"d":"M 154 36 Q 155 43 157 46","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 160 35 L 160 45","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 166 36 Q 165 43 163 46","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"e":[153,64,9,7],"g":"head","fill":"#fbe3c4"},{"e":[167,64,9,7],"g":"head","fill":"#fbe3c4"},{"e":[151,55,5.2,5.8],"stroke":"#8a4a1d","sw":1,"g":"head","pivot":[151,55],"anim":[{"blink":[4.6,0]}],"fill":"#8db63b"},{"e":[151.4,55,1.8,4.6],"g":"head","pivot":[151,55],"anim":[{"blink":[4.6,0]}],"fill":"#1b1b1b"},{"e":[152.7,52.8,1.3,1.3],"g":"head","pivot":[151,55],"anim":[{"blink":[4.6,0]}],"fill":"#ffffff"},{"e":[169,55,5.2,5.8],"stroke":"#8a4a1d","sw":1,"g":"head","pivot":[169,55],"anim":[{"blink":[4.6,0]}],"fill":"#8db63b"},{"e":[169.4,55,1.8,4.6],"g":"head","pivot":[169,55],"anim":[{"blink":[4.6,0]}],"fill":"#1b1b1b"},{"e":[170.7,52.8,1.3,1.3],"g":"head","pivot":[169,55],"anim":[{"blink":[4.6,0]}],"fill":"#ffffff"},{"d":"M 157 60 L 163 60 L 160 63.5 Z","g":"head","fill":"#e9837f"},{"d":"M 160 63.5 Q 159 67 155 66.5 M 160 63.5 Q 161 67 165 66.5","stroke":"#8a4a1d","sw":1.2,"cap":1,"g":"head"},{"d":"M 148 63 L 130 59 M 148 66 L 130 67 M 172 63 L 190 59 M 172 66 L 190 67","stroke":"#ffffff","sw":0.9,"cap":1,"g":"head","alpha":0.85}]},"sit":{"groups":{"all":[],"body":[{"sy":[0.012,3.2,0]}],"head":[{"ty":[0.8,3.2,0.6]}]},"parts":[{"d":"M 72 129 C 58 110, 66 80, 94 68 C 116 60, 134 76, 133 98 C 133 118, 124 129, 110 129 Z","stroke":"#8a4a1d","sw":1.4,"g":"body","fill":{"lg":[0,66,0,129,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"e":[118,96,13,20],"g":"body","fill":"#fbe3c4"},{"d":"M 80 80 Q 86 86 84 93","stroke":"#c2672a","sw":3.2,"cap":1,"g":"body"},{"d":"M 76 95 Q 83 100 81 107","stroke":"#c2672a","sw":3.2,"cap":1,"g":"body"},{"e":[86,112,25,17],"stroke":"#8a4a1d","sw":1.4,"g":"body","fill":{"lg":[0,95,0,129,[[0.0,"#ec9a4e"],[1.0,"#e0853a"]]]}},{"d":"M 106.5 98 C 106.5 115.05, 108.04 117, 108.04 125 Q 108.04 129 112 129 Q 115.96 129 115.96 125 C 115.96 117, 117.5 115.05, 117.5 98 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[112,100],"fill":"#cf7a36"},{"e":[113.5,126.4,6.16,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[112,100],"fill":"#fbe3c4"},{"d":"M 118.25 98 C 118.25 115.05, 119.86 117, 119.86 125 Q 119.86 129 124 129 Q 128.14 129 128.14 125 C 128.14 117, 129.75 115.05, 129.75 98 Z","stroke":"#8a4a1d","sw":1.4,"pivot":[124,100],"fill":"#ec9a4e"},{"e":[125.5,126.4,6.34,3.5],"stroke":"#8a4a1d","sw":0.9,"pivot":[124,100],"fill":"#fbe3c4"},{"d":"M 66 124 C 86 133, 126 134, 142 125 C 147 121, 144 115, 139 118 C 126 126, 92 127, 72 118 Z","stroke":"#8a4a1d","sw":1.3,"pivot":[70,122],"anim":[{"rot":[1.5,2.6,0]}],"fill":{"lg":[66,120,146,120,[[0.0,"#ec9a4e"],[1.0,"#c2672a"]]]}},{"d":"M 128 124 Q 131 128 129 131","stroke":"#c2672a","sw":2.2,"cap":1,"pivot":[70,122],"anim":[{"rot":[1.5,2.6,0]}]},{"d":"M 99 42 L 97 19 L 114 33 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 121 33 L 137 18 L 138 42 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 101 39 L 100 25 L 110 34 Z","g":"head","fill":"#f3a7a2"},{"d":"M 124 34 L 134 24 L 135 39 Z","g":"head","fill":"#f3a7a2"},{"e":[118,50,24,21],"stroke":"#8a4a1d","sw":1.4,"g":"head","fill":{"lg":[118,29,118,71,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"d":"M 112 30 Q 113 37 115 40","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 118 29 L 118 39","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 124 30 Q 123 37 121 40","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"e":[111,58,9,7],"g":"head","fill":"#fbe3c4"},{"e":[125,58,9,7],"g":"head","fill":"#fbe3c4"},{"e":[109,49,5.2,5.8],"stroke":"#8a4a1d","sw":1,"g":"head","pivot":[109,49],"anim":[{"blink":[4.6,0]}],"fill":"#8db63b"},{"e":[109.4,49,1.8,4.6],"g":"head","pivot":[109,49],"anim":[{"blink":[4.6,0]}],"fill":"#1b1b1b"},{"e":[110.7,46.8,1.3,1.3],"g":"head","pivot":[109,49],"anim":[{"blink":[4.6,0]}],"fill":"#ffffff"},{"e":[127,49,5.2,5.8],"stroke":"#8a4a1d","sw":1,"g":"head","pivot":[127,49],"anim":[{"blink":[4.6,0]}],"fill":"#8db63b"},{"e":[127.4,49,1.8,4.6],"g":"head","pivot":[127,49],"anim":[{"blink":[4.6,0]}],"fill":"#1b1b1b"},{"e":[128.7,46.8,1.3,1.3],"g":"head","pivot":[127,49],"anim":[{"blink":[4.6,0]}],"fill":"#ffffff"},{"d":"M 115 54 L 121 54 L 118 57.5 Z","g":"head","fill":"#e9837f"},{"d":"M 118 57.5 Q 117 61 113 60.5 M 118 57.5 Q 119 61 123 60.5","stroke":"#8a4a1d","sw":1.2,"cap":1,"g":"head"},{"d":"M 106 57 L 88 53 M 106 60 L 88 61 M 130 57 L 148 53 M 130 60 L 148 61","stroke":"#ffffff","sw":0.9,"cap":1,"g":"head","alpha":0.85}]},"sleep":{"groups":{"all":[],"body":[{"sy":[0.022,3.6,0]}],"head":[{"ty":[0.9,3.6,0]}]},"parts":[{"d":"M 38 116 C 36 94, 62 84, 100 84 C 138 84, 160 96, 160 114 C 160 126, 140 130, 100 130 C 60 130, 38 128, 38 116 Z","stroke":"#8a4a1d","sw":1.4,"g":"body","fill":{"lg":[0,84,0,130,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"d":"M 70 88 Q 74 95 71 101","stroke":"#c2672a","sw":3.2,"cap":1,"g":"body"},{"d":"M 88 85 Q 92 93 89 100","stroke":"#c2672a","sw":3.2,"cap":1,"g":"body"},{"d":"M 106 86 Q 110 93 107 100","stroke":"#c2672a","sw":3.2,"cap":1,"g":"body"},{"d":"M 46 124 C 70 132, 140 134, 166 124 C 168 120, 162 117, 158 120 C 136 128, 76 127, 52 119 Z","stroke":"#8a4a1d","sw":1.2,"g":"body","fill":{"lg":[46,120,166,120,[[0.0,"#ec9a4e"],[1.0,"#c2672a"]]]}},{"e":[150,124,10,5],"stroke":"#8a4a1d","sw":1,"fill":"#fbe3c4"},{"d":"M 131 98 L 129 75 L 146 89 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 153 89 L 169 74 L 170 98 Z","stroke":"#8a4a1d","sw":1.4,"g":"head","fill":"#ec9a4e"},{"d":"M 133 95 L 132 81 L 142 90 Z","g":"head","fill":"#f3a7a2"},{"d":"M 156 90 L 166 80 L 167 95 Z","g":"head","fill":"#f3a7a2"},{"e":[150,106,24,21],"stroke":"#8a4a1d","sw":1.4,"g":"head","fill":{"lg":[150,85,150,127,[[0.0,"#e0853a"],[1.0,"#ec9a4e"]]]}},{"d":"M 144 86 Q 145 93 147 96","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 150 85 L 150 95","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"d":"M 156 86 Q 155 93 153 96","stroke":"#c2672a","sw":2.2,"cap":1,"g":"head"},{"e":[143,114,9,7],"g":"head","fill":"#fbe3c4"},{"e":[157,114,9,7],"g":"head","fill":"#fbe3c4"},{"d":"M 136 105 Q 141 109 146 105","stroke":"#8a4a1d","sw":1.8,"cap":1,"g":"head"},{"d":"M 154 105 Q 159 109 164 105","stroke":"#8a4a1d","sw":1.8,"cap":1,"g":"head"},{"d":"M 147 110 L 153 110 L 150 113.5 Z","g":"head","fill":"#e9837f"},{"d":"M 150 113.5 Q 149 117 145 116.5 M 150 113.5 Q 151 117 155 116.5","stroke":"#8a4a1d","sw":1.2,"cap":1,"g":"head"},{"d":"M 138 113 L 120 109 M 138 116 L 120 117 M 162 113 L 180 109 M 162 116 L 180 117","stroke":"#ffffff","sw":0.9,"cap":1,"g":"head","alpha":0.85}]}},"dog":{"walk":{"groups":{"all":[{"ty":[1.4,0.28,0]}],"head":[{"ty":[1.2,0.28,1.2]}]},"parts":[{"d":"M 50 74 C 40 66, 32 50, 36 36 C 38 30, 46 31, 44 38 C 42 50, 48 62, 56 68 Z","stroke":"#7a4a22","sw":1.4,"pivot":[52,72],"anim":[{"rot":[18,0.42,0]}],"fill":{"lg":[36,34,56,74,[[0.0,"#bd8143"],[1.0,"#dda25f"]]]}},{"d":"M 121 88 C 121 110.55, 122.96 117, 122.96 125 Q 122.96 129 128 129 Q 133.04 129 133.04 125 C 133.04 117, 135 110.55, 135 88 Z","stroke":"#7a4a22","sw":1.4,"pivot":[128,90],"anim":[{"rot":[24,0.56,3.141592653589793]}],"fill":"#bd8143"},{"e":[129.5,126.4,7.24,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[128,90],"anim":[{"rot":[24,0.56,3.141592653589793]}],"fill":"#f6e2c0"},{"d":"M 64.2 94 C 62.2 104, 59.2 112, 60.2 116 C 61.2 121, 64.2 125, 65.2 126 Q 65.2 129 71 129 Q 75.8 129 75.8 125 C 74.8 118, 75.8 110, 77.8 96 Z","stroke":"#7a4a22","sw":1.4,"pivot":[72,84],"anim":[{"rot":[20,0.56,0]}],"fill":"#bd8143"},{"e":[72,126.4,7.4,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[72,84],"anim":[{"rot":[20,0.56,0]}],"fill":"#f6e2c0"},{"e":[72,89,14.25,17.25],"stroke":"#7a4a22","sw":1.4,"pivot":[72,84],"anim":[{"rot":[20,0.56,0]}],"fill":"#bd8143"},{"d":"M 44 82 C 44 62, 70 56, 100 57 C 130 58, 150 62, 152 82 C 153 100, 136 106, 100 106 C 68 106, 44 102, 44 82 Z","stroke":"#7a4a22","sw":1.4,"fill":{"lg":[0,57,0,106,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"d":"M 70 102 C 90 108, 118 108, 138 99 C 124 104, 90 105, 70 102 Z","fill":"#f6e2c0"},{"e":[142,84,12,16],"fill":"#f6e2c0"},{"d":"M 49.88 92 C 47.88 102, 44.88 112, 45.88 116 C 46.88 121, 49.88 125, 50.88 126 Q 50.88 129 57 129 Q 62.12 129 62.12 125 C 61.12 118, 62.12 110, 64.12 94 Z","stroke":"#7a4a22","sw":1.4,"pivot":[58,82],"anim":[{"rot":[20,0.56,3.141592653589793]}],"fill":"#dda25f"},{"e":[58,126.4,7.72,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[58,82],"anim":[{"rot":[20,0.56,3.141592653589793]}],"fill":"#f6e2c0"},{"e":[58,87,15.2,18.4],"stroke":"#7a4a22","sw":1.4,"pivot":[58,82],"anim":[{"rot":[20,0.56,3.141592653589793]}],"fill":"#dda25f"},{"d":"M 132.75 90 C 132.75 111.45, 134.78 117, 134.78 125 Q 134.78 129 140 129 Q 145.22 129 145.22 125 C 145.22 117, 147.25 111.45, 147.25 90 Z","stroke":"#7a4a22","sw":1.4,"pivot":[140,92],"anim":[{"rot":[24,0.56,0]}],"fill":"#dda25f"},{"e":[141.5,126.4,7.42,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[140,92],"anim":[{"rot":[24,0.56,0]}],"fill":"#f6e2c0"},{"e":[160,56,22,20],"stroke":"#7a4a22","sw":1.4,"g":"head","fill":{"lg":[160,36,160,76,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"e":[162,66,14,10.5],"stroke":"#7a4a22","sw":1.2,"g":"head","fill":"#f6e2c0"},{"e":[162,60,5.8,4.2],"g":"head","fill":"#2a201c"},{"e":[163.6,58.6,1.6,1],"g":"head","alpha":0.6,"fill":"#ffffff"},{"d":"M 162 64 L 162 68 M 162 68 Q 157 72 153 69 M 162 68 Q 167 72 171 69","stroke":"#7a4a22","sw":1.3,"cap":1,"g":"head"},{"d":"M 158 70 Q 162 80 166 70 Z","stroke":"#c45567","sw":1,"g":"head","fill":"#ef7d8b"},{"e":[152,52,4.4,4.8],"g":"head","pivot":[152,52],"anim":[{"blink":[5.2,1.3]}],"fill":"#5a3a22"},{"e":[153.4,50.4,1.4,1.4],"g":"head","pivot":[152,52],"anim":[{"blink":[5.2,1.3]}],"fill":"#ffffff"},{"e":[170,52,4.4,4.8],"g":"head","pivot":[170,52],"anim":[{"blink":[5.2,1.3]}],"fill":"#5a3a22"},{"e":[171.4,50.4,1.4,1.4],"g":"head","pivot":[170,52],"anim":[{"blink":[5.2,1.3]}],"fill":"#ffffff"},{"d":"M 146 40 C 130 42, 128 64, 136 72 C 142 76, 146 62, 148 50 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[146,42],"anim":[{"rot":[5,1.4,0]}],"fill":{"lg":[130,40,146,74,[[0.0,"#9a6233"],[1.0,"#b57a45"]]]}},{"d":"M 176 40 C 192 42, 194 64, 186 72 C 180 76, 176 62, 174 50 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[176,42],"anim":[{"rot":[5,1.4,1.5]}],"fill":{"lg":[174,40,192,74,[[0.0,"#b57a45"],[1.0,"#9a6233"]]]}}]},"sit":{"groups":{"all":[],"body":[{"sy":[0.014,2.4,0]}],"head":[{"ty":[1.0,2.4,0.6]}]},"parts":[{"d":"M 60 126 C 44 122, 40 108, 50 104 C 56 101, 60 108, 56 112 C 54 117, 60 120, 68 120 Z","stroke":"#7a4a22","sw":1.3,"pivot":[64,122],"anim":[{"rot":[10,0.6,0]}],"fill":{"lg":[40,100,68,126,[[0.0,"#bd8143"],[1.0,"#dda25f"]]]}},{"d":"M 70 129 C 56 110, 64 78, 94 68 C 118 60, 136 76, 135 98 C 135 118, 126 129, 112 129 Z","stroke":"#7a4a22","sw":1.4,"g":"body","fill":{"lg":[0,66,0,129,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"e":[120,94,13,21],"g":"body","fill":"#f6e2c0"},{"e":[86,112,26,17],"stroke":"#7a4a22","sw":1.4,"g":"body","fill":{"lg":[0,95,0,129,[[0.0,"#dda25f"],[1.0,"#c98a47"]]]}},{"d":"M 106.5 96 C 106.5 114.15, 108.32 117, 108.32 125 Q 108.32 129 113 129 Q 117.68 129 117.68 125 C 117.68 117, 119.5 114.15, 119.5 96 Z","stroke":"#7a4a22","sw":1.4,"pivot":[113,98],"fill":"#bd8143"},{"e":[114.5,126.4,6.88,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[113,98],"fill":"#f6e2c0"},{"d":"M 119.25 96 C 119.25 114.15, 121.14 117, 121.14 125 Q 121.14 129 126 129 Q 130.86 129 130.86 125 C 130.86 117, 132.75 114.15, 132.75 96 Z","stroke":"#7a4a22","sw":1.4,"pivot":[126,98],"fill":"#dda25f"},{"e":[127.5,126.4,7.06,3.5],"stroke":"#7a4a22","sw":0.9,"pivot":[126,98],"fill":"#f6e2c0"},{"e":[120,50,22,20],"stroke":"#7a4a22","sw":1.4,"g":"head","fill":{"lg":[120,30,120,70,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"e":[122,60,14,10.5],"stroke":"#7a4a22","sw":1.2,"g":"head","fill":"#f6e2c0"},{"e":[122,54,5.8,4.2],"g":"head","fill":"#2a201c"},{"e":[123.6,52.6,1.6,1],"g":"head","alpha":0.6,"fill":"#ffffff"},{"d":"M 122 58 L 122 62 M 122 62 Q 117 66 113 63 M 122 62 Q 127 66 131 63","stroke":"#7a4a22","sw":1.3,"cap":1,"g":"head"},{"d":"M 118 64 Q 122 74 126 64 Z","stroke":"#c45567","sw":1,"g":"head","fill":"#ef7d8b"},{"e":[112,46,4.4,4.8],"g":"head","pivot":[112,46],"anim":[{"blink":[5.2,1.3]}],"fill":"#5a3a22"},{"e":[113.4,44.4,1.4,1.4],"g":"head","pivot":[112,46],"anim":[{"blink":[5.2,1.3]}],"fill":"#ffffff"},{"e":[130,46,4.4,4.8],"g":"head","pivot":[130,46],"anim":[{"blink":[5.2,1.3]}],"fill":"#5a3a22"},{"e":[131.4,44.4,1.4,1.4],"g":"head","pivot":[130,46],"anim":[{"blink":[5.2,1.3]}],"fill":"#ffffff"},{"d":"M 106 34 C 90 36, 88 58, 96 66 C 102 70, 106 56, 108 44 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[106,36],"anim":[{"rot":[5,1.4,0]}],"fill":{"lg":[90,34,106,68,[[0.0,"#9a6233"],[1.0,"#b57a45"]]]}},{"d":"M 136 34 C 152 36, 154 58, 146 66 C 140 70, 136 56, 134 44 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[136,36],"anim":[{"rot":[5,1.4,1.5]}],"fill":{"lg":[134,34,152,68,[[0.0,"#b57a45"],[1.0,"#9a6233"]]]}}]},"sleep":{"groups":{"all":[],"body":[{"sy":[0.022,3.6,0]}],"head":[{"ty":[0.9,3.6,0]}]},"parts":[{"d":"M 36 116 C 34 94, 62 84, 100 84 C 138 84, 160 96, 160 114 C 160 126, 140 130, 100 130 C 60 130, 36 128, 36 116 Z","stroke":"#7a4a22","sw":1.4,"g":"body","fill":{"lg":[0,84,0,130,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"d":"M 40 124 C 26 127, 16 121, 20 114 C 23 109, 30 115, 42 116 Z","stroke":"#7a4a22","sw":1.2,"pivot":[40,120],"anim":[{"rot":[3,3.6,0]}],"fill":{"lg":[16,112,42,124,[[0.0,"#bd8143"],[1.0,"#dda25f"]]]}},{"e":[146,125,14,5],"stroke":"#7a4a22","sw":1.1,"fill":"#f6e2c0"},{"e":[124,125,12,5],"stroke":"#7a4a22","sw":1.1,"fill":"#f6e2c0"},{"e":[146,104,22,20],"stroke":"#7a4a22","sw":1.4,"g":"head","fill":{"lg":[146,84,146,124,[[0.0,"#c98a47"],[1.0,"#dda25f"]]]}},{"e":[148,114,14,10.5],"stroke":"#7a4a22","sw":1.2,"g":"head","fill":"#f6e2c0"},{"e":[148,108,5.8,4.2],"g":"head","fill":"#2a201c"},{"e":[149.6,106.6,1.6,1],"g":"head","alpha":0.6,"fill":"#ffffff"},{"d":"M 148 112 L 148 116 M 148 116 Q 143 120 139 117 M 148 116 Q 153 120 157 117","stroke":"#7a4a22","sw":1.3,"cap":1,"g":"head"},{"d":"M 133 101 Q 138 105 143 101","stroke":"#7a4a22","sw":1.8,"cap":1,"g":"head"},{"d":"M 151 101 Q 156 105 161 101","stroke":"#7a4a22","sw":1.8,"cap":1,"g":"head"},{"d":"M 132 88 C 116 90, 114 112, 122 120 C 128 124, 132 110, 134 98 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[132,90],"anim":[{"rot":[5,1.4,0]}],"fill":{"lg":[116,88,132,122,[[0.0,"#9a6233"],[1.0,"#b57a45"]]]}},{"d":"M 162 88 C 178 90, 180 112, 172 120 C 166 124, 162 110, 160 98 Z","stroke":"#7a4a22","sw":1.3,"g":"head","pivot":[162,90],"anim":[{"rot":[5,1.4,1.5]}],"fill":{"lg":[160,88,178,122,[[0.0,"#b57a45"],[1.0,"#9a6233"]]]}}]}}}};
const KIND=['cat','dog'];
let last=null,kind=0,os='sh',evSeen=null,busy=false,walkUntil=0,sizing=0,szT=null;

// the pet: smooth shapes and how each part moves (the desktop helper draws the same rig with QPainter)
function rigAnim(g,list,t,pivot){
  for(const a of list||[]){
    const px=pivot?pivot[0]:0,py=pivot?pivot[1]:0;
    if(a.rot){const[amp,per,ph]=a.rot;g.translate(px,py);g.rotate(amp*Math.PI/180*Math.sin(2*Math.PI*t/per+ph));g.translate(-px,-py)}
    else if(a.ty){const[amp,per,ph]=a.ty;g.translate(0,amp*Math.sin(2*Math.PI*t/per+ph))}
    else if(a.sy){const[amp,per,ph]=a.sy;g.translate(0,130);g.scale(1,1+amp*Math.sin(2*Math.PI*t/per+ph));g.translate(0,-130)}
    else if(a.blink){const[per,off]=a.blink;if(((t+off)%per)<0.14){g.translate(px,py);g.scale(1,0.12);g.translate(-px,-py)}}
  }
}
const pathCache=new Map();
function rigPath(p){let x=pathCache.get(p);if(!x){if(p.e){x=new Path2D();x.ellipse(p.e[0],p.e[1],p.e[2],p.e[3],0,0,Math.PI*2)}else x=new Path2D(p.d);pathCache.set(p,x)}return x}
function rigFill(g,f){if(!f)return null;if(typeof f==='string')return f;
  const[x1,y1,x2,y2,st]=f.lg,gr=g.createLinearGradient(x1,y1,x2,y2);st.forEach(([o,c])=>gr.addColorStop(o,c));return gr}
function drawPet(g,kind,pose,t,facing){
  const P=RIG.pets[kind][pose];
  for(const p of P.parts){
    g.save();
    if(facing<0){g.translate(RIG.box[0],0);g.scale(-1,1)}
    rigAnim(g,P.groups.all,t,null);if(p.g)rigAnim(g,P.groups[p.g],t,null);rigAnim(g,p.anim,t,p.pivot);
    const path=rigPath(p);if(p.alpha!=null)g.globalAlpha=p.alpha;
    const f=rigFill(g,p.fill);if(f){g.fillStyle=f;g.fill(path)}
    if(p.stroke){g.strokeStyle=p.stroke;g.lineWidth=p.sw||1;g.lineCap=p.cap?'round':'butt';g.lineJoin='round';g.stroke(path)}
    g.restore();
  }
}
function fit(cv,w){const r=window.devicePixelRatio||1;cv.width=Math.round(w*r);cv.height=Math.round(w*0.7*r);
  cv.style.width=w+'px';cv.style.height=Math.round(w*0.7)+'px';cv._k=w*r/RIG.box[0]}
fit($('kc0'),150);fit($('kc1'),150);fit($('big'),270);
function paintPet(cv,k,pose,t){const g=cv.getContext('2d');g.setTransform(1,0,0,1,0,0);g.clearRect(0,0,cv.width,cv.height);
  g.scale(cv._k,cv._k);drawPet(g,k,pose,t,1)}
function loop(ms){
  const t=ms/1000;
  if($('adopt').style.display!=='none'){paintPet($('kc0'),'cat','sit',t);paintPet($('kc1'),'dog','sit',t+1.3)}
  if(last&&last.pet)paintPet($('big'),KIND[last.pet.k]||'cat',last.pet.sleep?'sleep':(performance.now()<walkUntil?'walk':'sit'),t);
  requestAnimationFrame(loop)}
requestAnimationFrame(loop);
document.querySelectorAll('.kind').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.kind').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');kind=+b.dataset.k});

const MOODS={happy:'is happy',content:'is content',hungry:'is hungry',tired:'is tired',lonely:'is lonely',sleeping:'is asleep'};
const WHY={asleep:'asleep — wake it up first',full:'not hungry right now',tired:'too tired to play',nopet:'no pet yet',offline:'that computer is offline'};

function fx(t){const s=$('stage'),e=document.createElement('div');e.className='fx';e.textContent=t;
  e.style.left=(42+Math.random()*16)+'%';e.style.bottom='90px';s.appendChild(e);setTimeout(()=>e.remove(),1700)}
function event(k){
  if(k==='feed')fx(last.pet.k===1?'🦴':'🐟');
  else if(k==='pet'||k==='poke'||k==='adopt'){fx('❤️');setTimeout(()=>fx('❤️'),250)}
  else if(k==='play'){walkUntil=performance.now()+1800;const b=$('big');b.animate([{transform:'translateX(-50%) translateY(0)'},{transform:'translateX(-50%) translateY(-28px)'},
    {transform:'translateX(-50%) translateY(0)'}],{duration:420,iterations:3})}
}
function bar(id,v){const c=v<25?'#ef4444':v<50?'#f59e0b':'#22c55e';$('b'+id).style.width=v+'%';$('b'+id).style.background=c;$('v'+id).textContent=v}

function draw(d){
  last=d;
  const p=d.pet;
  $('adopt').style.display=p?'none':'block';
  $('home').style.display=p?'block':'none';
  if(!p){$('sub').textContent='A pet that lives on the WiFi';return}
  const k=KIND[p.k]||'cat';
  if(Date.now()>sizing){$('sz').value=p.sz||120;$('szv').textContent=sizeWord(p.sz||120)}
  let z=$('stage').querySelector('.zz');
  if(p.sleep&&!z){z=document.createElement('div');z.className='zz';z.textContent='z Z';$('stage').appendChild(z)}
  if(!p.sleep&&z)z.remove();
  $('pname').textContent=p.n;
  const here=d.devs.find(x=>x.here);
  $('where').textContent=here?'on '+here.n:'waiting for a computer';
  $('pmood').textContent=p.n+' '+(MOODS[p.mood]||'is '+p.mood)+'.';
  $('sub').textContent=p.n+' the '+k;
  bar('hun',p.hun);bar('hap',p.hap);bar('en',p.en);
  const sb=$('sleepb');sb.textContent=p.sleep?'Wake':'Sleep';sb.dataset.a=p.sleep?'wake':'sleep';
  if(evSeen!==null&&d.ev!==evSeen)event(d.evk);
  evSeen=d.ev;
  const box=$('devs');box.innerHTML='';
  if(!d.devs.length){const e=document.createElement('p');e.className='empty';
    e.textContent='No computers yet. Run the command below on one, and '+p.n+' will walk onto its screen.';box.appendChild(e)}
  for(const v of d.devs){
    const row=document.createElement('div');row.className='dev';
    const dot=document.createElement('span');dot.className='dot'+(v.on?' on':'');
    const n=document.createElement('span');n.className='n';n.textContent=v.n;
    row.append(dot,n);
    if(v.here){const t=document.createElement('span');t.className='tag';t.textContent=p.n+' is here';row.appendChild(t)}
    else if(v.on){const b=document.createElement('button');b.className='ghost';b.textContent='Call here';
      b.onclick=()=>post('/call','i='+v.i).then(r=>r.ok?draw(r):say(r.err));row.appendChild(b)}
    else{const t=document.createElement('span');t.className='tag';t.textContent='offline';row.appendChild(t)}
    box.appendChild(row);
  }
  showCmd();
}

// how big the pet stands on every computer screen
const sizeWord=v=>v<100?'Small':v<160?'Medium':v<210?'Large':'Huge';
$('sz').oninput=()=>{const v=+$('sz').value;$('szv').textContent=sizeWord(v);sizing=Date.now()+2000;
  clearTimeout(szT);szT=setTimeout(()=>post('/size','v='+v).then(r=>{if(r.ok)draw(r)}).catch(()=>{}),250)};

function say(err){$('toast').textContent=last&&last.pet?last.pet.n+' is '+(WHY[err]||err):(WHY[err]||err);
  clearTimeout(say.t);say.t=setTimeout(()=>$('toast').textContent='',3500)}

document.querySelectorAll('.acts button').forEach(b=>b.onclick=async()=>{
  if(busy)return;busy=true;
  try{const r=await post('/act','a='+b.dataset.a);r.ok?draw(r):say(r.err)}catch(e){say('not answering')}
  busy=false});

$('adoptb').onclick=async()=>{const n=$('nm').value.trim();if(!n){$('aerr').textContent='Give it a name first.';return}
  try{const r=await post('/adopt','k='+kind+'&n='+enc(n));r.ok?draw(r):($('aerr').textContent='Could not adopt.')}catch(e){$('aerr').textContent='The board did not answer.'}};
$('byeb').onclick=async()=>{if(!last||!last.pet)return;
  if(!confirm('Say goodbye to '+last.pet.n+'? You can adopt a new pet afterwards.'))return;
  try{const r=await post('/release','');if(r.ok)draw(r)}catch(e){}};

// the command that adds a computer, for whichever address this page was opened on
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');os=b.dataset.os;showCmd()});
const line=(h,o)=>o==='sh'?'curl -fsSL http://'+h+'/i | sh':'irm http://'+h+'/i.ps1 | iex';
function showCmd(){
  const h=location.host||'pet.local';
  $('cmd').textContent=line(h,os);
  $('cmdnote').textContent=(os==='sh'?'Paste it into Terminal.':'Paste it into PowerShell.')+
    ' Run it once — the pet helper starts by itself from then on.';
  const ip=last&&last.board;
  $('alt').innerHTML='';
  if(ip&&ip!==h&&ip!=='0.0.0.0'){$('alt').append('If that can’t find the board, use its address: ');
    const c=document.createElement('code');c.textContent=line(ip,os);$('alt').append(c)}
}
$('copyb').onclick=()=>{
  const t=$('cmd').textContent,done=()=>{$('copyb').textContent='Copied';setTimeout(()=>$('copyb').textContent='Copy',1500)};
  if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t).then(done);return}
  const a=document.createElement('textarea');a.value=t;a.style.position='fixed';a.style.opacity='0';document.body.appendChild(a);
  a.select();try{document.execCommand('copy');done()}catch(e){}a.remove()};

async function poll(){try{const d=await get('/state');if(d.ok&&!busy)draw(d)}catch(e){$('sub').textContent='Reconnecting to the board…'}
  setTimeout(poll,1500)}
showCmd();poll();
</script>
</body></html>
)HTML";
