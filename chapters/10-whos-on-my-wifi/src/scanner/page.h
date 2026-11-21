#pragma once
// The phone page: everything on the network, a device's details and actions, the latency graph,
// messages to computers, settings and export.

static const char PAGE_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><title>Who's on my WiFi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
:root{--bg:#0d1117;--card:#161b24;--card2:#1c2230;--line:#2a3140;--ink:#eef1f6;--mute:#8b95a7;--faint:#5b6476;
  --acc:#3b82f6;--ok:#22c55e;--warn:#f59e0b;--bad:#ef4444;color-scheme:dark}
body{background:var(--bg);color:var(--ink);min-height:100vh;-webkit-font-smoothing:antialiased;
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Inter,system-ui,sans-serif;
  padding:max(14px,env(safe-area-inset-top)) 14px calc(24px + env(safe-area-inset-bottom))}
.wrap{max-width:560px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin:4px 0 16px}
.logo{width:32px;height:32px;border-radius:9px;background:linear-gradient(135deg,#3b82f6,#06b6d4);display:grid;place-items:center;font-size:17px;flex:none}
h1{font-size:20px;font-weight:700;letter-spacing:-.02em;line-height:1.2}
.sub{color:var(--mute);font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 12px}
.label{color:var(--mute);font-size:12px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;margin:0 0 10px;display:flex;align-items:center;gap:8px}
.label .r{margin-left:auto;text-transform:none;letter-spacing:0;font-weight:500}
input,select,textarea{width:100%;font:inherit;color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:11px 13px;outline:0}
textarea{resize:none;height:74px}
input:focus,select:focus,textarea:focus{border-color:var(--acc)}
button,.btn{font:inherit;font-weight:600;cursor:pointer;border:1px solid transparent;border-radius:10px;padding:10px 14px;background:var(--acc);color:#fff;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;gap:6px}
button:active,.btn:active{transform:translateY(1px)}button:disabled{opacity:.45;cursor:default}
.ghost{background:transparent;border-color:var(--line);color:var(--ink)}
.small{padding:6px 11px;font-size:13px}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:0 0 14px}
.stat{background:var(--bg);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.stat b{display:block;font-size:24px;letter-spacing:-.02em;font-variant-numeric:tabular-nums;line-height:1.2}
.stat span{color:var(--mute);font-size:12px}
.stat.new b{color:var(--warn)}
.scanrow{display:flex;gap:12px;align-items:center}
.scanrow .info{flex:1;min-width:0;font-size:13px;color:var(--mute)}
.prog{height:6px;border-radius:999px;background:#232a37;overflow:hidden;margin-top:6px}
.prog i{display:block;height:100%;width:0;background:linear-gradient(90deg,#3b82f6,#06b6d4);transition:width .5s}
.alert{border-color:rgba(245,158,11,.45);background:linear-gradient(rgba(245,158,11,.10),rgba(245,158,11,.04)),var(--card)}
.alert .label{color:var(--warn)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 12px}
.chip{font-size:13px;background:var(--bg);border:1px solid var(--line);border-radius:999px;padding:5px 10px;cursor:pointer;color:var(--ink)}
.seg{display:flex;gap:4px;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:3px}
.seg button{flex:1;background:transparent;color:var(--mute);padding:6px 8px;font-size:13px;border-radius:7px;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.seg button.sel{background:var(--card2);color:var(--ink);box-shadow:0 0 0 1px #3a4458}
#graph{display:block;width:100%;height:150px;margin:12px 0 10px}
.lat{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;text-align:center}
.lat div{font-size:12px;color:var(--mute)}.lat b{display:block;font-size:17px;color:var(--ink);font-variant-numeric:tabular-nums}
.q{font-size:12px;font-weight:700;border-radius:999px;padding:2px 9px}
.q.g{background:rgba(34,197,94,.15);color:#4ade80}.q.o{background:rgba(245,158,11,.15);color:#fbbf24}.q.b{background:rgba(239,68,68,.15);color:#f87171}
.tools{display:grid;grid-template-columns:1fr auto;gap:8px;margin:0 0 10px}
.tools select{width:auto;padding:10px}
.list{margin:0 -6px}
.dev{display:flex;align-items:center;gap:12px;padding:10px 6px;border-top:1px solid var(--line);cursor:pointer;border-radius:10px}
.dev:first-child{border-top:0}
.dev:hover{background:rgba(255,255,255,.025)}
.ic{width:40px;height:40px;border-radius:11px;background:var(--card2);display:grid;place-items:center;font-size:20px;flex:none;position:relative}
.ic .d{position:absolute;right:-2px;bottom:-2px;width:11px;height:11px;border-radius:50%;background:var(--faint);border:2px solid var(--card)}
.ic .d.on{background:var(--ok)}
.dev .t{flex:1;min-width:0}
.dev .n{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;display:flex;gap:6px;align-items:center}
.dev .n span{overflow:hidden;text-overflow:ellipsis}
.dev .s{color:var(--mute);font-size:12.5px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-variant-numeric:tabular-nums}
.dev .ms{font-size:13px;font-variant-numeric:tabular-nums;text-align:right;flex:none;min-width:48px;color:var(--mute)}
.ms.g{color:#4ade80}.ms.o{color:#fbbf24}.ms.b{color:#f87171}
.badge{font-size:10.5px;font-weight:700;letter-spacing:.03em;border-radius:5px;padding:1px 6px;flex:none}
.badge.new{background:rgba(245,158,11,.18);color:#fbbf24}
.badge.msg{background:rgba(59,130,246,.18);color:#93c5fd}
.off .n,.off .ic{opacity:.55}
.empty{color:var(--mute);font-size:14px;padding:8px 0}
.thread{max-height:300px;overflow-y:auto;display:flex;flex-direction:column;gap:8px;margin:0 0 12px}
.bub{max-width:82%;padding:8px 12px;border-radius:14px;font-size:14px;word-wrap:break-word}
.bub small{display:block;font-size:11px;color:var(--mute);margin-bottom:2px}
.bub.out{align-self:flex-end;background:#1e3a8a;border-bottom-right-radius:4px}
.bub.in{align-self:flex-start;background:var(--card2);border-bottom-left-radius:4px}
.compose{display:grid;gap:8px}
.compose .row{display:grid;grid-template-columns:1fr auto;gap:8px}
.tabs{display:flex;gap:6px;margin:12px 0 8px}
.tabs button{padding:6px 11px;font-size:13px;background:transparent;border-color:var(--line);color:var(--mute)}
.tabs button.sel{background:var(--card2);color:var(--ink);border-color:#3a4458}
.cmd{display:flex;gap:8px}
.cmd code{flex:1;min-width:0;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:10px 12px;font:13px ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;overflow-x:auto;white-space:nowrap}
.note{color:var(--mute);font-size:13px;margin-top:8px}
.set{display:flex;align-items:center;gap:12px;padding:8px 0;border-top:1px solid var(--line);font-size:14px}
.set:first-of-type{border-top:0}.set span{flex:1}
.set select{width:auto;padding:7px 10px}
.sw{position:relative;width:44px;height:26px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw i{position:absolute;inset:0;background:#2a3140;border-radius:999px;transition:.2s;cursor:pointer}
.sw i:after{content:"";position:absolute;left:3px;top:3px;width:20px;height:20px;border-radius:50%;background:#fff;transition:.2s}
.sw input:checked+i{background:var(--acc)}.sw input:checked+i:after{transform:translateX(18px)}
.foot{text-align:center;color:var(--faint);font-size:12px;margin-top:14px}
/* the device sheet */
.veil{position:fixed;inset:0;background:rgba(0,0,0,.55);opacity:0;pointer-events:none;transition:opacity .2s;z-index:5}
.veil.open{opacity:1;pointer-events:auto}
.sheet{position:fixed;left:0;right:0;bottom:0;max-height:88vh;overflow-y:auto;background:var(--card);border-top:1px solid var(--line);
  border-radius:18px 18px 0 0;padding:10px 16px calc(18px + env(safe-area-inset-bottom));transform:translateY(105%);transition:transform .25s ease;z-index:6}
.sheet.open{transform:none}
.sheet .in{max-width:560px;margin:0 auto}
.grab{width:40px;height:4px;border-radius:9px;background:#3a4458;margin:0 auto 12px}
.shead{display:flex;gap:14px;align-items:center;margin:0 0 14px}
.shead .ic{width:52px;height:52px;font-size:26px;border-radius:14px}
.shead h2{font-size:19px;letter-spacing:-.01em;line-height:1.25;word-break:break-word}
.acts{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin:0 0 14px}
.acts button{flex-direction:column;gap:3px;padding:10px 4px;font-size:12px;background:var(--card2);border-color:var(--line);color:var(--ink)}
.acts button b{font-size:19px;font-weight:400}
.kv{display:grid;grid-template-columns:110px 1fr;gap:7px 10px;font-size:13.5px;margin:0 0 14px}
.kv dt{color:var(--mute)}.kv dd{word-break:break-all;font-variant-numeric:tabular-nums}
.edit{display:grid;grid-template-columns:1fr 130px;gap:8px;margin:0 0 8px}
.ports{display:flex;flex-direction:column;gap:6px;margin:0 0 14px}
.port{display:flex;gap:10px;align-items:center;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:8px 12px;font-size:14px}
.port code{font:13px ui-monospace,Menlo,Consolas,monospace;color:var(--mute);min-width:48px}
.port span{flex:1}.port a{color:#93c5fd;font-size:13px}
.forget{display:block;margin:8px auto 0;background:none;border:0;color:var(--faint);font-weight:500;font-size:13px}
.toast{position:fixed;left:50%;bottom:calc(22px + env(safe-area-inset-bottom));transform:translate(-50%,30px);opacity:0;background:#0b1220;border:1px solid var(--line);
  padding:10px 16px;border-radius:12px;font-size:14px;transition:.25s;z-index:9;pointer-events:none;max-width:90vw}
.toast.show{opacity:1;transform:translate(-50%,0)}
.spin{display:inline-block;width:12px;height:12px;border:2px solid currentColor;border-right-color:transparent;border-radius:50%;animation:sp .8s linear infinite}
@keyframes sp{to{transform:rotate(360deg)}}
</style></head>
<body><div class="wrap">
<header><span class="logo">📡</span><div><h1>Who's on my WiFi</h1><p class="sub" id="sub">Connecting to the board…</p></div></header>

<div class="card">
  <div class="stats">
    <div class="stat"><b id="nHere">–</b><span>here now</span></div>
    <div class="stat"><b id="nKnown">–</b><span>ever seen</span></div>
    <div class="stat new"><b id="nNew">–</b><span>new</span></div>
  </div>
  <div class="scanrow">
    <div class="info"><span id="scanTxt">…</span><div class="prog"><i id="prog"></i></div></div>
    <button id="scanBtn">Scan now</button>
  </div>
</div>

<div class="card alert" id="newCard" hidden>
  <p class="label">⚠︎ New on your WiFi</p>
  <div class="chips" id="newList"></div>
  <button class="ghost small" id="allKnown">They're all mine — mark as known</button>
</div>

<div class="card">
  <p class="label">Latency <span class="r"><span class="q" id="qual"></span></span></p>
  <div class="seg" id="watchSeg">
    <button data-w="router">Router</button><button data-w="internet">Internet</button><button data-w="device" id="watchDev" hidden>Device</button>
  </div>
  <canvas id="graph"></canvas>
  <div class="lat">
    <div><b id="lNow">–</b>now</div><div><b id="lAvg">–</b>average</div><div><b id="lJit">–</b>jitter</div><div><b id="lLoss">–</b>lost</div>
  </div>
  <p class="note" id="latNote"></p>
</div>

<div class="card">
  <p class="label">Devices <span class="r" id="countTxt"></span></p>
  <div class="tools">
    <input id="q" type="search" placeholder="Search name, IP, maker, MAC" autocomplete="off">
    <select id="sort"><option value="ip">By IP</option><option value="name">By name</option><option value="ms">By speed</option><option value="seen">Recently seen</option></select>
  </div>
  <div class="seg" id="filt" style="margin:0 0 6px">
    <button data-f="here" class="sel">Here now</button><button data-f="all">All</button><button data-f="new">New</button><button data-f="msg">📨 Inbox</button>
  </div>
  <div class="list" id="list"><p class="empty">The first scan takes about 20 seconds…</p></div>
</div>

<div class="card" id="msgCard">
  <p class="label">Messages <span class="r">extra</span></p>
  <div class="thread" id="thread"></div>
  <div class="compose" id="compose" hidden>
    <select id="to"></select>
    <div class="row"><textarea id="txt" maxlength="190" placeholder="Write a message…"></textarea><button id="send">Send</button></div>
  </div>
  <p class="note" id="msgNote">Send a message to a computer on this list. It shows up on top of whatever is open there, with a box to reply. Each computer needs the small inbox helper once:</p>
  <div class="tabs"><button class="sel" data-os="sh" type="button">Mac / Linux</button><button data-os="ps" type="button">Windows</button></div>
  <div class="cmd"><code id="cmd"></code><button class="ghost" id="copy" type="button">Copy</button></div>
  <p class="note">Paste it into Terminal (on Windows, PowerShell) and run it once. Nothing to download from the internet; it uses the Python that's already there.</p>
</div>

<div class="card">
  <p class="label">Settings</p>
  <div class="set"><span>Full scan every</span><select id="every"><option>1</option><option>2</option><option>5</option><option>10</option><option>15</option><option>30</option><option>60</option></select><span style="flex:none;color:var(--mute)">min</span></div>
  <div class="set"><span>Tell my computers when something new joins</span><label class="sw"><input type="checkbox" id="alert"><i></i></label></div>
  <div class="set"><span>Every device, as a spreadsheet</span><a class="btn ghost small" href="/export.csv" download>Export CSV</a></div>
</div>
<p class="foot" id="foot"></p>
</div>

<div class="veil" id="veil"></div>
<div class="sheet" id="sheet"><div class="in" id="sheetIn"></div></div>
<div class="toast" id="toast"></div>

<script>
const $=i=>document.getElementById(i);
const ICON={router:'📶',phone:'📱',computer:'💻',tv:'📺',printer:'🖨️',speaker:'🔊',console:'🎮',camera:'📷',smart:'💡',storage:'🗄️',tablet:'📲',board:'📡',unknown:'❔'};
const KINDS=['unknown','router','phone','computer','tv','printer','speaker','console','camera','smart','storage','tablet'];
const KLABEL={unknown:'Unknown',router:'Router / network',phone:'Phone',computer:'Computer',tv:'TV / streamer',printer:'Printer',speaker:'Speaker',console:'Game console',camera:'Camera',smart:'Smart home',storage:'Storage / NAS',tablet:'Tablet',board:'This scanner'};
let S=null,devs=[],ver=0,PORTS=[],SVCS=[],filter='here',open=null,busy=false,os='sh',typing=false,lastMsgs='';
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),2600)}
async function post(u,data){const b=new URLSearchParams(data||{});const r=await fetch(u,{method:'POST',body:b});return r.json()}
function ipNum(ip){return ip.split('.').reduce((a,x)=>a*256+(+x),0)}
function title(d){return d.nk||d.n||d.md||(d.mk?d.mk+' device':d.rnd?'Private device':'Unknown device')}
function msClass(r){return r<0?'':r<30?'g':r<120?'o':'b'}
function ago(t){if(!t||!S)return'–';const s=Math.max(0,S.now-t);if(s<60)return'just now';if(s<3600)return Math.floor(s/60)+' min ago';if(s<86400)return Math.floor(s/3600)+' h ago';return Math.floor(s/86400)+' d ago'}
function when(t){if(!t)return'–';const d=new Date(t*1000);return d.toLocaleDateString(undefined,{day:'numeric',month:'short'})+', '+d.toLocaleTimeString(undefined,{hour:'2-digit',minute:'2-digit'})}
function makerLine(d){return d.mk||(d.rnd?'Private address':'Unknown maker')}

async function poll(){
  try{
    const r=await fetch('/state?v='+ver+'&t='+Math.floor(Date.now()/1000));const d=await r.json();
    S=d;if(d.devs){devs=d.devs;ver=d.v}if(d.ports)PORTS=d.ports;if(d.svcs)SVCS=d.svcs;draw();
  }catch(e){$('sub').textContent='Reconnecting to the board…'}
  setTimeout(poll,S&&(S.scan.busy||S.scan.ports)?1000:2000);
}

function draw(){
  const here=devs.filter(d=>d.on),nw=devs.filter(d=>d.nw);
  $('sub').textContent=(S.ssid||'WiFi')+' · '+here.length+' here now';
  $('nHere').textContent=here.length;$('nKnown').textContent=devs.length;$('nNew').textContent=nw.length;
  const sc=S.scan;
  $('prog').style.width=(sc.busy?sc.pct:0)+'%';
  $('scanBtn').disabled=!!sc.busy;$('scanBtn').innerHTML=sc.busy?'<span class="spin"></span> Scanning':'Scan now';
  $('scanTxt').textContent=sc.busy?'Looking at '+S.range+'… '+sc.pct+'%':(sc.ago<0?'Starting…':'Last scan '+(sc.ago<60?'just now':Math.floor(sc.ago/60)+' min ago')+' · next in '+Math.max(1,S.every-Math.floor(sc.ago/60))+' min');
  $('newCard').hidden=!nw.length;
  $('newList').innerHTML=nw.map(d=>`<span class="chip" data-m="${d.m}">${ICON[d.k]||'❔'} ${esc(title(d))} · ${esc(d.ip)}</span>`).join('');
  drawLatency();drawList();drawMsgs();
  if(!$('every').matches(':focus'))$('every').value=S.every;$('alert').checked=!!S.alert;
  $('foot').textContent='Scanning '+S.range+' · board '+S.me+' · router '+S.gw+' · signal '+S.rssi+' dBm';
  if(open)drawSheet();
}

function drawList(){
  const q=$('q').value.trim().toLowerCase(),srt=$('sort').value;
  let L=devs.filter(d=>filter==='all'||(filter==='here'&&d.on)||(filter==='new'&&d.nw)||(filter==='msg'&&d.h));
  if(q)L=L.filter(d=>[title(d),d.n,d.ip,d.mk,d.m,d.md,KLABEL[d.k]].join(' ').toLowerCase().includes(q));
  const by={ip:(a,b)=>ipNum(a.ip)-ipNum(b.ip),name:(a,b)=>title(a).localeCompare(title(b)),
    ms:(a,b)=>(a.r<0?1e9:a.r)-(b.r<0?1e9:b.r),seen:(a,b)=>b.l-a.l};
  L.sort((a,b)=>(b.on-a.on)||by[srt](a,b));
  $('countTxt').textContent=L.length+' shown';
  if(!L.length){$('list').innerHTML='<p class="empty">'+(devs.length?'Nothing matches.':'The first scan takes about 20 seconds…')+'</p>';return}
  $('list').innerHTML=L.map(d=>`<div class="dev${d.on?'':' off'}" data-m="${d.m}">
    <div class="ic">${ICON[d.k]||'❔'}<i class="d${d.on?' on':''}"></i></div>
    <div class="t"><div class="n"><span>${esc(title(d))}</span>${d.nw?'<b class="badge new">NEW</b>':''}${d.h?'<b class="badge msg">📨</b>':''}</div>
    <div class="s">${esc(d.ip)} · ${esc(makerLine(d))}${d.on?'':' · '+ago(d.l)}</div></div>
    <div class="ms ${msClass(d.r)}">${d.on?(d.r>=0?d.r+' ms':'—'):''}</div></div>`).join('');
}

function drawLatency(){
  const L=S.lat,m=L.m1;
  document.querySelectorAll('#watchSeg button').forEach(b=>b.classList.toggle('sel',b.dataset.w===L.what));
  const wd=$('watchDev');wd.hidden=L.what!=='device';
  if(L.what==='device'){const d=devs.find(x=>x.ip===L.ip);wd.textContent=d?title(d):L.ip}
  const s=L.s,last=s.length?s[s.length-1]:null;
  $('lNow').textContent=last==null?'–':last<0?'lost':last+' ms';
  $('lAvg').textContent=m.n?m.avg+' ms':'–';$('lJit').textContent=m.n?m.jit+' ms':'–';$('lLoss').textContent=m.n?m.loss+'%':'–';
  const q=$('qual');
  if(!m.n){q.textContent='';q.className='q'}
  else{const bad=m.loss>=5||m.avg>150||m.jit>60,ok=m.loss>0||m.avg>40||m.jit>20;q.className='q '+(bad?'b':ok?'o':'g');q.textContent=bad?'Laggy':ok?'OK':'Smooth'}
  $('latNote').textContent=(L.what==='router'?'Pinging your router ('+L.ip+') once a second — this is your WiFi itself.':L.what==='internet'?'Pinging 1.1.1.1 once a second — your WiFi plus your internet.':'Pinging '+L.ip+' once a second.')+' Last 2 minutes; figures for the last minute.';
  const cv=$('graph'),r=devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;
  if(cv.width!==W*r||cv.height!==H*r){cv.width=W*r;cv.height=H*r}
  const g=cv.getContext('2d');g.setTransform(r,0,0,r,0,0);g.clearRect(0,0,W,H);
  const hi=Math.max(0,...s);
  let top=Math.max(20,Math.min(hi,2000)*1.15);const step=top>1000?500:top>400?200:top>200?100:top>80?50:top>40?20:10;top=Math.ceil(top/step)*step;
  const N=120,pad=26,x=i=>pad+(W-pad-4)*(i+N-s.length)/(N-1),y=v=>H-16-(H-24)*Math.min(v,top)/top;
  g.font='10px -apple-system,system-ui,sans-serif';g.fillStyle='#5b6476';g.strokeStyle='#232a37';g.lineWidth=1;
  for(let v=0;v<=top;v+=step){g.beginPath();g.moveTo(pad,y(v)+.5);g.lineTo(W,y(v)+.5);g.stroke();g.fillText(v,0,y(v)+3)}
  g.fillText('2 min ago',pad,H-2);g.textAlign='right';g.fillText('now',W,H-2);g.textAlign='left';
  if(!s.length)return;
  const grad=g.createLinearGradient(0,0,0,H);grad.addColorStop(0,'rgba(59,130,246,.35)');grad.addColorStop(1,'rgba(59,130,246,0)');
  let seg=[];const flush=()=>{if(seg.length>1){g.beginPath();g.moveTo(seg[0][0],H-16);seg.forEach(p=>g.lineTo(p[0],p[1]));g.lineTo(seg[seg.length-1][0],H-16);g.fillStyle=grad;g.fill();
    g.beginPath();seg.forEach((p,i)=>i?g.lineTo(p[0],p[1]):g.moveTo(p[0],p[1]));g.strokeStyle='#60a5fa';g.lineWidth=1.8;g.lineJoin='round';g.stroke()}seg=[]};
  s.forEach((v,i)=>{if(v<0){flush();g.fillStyle='#ef4444';g.fillRect(x(i)-1.5,H-22,3,6)}else seg.push([x(i),y(v)])});flush();
  if(last>=0){g.fillStyle='#93c5fd';g.beginPath();g.arc(x(s.length-1),y(last),3,0,7);g.fill()}
}

function drawMsgs(){
  const hs=S.helpers||[],on=hs.filter(h=>h.on);
  $('compose').hidden=!hs.length;
  const sel=$('to'),cur=sel.value,opts=(on.length>1?[['all','Everyone ('+on.length+' computers)']]:[]).concat(hs.map(h=>[h.ip,(h.n||h.ip)+(h.on?'':' — offline, gets it when back')]));
  const html=opts.map(o=>`<option value="${esc(o[0])}">${esc(o[1])}</option>`).join('');
  if(sel._h!==html){sel.innerHTML=html;sel._h=html;if(cur&&opts.some(o=>o[0]===cur))sel.value=cur}
  const key=JSON.stringify(S.msgs);if(key===lastMsgs)return;lastMsgs=key;
  const name=ip=>{const h=hs.find(x=>x.ip===ip);return h?h.n||ip:ip};
  const th=$('thread');
  th.innerHTML=S.msgs.length?S.msgs.map(m=>m.out?`<div class="bub out"><small>${m.who==='Scanner'?'Scanner':'You'} → ${esc(name(m.ip))} · ${m.st?'delivered':'waiting'}</small>${esc(m.x)}</div>`:
    `<div class="bub in"><small>${esc(m.who)}</small>${esc(m.x)}</div>`).join(''):'';
  th.hidden=!S.msgs.length;th.scrollTop=th.scrollHeight;
  $('msgNote').hidden=hs.length>0&&S.msgs.length>0;
}

function cmdText(){const h=location.host||'scan.local';return os==='sh'?'curl -fsSL http://'+h+'/i | sh':'irm http://'+h+'/i.ps1 | iex'}

// ── one device
function openSheet(m){open=m;drawSheet();$('veil').classList.add('open');$('sheet').classList.add('open')}
function closeSheet(){open=null;$('veil').classList.remove('open');$('sheet').classList.remove('open')}
function drawSheet(){
  const d=devs.find(x=>x.m===open);if(!d){closeSheet();return}
  const nameIn=$('nk'),editing=nameIn&&document.activeElement===nameIn;
  if(editing||($('kd')&&document.activeElement===$('kd')))return;
  const checking=S.scan.ports===d.ip,ports=PORTS.filter((p,i)=>d.p>>>i&1);
  const sub=[KLABEL[d.k],makerLine(d)].join(' · ');
  const web=p=>p===80||p===8080?'http':p===443||p===8443?'https':null;
  $('sheetIn').innerHTML=`<div class="grab"></div>
  <div class="shead"><div class="ic">${ICON[d.k]||'❔'}<i class="d${d.on?' on':''}"></i></div><div><h2>${esc(title(d))}</h2><p class="sub">${esc(sub)}</p></div></div>
  ${d.nw?'<button class="ghost small" id="known" style="width:100%;margin:0 0 12px">⚠︎ New — tap if you know it, to mark it as yours</button>':''}
  <div class="acts">
    <button id="aMsg" ${d.h?'':'disabled'}><b>📨</b>Message</button>
    <button id="aWake"><b>⏰</b>Wake</button>
    <button id="aWatch" ${d.on?'':'disabled'}><b>📈</b>Latency</button>
    <button id="aPorts" ${d.on&&!checking?'':'disabled'}><b>${checking?'<span class="spin"></span>':'🚪'}</b>${checking?'Checking':'Ports'}</button>
  </div>
  <div class="edit"><input id="nk" maxlength="27" placeholder="Give it a name" value="${esc(d.nk)}"><select id="kd">${KINDS.map(k=>`<option value="${k}" ${k===d.k?'selected':''}>${ICON[k]} ${KLABEL[k]}</option>`).join('')}</select></div>
  <button class="ghost small" id="saveNk" style="width:100%;margin:0 0 14px">Save name &amp; type</button>
  ${d.pa||checking?`<p class="label">Open ports ${d.pa?'<span class="r">checked '+ago(d.pa)+'</span>':''}</p><div class="ports">${checking?'<p class="empty"><span class="spin"></span> Trying '+PORTS.length+' common ports…</p>':ports.length?ports.map(p=>`<div class="port"><code>${p[0]}</code><span>${esc(p[1])}</span>${web(p[0])?`<a href="${web(p[0])}://${d.ip}${p[0]===80||p[0]===443?'':':'+p[0]}/" target="_blank" rel="noopener">Open ↗</a>`:''}</div>`).join(''):'<p class="empty">None of the '+PORTS.length+' common ports are open.</p>'}</div>`:''}
  <dl class="kv">
    <dt>Status</dt><dd>${d.on?'Here now'+(d.r>=0?' · '+d.r+' ms':' · doesn’t answer ping'):'Away · last seen '+ago(d.l)}</dd>
    <dt>IP address</dt><dd>${esc(d.ip)}</dd>
    <dt>MAC address</dt><dd>${esc(d.m)}${d.rnd?' <span class="sub">(private, random per network)</span>':''}</dd>
    <dt>Maker</dt><dd>${esc(makerLine(d))}</dd>
    ${d.n?`<dt>Calls itself</dt><dd>${esc(d.n)} <span class="sub">via ${esc(d.src)}</span></dd>`:''}
    ${d.md&&d.md!==d.n?`<dt>Model</dt><dd>${esc(d.md)}</dd>`:''}
    ${d.sv?`<dt>Offers</dt><dd>${esc([...new Set(SVCS.filter((s,i)=>d.sv>>>i&1))].join(', '))}</dd>`:''}
    ${d.ti?`<dt>Web page</dt><dd>${esc(d.ti)}</dd>`:''}
    <dt>First seen</dt><dd>${when(d.f)}</dd>
    <dt>Last seen</dt><dd>${d.on?'now':when(d.l)}</dd>
  </dl>
  <button class="forget" id="forget">Forget this device</button>`;
  $('known')&&($('known').onclick=async()=>{await post('/seen',{m:d.m});toast('Marked as known');ver=0});
  $('aMsg').onclick=()=>{closeSheet();$('to').value=d.ip;$('txt').focus();$('msgCard').scrollIntoView({behavior:'smooth'})};
  $('aWake').onclick=async()=>{const r=await post('/wake',{m:d.m});toast(r.ok?'Wake-up sent. A PC set up for Wake-on-LAN starts in a few seconds.':'Couldn’t send it')};
  $('aWatch').onclick=async()=>{await post('/watch',{w:d.ip});toast('Latency graph now follows '+title(d));closeSheet();window.scrollTo({top:$('graph').offsetTop-120,behavior:'smooth'})};
  $('aPorts').onclick=async()=>{await post('/ports',{ip:d.ip});S.scan.ports=d.ip;drawSheet()};
  $('saveNk').onclick=async()=>{await post('/name',{m:d.m,n:$('nk').value,k:$('kd').value});$('nk').blur();toast('Saved');ver=0};
  $('forget').onclick=async()=>{if(!confirm('Forget '+title(d)+'? It will show up as new if it comes back.'))return;await post('/forget',{m:d.m});closeSheet();ver=0};
}

// ── wiring
$('scanBtn').onclick=async()=>{await post('/scan');S.scan.busy=1;S.scan.pct=0;draw()};
$('allKnown').onclick=async()=>{await post('/seen',{m:'all'});toast('All marked as known');ver=0};
$('newList').onclick=e=>{const c=e.target.closest('.chip');if(c)openSheet(c.dataset.m)};
$('list').onclick=e=>{const r=e.target.closest('.dev');if(r)openSheet(r.dataset.m)};
$('veil').onclick=closeSheet;
$('q').oninput=drawList;$('sort').onchange=drawList;
$('filt').onclick=e=>{const b=e.target.closest('button');if(!b)return;filter=b.dataset.f;document.querySelectorAll('#filt button').forEach(x=>x.classList.toggle('sel',x===b));drawList()};
$('watchSeg').onclick=async e=>{const b=e.target.closest('button');if(!b||b.dataset.w==='device')return;await post('/watch',{w:b.dataset.w});S.lat.what=b.dataset.w;S.lat.s=[];drawLatency()};
$('send').onclick=async()=>{const t=$('txt').value.trim();if(!t)return;$('send').disabled=true;
  try{const r=await post('/send',{to:$('to').value,t});if(r.ok){$('txt').value='';toast('Sent — it shows on that screen in a couple of seconds')}else toast('That computer isn’t checking in')}finally{$('send').disabled=false}};
$('txt').onkeydown=e=>{if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();$('send').click()}};
$('every').onchange=()=>post('/settings',{every:$('every').value}).then(()=>toast('A full scan every '+$('every').value+' min'));
$('alert').onchange=()=>post('/settings',{alert:$('alert').checked?1:0});
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{os=b.dataset.os;document.querySelectorAll('.tabs button').forEach(x=>x.classList.toggle('sel',x===b));$('cmd').textContent=cmdText()});
$('cmd').textContent=cmdText();
$('copy').onclick=async()=>{const t=cmdText();try{await navigator.clipboard.writeText(t)}catch(e){const a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();document.execCommand('copy');a.remove()}toast('Copied')};
document.addEventListener('keydown',e=>{if(e.key==='Escape')closeSheet()});
addEventListener('resize',()=>S&&drawLatency());
poll();
</script></body></html>)HTML";
