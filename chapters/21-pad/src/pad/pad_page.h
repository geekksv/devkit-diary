#pragma once
// The controller. Two modes: Play (press the widgets, they drive the PC) and Edit (design your own
// layout — add, move, resize and map widgets; save and share). The phone turns each touch into the
// tiny intents pad.h understands and sends them over a WebSocket to the board.

static const char PAD_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover,user-scalable=no">
<title>Phone Pad</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}
:root{--bg:#0a0d13;--card:#151a24;--card2:#1b2130;--line:#283040;--ink:#eef1f7;--mute:#8a93a6;--acc:#6366f1;--acc2:#818cf8;--ok:#22c55e;--bad:#f87171;--warn:#f59e0b;color-scheme:dark}
html,body{height:100%;overflow:hidden;overscroll-behavior:none}
body{background:var(--bg);color:var(--ink);font:14px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,system-ui,sans-serif;-webkit-font-smoothing:antialiased;touch-action:none}
.bar{position:fixed;top:0;left:0;right:0;height:46px;display:flex;align-items:center;gap:8px;padding:0 10px;background:#0e121b;border-bottom:1px solid var(--line);z-index:20}
.logo{width:26px;height:26px;border-radius:8px;background:linear-gradient(135deg,var(--acc),var(--acc2));display:grid;place-items:center;font-weight:800;font-size:13px;flex:none}
.bar .name{font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:36vw}
.bar .sp{flex:1}
.chip{display:inline-flex;align-items:center;gap:6px;padding:5px 9px;border-radius:999px;font-size:11.5px;font-weight:600;background:#1c2230;color:var(--mute)}
.chip.ok{color:var(--ok)}.chip.bad{color:var(--bad)}.chip .d{width:7px;height:7px;border-radius:50%;background:currentColor}
button{font:inherit;font-weight:600;border:0;border-radius:9px;background:var(--acc);color:#fff;padding:7px 12px;cursor:pointer}
button.g{background:var(--card2);color:var(--ink);border:1px solid var(--line)}
button.sm{padding:5px 9px;font-size:12px}
.stage{position:fixed;top:46px;left:0;right:0;bottom:0;overflow:hidden}
.grid{position:absolute;inset:0;background-size:calc(100%/24) calc(100%/12);background-image:linear-gradient(to right,#ffffff08 1px,transparent 1px),linear-gradient(to bottom,#ffffff08 1px,transparent 1px);display:none}
body.edit .grid{display:block}
.w{position:absolute;border-radius:14px;display:grid;place-items:center;text-align:center;font-weight:700;color:var(--ink);user-select:none}
.w .lbl{pointer-events:none;font-size:13px}.w .lbl small{display:block;font-size:10px;color:var(--mute);font-weight:600}
.btnw{background:var(--card);border:1px solid var(--line);box-shadow:0 2px 0 rgba(0,0,0,.4)}
.btnw.down{background:var(--acc);box-shadow:none;transform:translateY(2px)}
.dpadw,.stickw,.trackw,.tiltw{background:var(--card2);border:1px solid var(--line)}
.stickw{border-radius:50%}
.nub{position:absolute;width:38%;height:38%;border-radius:50%;background:var(--acc);left:31%;top:31%;pointer-events:none;box-shadow:0 0 0 2px #0006}
.dpadw .seg{position:absolute;color:var(--mute);font-size:16px}
.dpadw .seg.on{color:var(--acc2)}
.trackw{color:var(--mute);font-size:12px}
body.edit .w{outline:2px dashed #6366f1aa;cursor:move}
.w .rz{position:absolute;right:-7px;bottom:-7px;width:18px;height:18px;border-radius:50%;background:var(--acc);display:none;border:2px solid #0a0d13}
body.edit .w .rz{display:block}
.w .ed{position:absolute;left:-7px;top:-7px;width:20px;height:20px;border-radius:50%;background:var(--card2);border:1px solid var(--line);display:none;align-items:center;justify-content:center;font-size:12px}
body.edit .w .ed{display:flex}
.sheet{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;align-items:flex-end;z-index:40}
.sheet.show{display:flex}
.sheet .box{background:var(--card);border-top:1px solid var(--line);border-radius:18px 18px 0 0;width:100%;max-width:560px;margin:0 auto;padding:18px 16px calc(18px + env(safe-area-inset-bottom));max-height:85vh;overflow:auto}
.sheet h3{font-size:15px;margin-bottom:12px}
.fld{margin-bottom:12px}.fld label{display:block;font-size:12px;color:var(--mute);margin-bottom:5px}
input,select{width:100%;font:inherit;padding:10px 12px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--ink)}
.rowb{display:flex;gap:8px;flex-wrap:wrap}.rowb button{flex:1}
.swatches{display:flex;gap:8px}.swatches .s{width:30px;height:30px;border-radius:50%;border:2px solid transparent}.swatches .s.sel{border-color:#fff}
.menu{position:fixed;top:50px;right:10px;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:6px;z-index:30;display:none;min-width:180px}
.menu.show{display:block}.menu button{display:block;width:100%;text-align:left;background:transparent;color:var(--ink);padding:9px 11px;border-radius:8px}.menu button:hover{background:var(--card2)}
.menu hr{border:0;border-top:1px solid var(--line);margin:4px 0}
.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--ink);color:var(--bg);padding:9px 15px;border-radius:10px;font-size:13px;font-weight:600;z-index:50;display:none}
.hintrot{position:fixed;inset:46px 0 0 0;display:none;place-items:center;text-align:center;padding:30px;color:var(--mute);background:var(--bg);z-index:15}
@media (orientation:portrait){.hintrot{display:grid}}
.addbar{position:fixed;left:0;right:0;bottom:0;background:#0e121b;border-top:1px solid var(--line);padding:10px;display:none;gap:8px;overflow-x:auto;z-index:25}
body.edit .addbar{display:flex}
.addbar button{flex:none;background:var(--card2);border:1px solid var(--line);color:var(--ink);font-size:12px;padding:8px 12px}
</style></head><body>
<div class="bar"><div class="logo">P</div><div class="name" id="layName">Gamepad</div>
 <span class="chip" id="cStatus"><span class="d"></span><span id="cText">connecting</span></span><div class="sp"></div>
 <span class="chip" id="cLat">&mdash;</span>
 <button class="g sm" id="editBtn">Edit</button><button class="sm" id="menuBtn">&#9776;</button></div>

<div class="stage" id="stage"><div class="grid"></div></div>
<div class="hintrot">Rotate your phone to landscape to play.</div>

<div class="addbar" id="addbar">
 <button data-add="btn">+ Button</button><button data-add="dpad">+ D-pad</button><button data-add="stick">+ Stick</button>
 <button data-add="track">+ Trackpad</button><button data-add="tilt">+ Tilt steer</button><button class="g" id="doneEdit">Done</button></div>

<div class="menu" id="menu">
 <button id="mNew">New layout</button><button id="mSwitch">Switch layout</button><button id="mRename">Rename</button>
 <hr><button id="mShare">Share this layout</button><button id="mImport">Import a code</button><button id="mFull">Fullscreen</button>
 <hr><button id="mDelete" style="color:var(--bad)">Delete layout</button></div>

<div class="sheet" id="sheet"><div class="box" id="sheetBox"></div></div>
<div class="toast" id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const stage=$('stage');let W=stage.clientWidth,H=stage.clientHeight;
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2000);}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
const store={get(k,d){try{const v=localStorage.getItem('pad.'+k);return v==null?d:v}catch(e){return d}},set(k,v){try{localStorage.setItem('pad.'+k,v)}catch(e){}}};

// ── keys ──
const KEYS={Space:44,Enter:40,Esc:41,Tab:43,Up:82,Down:81,Left:80,Right:79};
'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('').forEach((c,i)=>KEYS[c]=4+i);
'1234567890'.split('').forEach((c,i)=>KEYS[c]=30+i);
const MODS={Shift:2,Ctrl:1,Alt:4};
const KEYLIST=['W','A','S','D','Up','Down','Left','Right','Space','Enter','Esc','Tab','Shift','Ctrl','Alt','Q','E','R','F','1','2','3','4'];
function keyIntent(name,down){if(MODS[name]!=null)return 'm '+MODS[name]+' '+(down?1:0);const u=KEYS[name];return u?'k '+u+' '+(down?1:0):'';}
const GPNAMES=['A','B','X','Y','L1','R1','L2','R2','Back','Start','L3','R3','Up','Down','Left','Right'];

// ── WebSocket to the board ──
let ws,wsOk=false,lat=0;
function connect(){try{ws=new WebSocket('ws://'+location.hostname+':'+(window.__wsport||81)+'/ws');
 ws.onopen=()=>{wsOk=true;paintStatus();};ws.onclose=()=>{wsOk=false;paintStatus();setTimeout(connect,1000);};ws.onerror=()=>{try{ws.close()}catch(e){}};}catch(e){setTimeout(connect,1500);}}
function send(s){if(wsOk&&s)try{ws.send(s);}catch(e){}}
let paired=false;
async function poll(){try{const j=await (await fetch('/status')).json();paired=j.paired;paintStatus();}catch(e){}setTimeout(poll,2500);}
function paintStatus(){const c=$('cStatus');const ok=wsOk&&paired;c.className='chip '+(ok?'ok':'bad');$('cText').textContent=!wsOk?'no board':paired?'PC paired':'pair the PC';}

// ── layout model ──
// widget: {id,type,x,y,w,h,color,label, ...type fields}
//   btn: bind ('key:W'|'gp:0'|'mouse:left'), turbo
//   dpad: mode ('hat'|'wasd'|'arrows')
//   stick: side ('L'|'R') or mode 'wasd'
//   track: (mouse)   tilt: axis (0..3)
let layout=null,layoutName='Gamepad';
const COLORS=['#6366f1','#22c55e','#f59e0b','#f87171','#06b6d4','#a855f7'];
function defaultLayout(){return {name:'Gamepad',widgets:[
 {id:1,type:'dpad',x:3,y:5,w:5,h:6,mode:'wasd',color:'#6366f1',label:'Move'},
 {id:2,type:'btn',x:17,y:8,w:3.2,h:3.6,bind:'key:Space',color:'#22c55e',label:'Jump'},
 {id:3,type:'btn',x:20.5,y:6,w:3.2,h:3.6,bind:'key:Shift',color:'#f59e0b',label:'Run'},
 {id:4,type:'btn',x:17,y:4,w:3.2,h:3.6,bind:'key:E',color:'#f87171',label:'Use'},
 {id:5,type:'stick',x:9.5,y:5.5,w:5,h:6,side:'R',color:'#06b6d4',label:'Aim'}]};}
let nextId=100;

function toPx(w){return {x:w.x/24*W,y:w.y/12*H,w:w.w/24*W,h:w.h/12*H};}
function render(){
 [...stage.querySelectorAll('.w')].forEach(e=>e.remove());
 for(const wd of layout.widgets){const p=toPx(wd);const el=document.createElement('div');
  el.className='w '+wd.type+'w';el.dataset.id=wd.id;el.style.left=p.x+'px';el.style.top=p.y+'px';el.style.width=p.w+'px';el.style.height=p.h+'px';
  if(wd.color&&wd.type==='btn')el.style.borderColor=wd.color;
  let inner='<div class="lbl">'+esc(wd.label||'')+sublabel(wd)+'</div>';
  if(wd.type==='dpad')inner='<div class="seg" style="top:4px;left:50%;transform:translateX(-50%)" data-d="up">&#9650;</div><div class="seg" style="bottom:4px;left:50%;transform:translateX(-50%)" data-d="down">&#9660;</div><div class="seg" style="left:5px;top:50%;transform:translateY(-50%)" data-d="left">&#9664;</div><div class="seg" style="right:5px;top:50%;transform:translateY(-50%)" data-d="right">&#9654;</div>'+inner;
  if(wd.type==='stick')inner='<div class="nub"></div>'+inner;
  el.innerHTML=inner+'<div class="ed">&#9998;</div><div class="rz"></div>';
  stage.appendChild(el);}
 $('layName').textContent=layout.name;
}
function sublabel(wd){if(wd.type==='btn'){const b=wd.bind||'';if(b.startsWith('key:'))return '<small>'+b.slice(4)+'</small>';if(b.startsWith('gp:'))return '<small>'+GPNAMES[+b.slice(3)]+'</small>';if(b.startsWith('mouse:'))return '<small>'+b.slice(6)+' click</small>';}
 if(wd.type==='stick')return '<small>'+(wd.mode==='wasd'?'WASD':wd.side+' stick')+'</small>';
 if(wd.type==='tilt')return '<small>tilt &rarr; '+['LX','LY','RX','RY'][wd.axis||0]+'</small>';
 if(wd.type==='dpad')return '<small>'+(wd.mode||'wasd')+'</small>';return '';}

// ── PLAY: touch → intents ──
const active=new Map(); // touchId -> {wd, el, state}
function widgetAt(x,y){const e=document.elementFromPoint(x,y);const w=e&&e.closest?e.closest('.w'):null;if(!w)return null;return layout.widgets.find(k=>k.id==w.dataset.id);}
function elFor(id){return stage.querySelector('.w[data-id="'+id+'"]');}
function press(wd,x,y,on,tid){
 const el=elFor(wd.id);
 if(wd.type==='btn'){if(on){el.classList.add('down');buzz();doBtn(wd,true);}else{el.classList.remove('down');doBtn(wd,false);}}
 else if(wd.type==='dpad'){dpadMove(wd,x,y,on);}
 else if(wd.type==='stick'){stickMove(wd,x,y,on);}
 else if(wd.type==='track'){/* handled by move */}
}
function doBtn(wd,down){const b=wd.bind||'';
 if(b.startsWith('key:')){if(wd.turbo&&down){turbo(wd,b.slice(4));}else{if(wd._turbo){clearInterval(wd._turbo);wd._turbo=0;send(keyIntent(b.slice(4),false));}send(keyIntent(b.slice(4),down));}}
 else if(b.startsWith('gp:'))send('b '+(+b.slice(3))+' '+(down?1:0));
 else if(b.startsWith('mouse:')){const m={left:0,right:1,middle:2}[b.slice(6)]||0;send('c '+m+' '+(down?1:0));}}
function turbo(wd,name){let on=false;wd._turbo=setInterval(()=>{on=!on;send(keyIntent(name,on));},60);}
function dpadMove(wd,x,y,on){const el=elFor(wd.id),r=el.getBoundingClientRect();const cx=r.left+r.width/2,cy=r.top+r.height/2;
 const dx=x-cx,dy=y-cy;const dirs={up:dy<-r.height*0.12,down:dy>r.height*0.12,left:dx<-r.width*0.12,right:dx>r.width*0.12};
 if(!on)for(const k in dirs)dirs[k]=false;
 el.querySelectorAll('.seg').forEach(s=>s.classList.toggle('on',dirs[s.dataset.d]));
 const keyFor={up:'W',down:'S',left:'A',right:'D'},arr={up:'Up',down:'Down',left:'Left',right:'Right'};
 wd._d=wd._d||{};for(const d of ['up','down','left','right']){if(dirs[d]!==!!wd._d[d]){wd._d[d]=dirs[d];
  if(wd.mode==='hat'){sendHat(wd);}else send(keyIntent((wd.mode==='arrows'?arr:keyFor)[d],dirs[d]));}}}
function sendHat(wd){const d=wd._d||{};const up=d.up,dn=d.down,l=d.left,r=d.right;
 let h=8;if(up&&r)h=1;else if(dn&&r)h=3;else if(dn&&l)h=5;else if(up&&l)h=7;else if(up)h=0;else if(r)h=2;else if(dn)h=4;else if(l)h=6;send('h '+h);}
function stickMove(wd,x,y,on){const el=elFor(wd.id),r=el.getBoundingClientRect();const cx=r.left+r.width/2,cy=r.top+r.height/2;
 let dx=(x-cx)/(r.width/2),dy=(y-cy)/(r.height/2);const m=Math.hypot(dx,dy);if(m>1){dx/=m;dy/=m;}if(!on){dx=0;dy=0;}
 const nub=el.querySelector('.nub');if(nub){nub.style.left=(31+dx*31)+'%';nub.style.top=(31+dy*31)+'%';}
 if(wd.mode==='wasd'){const th=0.4;wd._s=wd._s||{};const st={W:dy<-th,S:dy>th,A:dx<-th,D:dx>th};
  for(const k in st)if(st[k]!==!!wd._s[k]){wd._s[k]=st[k];send(keyIntent(k,st[k]));}}
 else{const ax=wd.side==='R'?2:0;send('a '+ax+' '+Math.round(dx*127)+';a '+(ax+1)+' '+Math.round(dy*127));}}
function buzz(){if(navigator.vibrate)navigator.vibrate(8);}

// trackpad move (mouse) + tap click
let trackLast=null,trackMoved=0,trackStart=0;
function trackMove(wd,x,y,phase,count){
 if(phase==='start'){trackLast={x,y};trackMoved=0;trackStart=Date.now();}
 else if(phase==='move'&&trackLast){let dx=x-trackLast.x,dy=y-trackLast.y;trackLast={x,y};trackMoved+=Math.abs(dx)+Math.abs(dy);
  if(count>=2)send('s '+Math.round(-dy*0.3));else send('p '+Math.round(dx*1.6)+' '+Math.round(dy*1.6));}
 else if(phase==='end'){if(trackMoved<8&&Date.now()-trackStart<250)send('c 0 1;c 0 0');trackLast=null;}}

// ── multi-touch dispatch (play mode) ──
function onTouch(e){if(document.body.classList.contains('edit'))return;e.preventDefault();
 const seen=new Set();
 for(const t of e.changedTouches){/* handled below in full scan */}
 // full scan each event for robustness
 const now=new Map();
 for(const t of e.touches){const wd=widgetAt(t.clientX,t.clientY)||(active.get(t.identifier)||{}).wd;
  if(!wd)continue;now.set(t.identifier,{wd,t});}
 // starts & moves
 for(const [id,{wd,t}] of now){const prev=active.get(id);
  if(!prev){active.set(id,{wd,el:elFor(wd.id)});if(wd.type==='track')trackMove(wd,t.clientX,t.clientY,'start',e.touches.length);else press(wd,t.clientX,t.clientY,true,id);}
  else{if(wd.type==='track')trackMove(wd,t.clientX,t.clientY,'move',e.touches.length);else if(wd.type==='dpad'||wd.type==='stick')press(wd,t.clientX,t.clientY,true,id);}}
 // ends
 for(const [id,info] of [...active]){if(!now.has(id)){const wd=info.wd;if(wd.type==='track')trackMove(wd,0,0,'end');else press(wd,0,0,false,id);active.delete(id);}}}
['touchstart','touchmove','touchend','touchcancel'].forEach(ev=>document.addEventListener(ev,onTouch,{passive:false}));
document.addEventListener('visibilitychange',()=>{if(document.hidden){send('x');active.clear();}});

// ── tilt steering ──
let tiltOn=false;
function enableTilt(){if(tiltOn)return;const go=()=>{window.addEventListener('deviceorientation',e=>{
  const tilts=layout.widgets.filter(w=>w.type==='tilt');if(!tilts.length||document.body.classList.contains('edit'))return;
  for(const w of tilts){let v=Math.max(-40,Math.min(40,e.gamma||0))/40*127;send('a '+(w.axis||0)+' '+Math.round(v));}});tiltOn=true;};
 if(typeof DeviceOrientationEvent!=='undefined'&&DeviceOrientationEvent.requestPermission)DeviceOrientationEvent.requestPermission().then(p=>{if(p==='granted')go();}).catch(()=>{});else go();}

// ── EDIT: move / resize / add / edit ──
let drag=null;
stage.addEventListener('touchstart',e=>{if(!document.body.classList.contains('edit'))return;
 const t=e.touches[0],el=document.elementFromPoint(t.clientX,t.clientY);if(!el)return;const w=el.closest('.w');if(!w)return;
 e.preventDefault();const wd=layout.widgets.find(k=>k.id==w.dataset.id);
 if(el.classList.contains('ed')){editWidget(wd);return;}
 if(el.classList.contains('rz'))drag={wd,mode:'rz',sx:t.clientX,sy:t.clientY,w0:wd.w,h0:wd.h};
 else drag={wd,mode:'mv',sx:t.clientX,sy:t.clientY,x0:wd.x,y0:wd.y};},{passive:false});
stage.addEventListener('touchmove',e=>{if(!drag)return;e.preventDefault();const t=e.touches[0];const dx=(t.clientX-drag.sx)/W*24,dy=(t.clientY-drag.sy)/H*12;
 if(drag.mode==='mv'){drag.wd.x=Math.max(0,Math.min(24-drag.wd.w,Math.round((drag.x0+dx)*2)/2));drag.wd.y=Math.max(0,Math.min(12-drag.wd.h,Math.round((drag.y0+dy)*2)/2));}
 else{drag.wd.w=Math.max(2,Math.min(12,Math.round((drag.w0+dx)*2)/2));drag.wd.h=Math.max(2,Math.min(12,Math.round((drag.h0+dy)*2)/2));}
 render();},{passive:false});
stage.addEventListener('touchend',()=>{if(drag){drag=null;saveLocal();}});
function addWidget(type){const w={id:nextId++,type,x:10,y:4,w:type==='btn'?3.2:5,h:type==='btn'?3.6:5,color:COLORS[layout.widgets.length%6],label:type[0].toUpperCase()+type.slice(1)};
 if(type==='btn')w.bind='key:Space';if(type==='dpad')w.mode='wasd';if(type==='stick')w.side='R';if(type==='tilt')w.axis=0;
 layout.widgets.push(w);render();saveLocal();editWidget(w);}
function editWidget(wd){const b=$('sheetBox');let f='<h3>Edit '+wd.type+'</h3>';
 f+='<div class="fld"><label>Label</label><input id="fLabel" maxlength="12" value="'+esc(wd.label||'')+'"></div>';
 if(wd.type==='btn'){f+='<div class="fld"><label>Does what</label><select id="fBind">';
  for(const k of KEYLIST)f+='<option value="key:'+k+'"'+(wd.bind==='key:'+k?' selected':'')+'>Key: '+k+'</option>';
  GPNAMES.forEach((n,i)=>f+='<option value="gp:'+i+'"'+(wd.bind==='gp:'+i?' selected':'')+'>Gamepad: '+n+'</option>');
  ['left','right','middle'].forEach(m=>f+='<option value="mouse:'+m+'"'+(wd.bind==='mouse:'+m?' selected':'')+'>Mouse: '+m+' click</option>');
  f+='</select></div><div class="fld"><label><input type="checkbox" id="fTurbo" style="width:auto;margin-right:6px"'+(wd.turbo?' checked':'')+'> Turbo (auto-repeat)</label></div>';}
 if(wd.type==='dpad'){f+='<div class="fld"><label>Sends</label><select id="fMode"><option value="wasd"'+(wd.mode==='wasd'?' selected':'')+'>WASD keys</option><option value="arrows"'+(wd.mode==='arrows'?' selected':'')+'>Arrow keys</option><option value="hat"'+(wd.mode==='hat'?' selected':'')+'>Gamepad d-pad</option></select></div>';}
 if(wd.type==='stick'){f+='<div class="fld"><label>Acts as</label><select id="fMode"><option value="L"'+(wd.side==='L'&&wd.mode!=='wasd'?' selected':'')+'>Left analog stick</option><option value="R"'+(wd.side==='R'&&wd.mode!=='wasd'?' selected':'')+'>Right analog stick</option><option value="wasd"'+(wd.mode==='wasd'?' selected':'')+'>WASD keys</option></select></div>';}
 if(wd.type==='tilt'){f+='<div class="fld"><label>Tilt controls</label><select id="fAxis"><option value="0"'+(wd.axis==0?' selected':'')+'>Left stick X (steer)</option><option value="2"'+(wd.axis==2?' selected':'')+'>Right stick X (aim)</option></select></div>';}
 f+='<div class="fld"><label>Colour</label><div class="swatches" id="fCol">'+COLORS.map(c=>'<div class="s'+(wd.color===c?' sel':'')+'" data-c="'+c+'" style="background:'+c+'"></div>').join('')+'</div></div>';
 f+='<div class="rowb"><button class="g" style="color:var(--bad)" id="fDel">Delete</button><button id="fSave">Save</button></div>';
 b.innerHTML=f;$('sheet').classList.add('show');
 let col=wd.color;b.querySelectorAll('.s').forEach(s=>s.onclick=()=>{col=s.dataset.c;b.querySelectorAll('.s').forEach(x=>x.classList.remove('sel'));s.classList.add('sel');});
 $('fDel').onclick=()=>{layout.widgets=layout.widgets.filter(k=>k!==wd);closeSheet();render();saveLocal();};
 $('fSave').onclick=()=>{wd.label=$('fLabel').value;wd.color=col;
  if(wd.type==='btn'){wd.bind=$('fBind').value;wd.turbo=$('fTurbo').checked;}
  if(wd.type==='dpad')wd.mode=$('fMode').value;
  if(wd.type==='stick'){const m=$('fMode').value;if(m==='wasd')wd.mode='wasd';else{wd.mode=null;wd.side=m;}}
  if(wd.type==='tilt'){wd.axis=+$('fAxis').value;enableTilt();}
  closeSheet();render();saveLocal();};}
function closeSheet(){$('sheet').classList.remove('show');}
$('sheet').onclick=e=>{if(e.target===$('sheet'))closeSheet();};

// ── layouts: local + board ──
function saveLocal(){store.set('layout',JSON.stringify(layout));store.set('name',layout.name);}
function loadLayout(obj){layout=obj;layoutName=obj.name;nextId=Math.max(100,...obj.widgets.map(w=>w.id+1));render();saveLocal();if(obj.widgets.some(w=>w.type==='tilt'))enableTilt();}
async function saveToBoard(){try{await fetch('/layout',{method:'POST',body:new URLSearchParams({name:layout.name,data:JSON.stringify(layout)})});}catch(e){}}
async function boardLayouts(){try{return await (await fetch('/layouts')).json();}catch(e){return[];}}

// ── menu actions ──
$('menuBtn').onclick=()=>$('menu').classList.toggle('show');
document.addEventListener('click',e=>{if(!e.target.closest('#menu')&&e.target!==$('menuBtn'))$('menu').classList.remove('show');});
$('editBtn').onclick=()=>toggleEdit();
$('doneEdit').onclick=()=>toggleEdit();
function toggleEdit(){const on=!document.body.classList.contains('edit');document.body.classList.toggle('edit',on);$('editBtn').textContent=on?'Play':'Edit';if(!on){saveLocal();saveToBoard();}}
$('addbar').addEventListener('click',e=>{const b=e.target.closest('[data-add]');if(b)addWidget(b.dataset.add);});
$('mNew').onclick=()=>{const n=prompt('New layout name:','My layout');if(!n)return;loadLayout({name:n,widgets:[]});toast('Created — add widgets in Edit');if(!document.body.classList.contains('edit'))toggleEdit();$('menu').classList.remove('show');};
$('mRename').onclick=()=>{const n=prompt('Rename layout:',layout.name);if(n){layout.name=n;render();saveLocal();saveToBoard();}$('menu').classList.remove('show');};
$('mDelete').onclick=()=>{if(confirm('Delete "'+layout.name+'"?')){fetch('/dellayout',{method:'GET'});fetch('/dellayout?name='+encodeURIComponent(layout.name));loadLayout(defaultLayout());}$('menu').classList.remove('show');};
$('mSwitch').onclick=async()=>{$('menu').classList.remove('show');const names=await boardLayouts();if(!names.length){toast('No saved layouts yet');return;}
 const b=$('sheetBox');b.innerHTML='<h3>Switch layout</h3>'+names.map(n=>'<div class="fld"><button class="g" style="width:100%;text-align:left" data-ln="'+esc(n)+'">'+esc(n)+'</button></div>').join('');
 $('sheet').classList.add('show');b.querySelectorAll('[data-ln]').forEach(x=>x.onclick=async()=>{try{const o=await (await fetch('/layout?name='+encodeURIComponent(x.dataset.ln))).json();loadLayout(o);closeSheet();toast('Loaded '+o.name);}catch(e){toast('Could not load');}});};
$('mShare').onclick=()=>{$('menu').classList.remove('show');const code=btoa(unescape(encodeURIComponent(JSON.stringify(layout))));
 const b=$('sheetBox');b.innerHTML='<h3>Share this layout</h3><p style="color:var(--mute);font-size:13px;margin-bottom:10px">Copy this code; on another phone, Menu &rarr; Import a code.</p><textarea readonly style="width:100%;height:120px;font-size:11px;background:var(--bg);color:var(--ink);border:1px solid var(--line);border-radius:10px;padding:10px">'+code+'</textarea><div class="rowb" style="margin-top:10px"><button id="cpy">Copy</button></div>';
 $('sheet').classList.add('show');$('cpy').onclick=()=>{try{navigator.clipboard.writeText(code);toast('Copied');}catch(e){}};};
$('mImport').onclick=()=>{$('menu').classList.remove('show');const code=prompt('Paste a layout code:');if(!code)return;
 try{const o=JSON.parse(decodeURIComponent(escape(atob(code.trim()))));if(!o.widgets)throw 0;loadLayout(o);saveToBoard();toast('Imported '+o.name);}catch(e){toast('That code did not work');}};
$('mFull').onclick=()=>{$('menu').classList.remove('show');try{document.documentElement.requestFullscreen();}catch(e){}};

// keep the screen awake while playing
async function wake(){try{if('wakeLock'in navigator)await navigator.wakeLock.request('screen');}catch(e){}}
document.addEventListener('click',wake,{once:true});

// ── boot ──
try{const saved=store.get('layout');layout=saved?JSON.parse(saved):defaultLayout();}catch(e){layout=defaultLayout();}
render();connect();poll();paintStatus();
window.__test={press,doBtn,stickMove,dpadMove,send,wd:id=>layout.widgets.find(w=>w.id==id),addWidget,edit:editWidget,layout:()=>layout};  // for the laptop end-to-end test
window.addEventListener('resize',()=>{W=stage.clientWidth;H=stage.clientHeight;render();});
if(layout.widgets.some(w=>w.type==='tilt'))document.addEventListener('click',enableTilt,{once:true});
</script></body></html>)HTML";
