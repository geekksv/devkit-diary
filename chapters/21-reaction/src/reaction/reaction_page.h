#pragma once
// The page: your name, Start, the state of the round, your time, your last five and the top ten.
// The LED on the board is the real signal; the page follows a moment behind, so it never times.

static const char REACTION_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Reaction</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
:root{--bg:#f5f6f8;--card:#fff;--line:#e2e5ea;--ink:#141821;--mute:#667085;--acc:#2563eb;--acc-ink:#fff;
 --wait:#d97706;--go:#16a34a;--bad:#dc2626;--idle:#cbd2dc;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#0d1117;--card:#161b22;--line:#2a313c;--ink:#e8ecf2;--mute:#8b95a7;
 --acc:#3b82f6;--acc-ink:#fff;--wait:#f59e0b;--go:#22c55e;--bad:#f87171;--idle:#2f3744;color-scheme:dark}}
body{background:var(--bg);color:var(--ink);font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,system-ui,sans-serif;
 padding:max(16px,env(safe-area-inset-top)) 16px 28px;-webkit-font-smoothing:antialiased}
.wrap{max-width:520px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin-bottom:18px}
.mark{width:36px;height:36px;border-radius:10px;background:var(--acc);display:grid;place-items:center;flex:none}
.mark svg{width:20px;height:20px;stroke:#fff;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
h1{font-size:19px;font-weight:650}.sub{color:var(--mute);font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
.light{width:min(62vw,240px);aspect-ratio:1;border-radius:50%;margin:6px auto 14px;background:var(--idle);
 display:grid;place-items:center;text-align:center;color:#fff;font-weight:700;font-size:20px;transition:background .08s;
 box-shadow:inset 0 -8px 24px rgba(0,0,0,.15)}
.light.waiting{background:var(--wait)}.light.lit{background:var(--go)}.light.early,.light.missed{background:var(--bad)}
.light.idle{color:var(--mute)}
.time{font-size:44px;font-weight:750;letter-spacing:-.02em;text-align:center;font-variant-numeric:tabular-nums}
.note{text-align:center;color:var(--mute);min-height:22px}
.row{display:flex;gap:10px}
input{flex:1;min-width:0;font:inherit;padding:11px 12px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--ink)}
button{font:inherit;font-weight:600;padding:11px 18px;border-radius:10px;border:0;background:var(--acc);color:var(--acc-ink);cursor:pointer}
button.ghost{background:transparent;color:var(--mute);border:1px solid var(--line);padding:7px 12px;font-size:13px}
button:disabled{opacity:.5}
h2{font-size:14px;font-weight:650;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center}
ol{list-style:none}
li{display:flex;justify-content:space-between;padding:8px 0;border-top:1px solid var(--line);font-variant-numeric:tabular-nums}
li:first-child{border-top:0}li .n{color:var(--mute);width:28px}li .who{flex:1}li.me{color:var(--acc);font-weight:650}
.chips{display:flex;gap:8px;flex-wrap:wrap}.chip{padding:5px 10px;border-radius:999px;background:var(--bg);border:1px solid var(--line);font-variant-numeric:tabular-nums;font-size:14px}
.empty{color:var(--mute);font-size:14px}
</style></head><body><div class="wrap">
<header><div class="mark"><svg viewBox="0 0 24 24"><circle cx="12" cy="13" r="8"/><path d="M12 9v4l2.5 2.5M9 2h6"/></svg></div>
<div><h1>Reaction</h1><div class="sub">Wait for the blue light on the board, then press BOOT</div></div></header>

<div class="card">
 <div class="light idle" id="light">Ready</div>
 <div class="time" id="time">&nbsp;</div>
 <div class="note" id="note">Press Start, or BOOT on the board.</div>
</div>

<div class="card"><div class="row"><input id="name" maxlength="16" placeholder="Your name" autocomplete="nickname">
 <button id="start">Start</button></div></div>

<div class="card"><h2>Your last five</h2><div class="chips" id="recent"><span class="empty">No times yet.</span></div></div>

<div class="card"><h2>Top ten <button class="ghost" id="clear">Clear</button></h2><ol id="top"></ol></div>
</div>
<script>
const $=id=>document.getElementById(id);
let v=0,S=null,timer=0;
const words={idle:'Ready',waiting:'Wait',lit:'Now',done:'Done',early:'Too soon',missed:'Missed'};
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function call(path,data){
 const r=await fetch(path,data?{method:'POST',body:new URLSearchParams(data)}:{});return r.json();}
function render(){
 const p=S.phase;$('light').className='light '+p;$('light').textContent=words[p];
 $('time').textContent=p==='done'?S.ms+' ms':' ';
 $('note').textContent=p==='waiting'?'Watch the board, not the screen.':p==='lit'?'Press BOOT!':
  p==='early'?'You pressed before the light. Press BOOT to go again.':p==='missed'?'No press within 3 seconds.':
  p==='done'?(S.rank===0?'New best time!':S.rank>0?'Number '+(S.rank+1)+' in the top ten.':'Press BOOT to go again.'):
  'Press Start, or BOOT on the board.';
 $('start').disabled=p==='waiting'||p==='lit';
 if(document.activeElement!==$('name')&&!$('name').value)$('name').value=S.player==='Player'?'':S.player;
 $('recent').innerHTML=S.recent.length?S.recent.map(t=>'<span class="chip">'+t+' ms</span>').join(''):'<span class="empty">No times yet.</span>';
 $('top').innerHTML=S.top.length?S.top.map((t,i)=>'<li class="'+(i===S.rank&&p==='done'?'me':'')+'"><span class="n">'+(i+1)+'</span><span class="who">'+esc(t.n)+'</span><span>'+t.ms+' ms</span></li>').join(''):'<li class="empty">Be the first.</li>';
}
async function poll(){
 clearTimeout(timer);let again=1000;
 try{const j=await call('/state?v='+v);if(j.ok&&!j.same){S=j;v=j.v;render();}
  if(S&&(S.phase==='waiting'||S.phase==='lit'))again=150;}catch(e){again=3000;}
 timer=setTimeout(poll,again);}
$('start').onclick=async()=>{const j=await call('/start',{name:$('name').value});if(j.ok){S=j;v=j.v;render();poll();}};
$('name').onchange=()=>call('/name',{name:$('name').value}).then(j=>{if(j.ok){S=j;v=j.v;render();}});
$('clear').onclick=async()=>{if(!confirm('Clear the top ten for everyone?'))return;const j=await call('/clear',{sure:'yes'});if(j.ok){S=j;v=j.v;render();}};
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll();});
poll();
</script></body></html>)HTML";
