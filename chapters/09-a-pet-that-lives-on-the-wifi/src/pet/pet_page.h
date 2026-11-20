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
.kinds{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:0 0 12px}
.kind{background:var(--bg);border:1px solid var(--line);color:var(--ink);padding:8px 4px 8px;display:flex;flex-direction:column;align-items:center;gap:4px;font-size:13px}
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
    <button class="kind sel" data-k="1" type="button"><canvas id="kc1"></canvas>Puppy</button>
    <button class="kind" data-k="0" type="button"><canvas id="kc0"></canvas>Kitten</button>
    <button class="kind" data-k="2" type="button"><canvas id="kc2"></canvas>Bunny</button>
    <button class="kind" data-k="3" type="button"><canvas id="kc3"></canvas>Fox</button>
    <button class="kind" data-k="4" type="button"><canvas id="kc4"></canvas>Panda</button>
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
// pets.js >>>
// The five pets — drawn and animated on a canvas. devkit-diary, chapter 9.
//
// One file, used twice: the phone page draws the pet with it, and the desktop helper shows it in a
// transparent always-on-top web view. So the pet on a computer screen and the pet on the phone are
// the same drawing, down to the pixel.
//
// The API a host uses:
//   const S = petState("puppy")        a pet's animation state
//   petStep(S, dt)                     advance it by dt seconds (blinking, breathing, wagging, walking)
//   S.mode = "sit" | "walk" | "zoom" | "sleep"
//   petPet(S), petFeed(S)              a stroke (hearts, a hop) or a bite (its food, chewing)
//   petDraw(canvas, S, cx, groundY, size, dpr)   draw it, bottom-centre at (cx, groundY), size px tall
"use strict";
const TAU=Math.PI*2;
// ───────────────────────── drawing helpers ─────────────────────────
function shade(g,x,y,r,c){const gr=g.createRadialGradient(x-r*0.35,y-r*0.45,r*0.08,x,y,r*1.25);
  gr.addColorStop(0,c[0]);gr.addColorStop(0.55,c[1]);gr.addColorStop(1,c[2]);return gr}
function path(g,fn){g.beginPath();fn(g);g.closePath()}
function fillPath(g,fn,fill){path(g,fn);g.fillStyle=fill;g.fill()}
function rim(g,fn,w,a){g.save();path(g,fn);g.clip();g.lineWidth=w;g.strokeStyle="rgba(255,255,255,"+a+")";
  g.translate(-w*0.35,-w*0.6);path(g,fn);g.stroke();g.restore()}
const zoom=g=>{const t=g.getTransform();return Math.hypot(t.a,t.b)||1};
function inner(g,fn,w,c){g.save();path(g,fn);g.clip();g.lineWidth=w;g.strokeStyle=c;g.filter="blur("+(w*0.5*zoom(g))+"px)";
  g.translate(w*0.2,w*0.55);path(g,fn);g.stroke();g.restore()}
function tufts(g,x0,y0,x1,y1,n,len,fill,dir){g.fillStyle=fill;const ang=Math.atan2(dir[1],dir[0]);
  for(let i=0;i<n;i++){const t=(i+0.5)/n,x=x0+(x1-x0)*t,y=y0+(y1-y0)*t,w=Math.hypot(x1-x0,y1-y0)/n,l=len*(0.75+0.35*((i*7)%3)/2);
    g.beginPath();g.ellipse(x+dir[0]*l*0.35,y+dir[1]*l*0.35,l*0.62,w*0.62,ang,0,TAU);g.fill()}}
function strands(g,fn,cx,cy,rx,ry,n,col,seed){g.save();path(g,fn);g.clip();g.strokeStyle=col;g.lineCap="round";
  let r=seed||1;const rnd=()=>((r=(r*16807)%2147483647)/2147483647);
  for(let i=0;i<n;i++){const a=rnd()*TAU,d=Math.sqrt(rnd()),x=cx+Math.cos(a)*rx*d,y=cy+Math.sin(a)*ry*d,l=3+rnd()*4;
    g.lineWidth=0.7+rnd()*0.6;g.globalAlpha=0.12+rnd()*0.16;g.beginPath();g.moveTo(x,y);
    g.quadraticCurveTo(x+(rnd()-0.5)*4,y+l*0.5,x+(rnd()-0.5)*5,y+l);g.stroke()}g.restore()}
function blob(g,x,y,rx,ry,a){g.save();g.filter="blur("+Math.max(rx,ry)*0.35*zoom(g)+"px)";g.fillStyle="rgba(0,0,0,"+a+")";
  g.beginPath();g.ellipse(x,y,rx,ry,0,0,TAU);g.fill();g.restore()}
function blush(g,pts,r){for(const [x,y] of pts){const b=g.createRadialGradient(x,y,0,x,y,r);
  b.addColorStop(0,"rgba(255,135,155,.42)");b.addColorStop(1,"rgba(255,135,155,0)");g.fillStyle=b;g.beginPath();g.ellipse(x,y,r*1.05,r*0.75,0,0,TAU);g.fill()}}
// a tapered tube along a cubic curve (tails)
function tube(g,p,w0,w1,fill,bulge){
  const N=24,L=[],R=[];
  for(let i=0;i<=N;i++){const t=i/N,mt=1-t;
    const x=mt*mt*mt*p[0]+3*mt*mt*t*p[2]+3*mt*t*t*p[4]+t*t*t*p[6],y=mt*mt*mt*p[1]+3*mt*mt*t*p[3]+3*mt*t*t*p[5]+t*t*t*p[7];
    const dx=3*mt*mt*(p[2]-p[0])+6*mt*t*(p[4]-p[2])+3*t*t*(p[6]-p[4]),dy=3*mt*mt*(p[3]-p[1])+6*mt*t*(p[5]-p[3])+3*t*t*(p[7]-p[5]);
    const l=Math.hypot(dx,dy)||1,w=(w0+(w1-w0)*t+(bulge||0)*Math.sin(Math.PI*t))/2;
    L.push([x-dy/l*w,y+dx/l*w]);R.push([x+dy/l*w,y-dx/l*w])}
  const fn=g=>{g.moveTo(L[0][0],L[0][1]);for(const q of L)g.lineTo(q[0],q[1]);
    const e=L[N],f=R[N];g.quadraticCurveTo((e[0]+f[0])/2+(e[0]-L[N-1][0])*2,(e[1]+f[1])/2+(e[1]-L[N-1][1])*2,f[0],f[1]);
    for(let i=N;i>=0;i--)g.lineTo(R[i][0],R[i][1])};
  fillPath(g,fn,fill);return fn}
const spring=(t,f,d)=>Math.exp(-d*t)*Math.sin(f*t);

// ───────────────────────── shared body parts ─────────────────────────
function paw(g,x,y,w,col){                // a round little paw, the toes marked
  const p=g=>g.ellipse(x,y-w*0.36,w*0.66,w*0.4,0,0,TAU);
  fillPath(g,p,shade(g,x,y-w*0.42,w*0.72,col));
  g.strokeStyle="rgba(95,88,100,.4)";g.lineWidth=1.2;g.lineCap="round";g.beginPath();
  for(const dx of[-0.2,0.08,0.36]){g.moveTo(x+dx*w,y-w*0.1);g.lineTo(x+dx*w,y-w*0.32)}g.stroke()}
function leg(g,x,y,len,w,ang,col,sock){    // a chubby baby leg: full at the top, a soft wrist, a round paw
  g.save();g.translate(x,y);g.rotate(ang);
  const l=g=>{g.moveTo(-w*0.6,0);g.bezierCurveTo(-w*0.64,len*0.35,-w*0.44,len*0.6,-w*0.42,len*0.84);g.quadraticCurveTo(-w*0.42,len+2,0,len+2);
    g.quadraticCurveTo(w*0.42,len+2,w*0.42,len*0.84);g.bezierCurveTo(w*0.44,len*0.6,w*0.64,len*0.35,w*0.6,0)};
  fillPath(g,l,shade(g,-w*0.15,len*0.3,len*0.8,col));
  if(sock){g.save();path(g,l);g.clip();fillPath(g,g=>g.rect(-w,len*0.6,2*w,len),shade(g,0,len*0.85,w,sock));g.restore()}
  paw(g,w*0.06,len+4,w*1.3,sock||col);g.restore()}
function leg2(g,x,y,len,w,ang,bend,col,sock){   // a walking leg: swings at the top, bends at the knee, still chubby
  g.save();g.translate(x,y);g.rotate(ang);
  const up=g=>{g.moveTo(-w*0.62,0);g.bezierCurveTo(-w*0.66,len*0.25,-w*0.52,len*0.45,-w*0.46,len*0.55);g.lineTo(w*0.46,len*0.55);g.bezierCurveTo(w*0.52,len*0.45,w*0.66,len*0.25,w*0.62,0)};
  fillPath(g,up,shade(g,-w*0.15,len*0.2,len*0.55,col));g.translate(0,len*0.52);g.rotate(bend);
  const lo=g=>{g.moveTo(-w*0.46,0);g.bezierCurveTo(-w*0.48,len*0.2,-w*0.42,len*0.36,-w*0.42,len*0.46);g.lineTo(w*0.42,len*0.46);g.bezierCurveTo(w*0.42,len*0.36,w*0.48,len*0.2,w*0.46,0)};
  fillPath(g,g=>g.ellipse(0,0,w*0.5,w*0.44,0,0,TAU),shade(g,0,0,w*0.6,col));
  fillPath(g,lo,shade(g,0,len*0.25,len*0.45,sock||col));paw(g,w*0.06,len*0.48+3,w*1.3,sock||col);g.restore()}

// an eye, looking at you. e = {iris:[light,mid,dark], slit:bool, lid:colour of the closed-eye line}
function eye(g,x,y,s,st,e,u){
  u=u||1;g.save();g.translate(x,y);g.scale(s*u,u);
  if(st.happy>0.5||st.blink>0.85){g.lineWidth=3.4;g.strokeStyle=e.lid;g.lineCap="round";g.beginPath();
    if(st.happy>0.5){g.moveTo(-11,4);g.quadraticCurveTo(0,-8,11,4)}else{g.moveTo(-11,0);g.quadraticCurveTo(0,6,11,0)}g.stroke();g.restore();return}
  g.save();g.scale(1,Math.max(0.08,1-st.blink));
  fillPath(g,g=>g.ellipse(0,0,15,17,0,0,TAU),"#060608");
  const iris=g=>g.ellipse(0.5,1,12.8,14.8,0,0,TAU);
  fillPath(g,iris,(()=>{const gr=g.createRadialGradient(0,7,1,0,2,15);gr.addColorStop(0,e.iris[0]);gr.addColorStop(0.4,e.iris[1]);
    gr.addColorStop(0.78,e.iris[2]);gr.addColorStop(1,"#0d0805");return gr})());
  g.save();path(g,iris);g.clip();g.strokeStyle="rgba(255,240,200,.22)";g.lineWidth=0.8;
  for(let i=0;i<14;i++){const a=i/14*TAU;g.beginPath();g.moveTo(1+Math.cos(a)*7,2+Math.sin(a)*8);g.lineTo(1+Math.cos(a)*12,2+Math.sin(a)*13.5);g.stroke()}
  const lid=g.createLinearGradient(0,-16,0,-2);lid.addColorStop(0,"rgba(0,0,0,.6)");lid.addColorStop(1,"rgba(0,0,0,0)");g.fillStyle=lid;g.fillRect(-16,-17,32,16);g.restore();
  const wide=st.happy>0.2||st.tongue>0.5;                     // slit pupils go round when it's excited
  fillPath(g,g=>g.ellipse(1,2.5,e.slit&&!wide?3.2:8.2,9.2,0,0,TAU),"#040405");
  fillPath(g,g=>g.ellipse(-4.2,-5.8,5.4,6.2,-0.45,0,TAU),"rgba(255,255,255,.97)");
  fillPath(g,g=>g.ellipse(5,6.5,2.3,2.3,0,0,TAU),"rgba(255,255,255,.9)");
  fillPath(g,g=>g.ellipse(2,10.5,5.5,2,0,0,TAU),"rgba(255,230,180,.2)");
  g.restore();
  g.lineWidth=1.3;g.strokeStyle="rgba(255,255,255,.14)";g.beginPath();g.ellipse(0.5,2,15.5,17.5,0,0.35*Math.PI,0.65*Math.PI);g.stroke();
  g.restore();
}
function glossyNose(g,fn,cx,cy,r,col){fillPath(g,fn,shade(g,cx,cy,r,col));
  fillPath(g,g=>g.ellipse(cx-r*0.32,cy-r*0.32,r*0.32,r*0.17,-0.3,0,TAU),"rgba(255,255,255,.7)")}
const W3=c=>[c[0],c[1],c[2]];

// ───────────────────────── the five pets ─────────────────────────
const P={};
const BLACK=["#3a3c46","#1d1e24","#0b0b0e"],BLACKF=["#24252c","#111216","#050506"],WHITE=["#ffffff","#eef0f5","#d4d9e4"],WHITEF=["#eef0f5","#d8dce6","#b9bfcc"];

// ── border collie puppy ──
// ears at the sides of the head, folding down — all black, as a real puppy's are
function earDrop(g,near){
  const e=g=>{g.moveTo(-11,0);g.bezierCurveTo(-9,-9,6,-13,15,-8);g.bezierCurveTo(27,-2,32,14,28,30);g.bezierCurveTo(25,42,12,44,6,36);g.bezierCurveTo(0,28,-7,14,-11,0)};
  fillPath(g,e,shade(g,10,10,30,near?BLACK:BLACKF));
  fillPath(g,g=>{g.moveTo(-2,2);g.bezierCurveTo(2,-2,10,-2,14,4);g.bezierCurveTo(18,12,16,22,12,26);g.bezierCurveTo(8,18,2,10,-2,2)},"rgba(0,0,0,.35)");
  if(near)rim(g,e,3,0.22);tufts(g,6,38,22,34,3,6,near?"#221f20":"#151213",[0,1])}
// the puppy exactly as it was after the 'enhance it more' pass: round face, deep eyes, folded ears
const PC={
  black:"#1d1e24", blackHi:"#3a3c46", blackLo:"#0b0b0e",
  white:"#ffffff", whiteLo:"#d9dde7", whiteMid:"#eef0f5",
  pink:"#f39aa8", pinkLo:"#d96f82", tongue:"#ff8597", tongueLo:"#e0566d",
  iris:"#5b3a22", irisLo:"#24160c", nose:"#15151a",
};
function pupEar(g,near){
  const base=g=>{g.moveTo(-15,8);g.bezierCurveTo(-17,-8,-10,-26,2,-32);g.bezierCurveTo(10,-36,18,-34,20,-28);
    g.bezierCurveTo(22,-20,18,-6,15,8)};
  fillPath(g,base,shade(g,2,-14,30,[near?PC.blackHi:PC.black,PC.black,PC.blackLo]));
  fillPath(g,g=>{g.moveTo(-8,6);g.bezierCurveTo(-9,-6,-5,-16,1,-20);g.bezierCurveTo(5,-20,7,-12,6,-2);g.bezierCurveTo(5,2,3,5,2,7)},
    shade(g,0,-6,16,["#b56b7c","#8f4c5c","#5e2f3a"]));
  // the folded flap
  const flap=g=>{g.moveTo(-1,-31);g.bezierCurveTo(8,-38,20,-34,21,-27);g.bezierCurveTo(22,-18,18,-10,12,-6);g.bezierCurveTo(10,-14,6,-24,-1,-31)};
  fillPath(g,flap,shade(g,12,-24,16,[PC.blackHi,PC.black,PC.blackLo]));
  if(near)rim(g,base,3,0.2);
  g.lineWidth=1.2;g.strokeStyle="rgba(0,0,0,.45)";g.beginPath();g.moveTo(-1,-31);g.bezierCurveTo(6,-24,10,-14,12,-6);g.stroke();
}

function pupHead(g,st){
  // far ear (behind): short, soft, the tip folded forward
  g.save();g.translate(-22,-54);g.rotate(-0.12+st.earL);pupEar(g,false);g.restore();
  // skull
  const skull=g=>{g.moveTo(-60,-4);g.bezierCurveTo(-62,-46,-30,-64,2,-64);g.bezierCurveTo(38,-64,62,-44,64,-8);
    g.bezierCurveTo(66,22,54,42,34,50);g.bezierCurveTo(14,58,-16,58,-34,50);g.bezierCurveTo(-52,42,-59,22,-60,-4)};
  fillPath(g,skull,shade(g,0,-10,70,[PC.blackHi,PC.black,PC.blackLo]));
  strands(g,skull,0,-24,56,36,70,"#7a7e8c",7);
  rim(g,skull,5,0.26);
  // white blaze, cheeks and chin
  const blaze=g=>{g.moveTo(-1,-63);g.bezierCurveTo(7,-63,9,-42,11,-26);g.bezierCurveTo(13,-12,26,-4,42,4);g.bezierCurveTo(60,14,62,36,42,48);
    g.bezierCurveTo(22,58,-18,58,-36,48);g.bezierCurveTo(-54,38,-52,14,-36,6);g.bezierCurveTo(-22,-2,-10,-12,-8,-26);g.bezierCurveTo(-6,-42,-9,-63,-1,-63)};
  fillPath(g,blaze,shade(g,6,10,64,[PC.white,PC.whiteMid,PC.whiteLo]));
  inner(g,blaze,8,"rgba(120,130,160,.35)");
  strands(g,blaze,4,30,46,18,26,"#c9cfdb",3);
  // a little blush on the cheeks
  for(const [bx,by] of [[-30,24],[44,20]]){const bg=g.createRadialGradient(bx,by,0,bx,by,13);
    bg.addColorStop(0,"rgba(255,140,160,.42)");bg.addColorStop(1,"rgba(255,140,160,0)");g.fillStyle=bg;g.beginPath();g.ellipse(bx,by,14,10,0,0,TAU);g.fill()}
  tufts(g,-40,44,-28,52,3,7,PC.whiteMid,[-0.6,0.7]);tufts(g,30,52,44,44,3,7,PC.whiteMid,[0.6,0.7]);
  // muzzle
  const muz=g=>{g.ellipse(16,24,27,19,0,0,TAU)};
  fillPath(g,muz,shade(g,16,24,30,[PC.white,PC.whiteMid,"#cfd4df"]));
  // mouth (chew moves the jaw)
  const jaw=st.chew*3;
  g.lineCap="round";g.lineWidth=2.6;g.strokeStyle="#2a2228";
  g.beginPath();g.moveTo(16,22);g.lineTo(16,28+jaw);g.moveTo(16,28+jaw);g.quadraticCurveTo(10,35+jaw,2,32+jaw*0.5);
  g.moveTo(16,28+jaw);g.quadraticCurveTo(22,35+jaw,31,31+jaw*0.5);g.stroke();
  if(st.tongue>0.01){                    // a happy tongue, out a little
    const L=10*st.tongue;
    fillPath(g,g=>{g.moveTo(9,31+jaw);g.bezierCurveTo(9,34+L,23,34+L,23,31+jaw);g.bezierCurveTo(20,29+jaw,12,29+jaw,9,31+jaw)},
      shade(g,16,33+L/2,12,[PC.tongue,PC.tongue,PC.tongueLo]));
    g.strokeStyle="rgba(170,40,70,.5)";g.lineWidth=1.2;g.beginPath();g.moveTo(16,31+jaw);g.lineTo(16,30+jaw+L*0.8);g.stroke();
  }
  // nose
  const nose=g=>{g.moveTo(3,10);g.bezierCurveTo(3,1,29,1,29,10);g.bezierCurveTo(29,18,21,23,16,23);g.bezierCurveTo(11,23,3,18,3,10)};
  fillPath(g,nose,shade(g,16,12,16,["#4a4a55",PC.nose,"#000"]));
  fillPath(g,g=>{g.ellipse(11,7,4.6,2.4,-0.3,0,TAU)},"rgba(255,255,255,.75)");
  fillPath(g,g=>{g.ellipse(10.5,15,2.4,1.5,0.5,0,TAU)},"#000");              // nostrils
  fillPath(g,g=>{g.ellipse(21.5,15,2.4,1.5,-0.5,0,TAU)},"#000");
  // eyes
  for(const [ex,s] of [[-16,0.92],[26,1]]){
    g.save();g.translate(ex,-12);g.scale(s,1);
    if(st.happy>0.5||st.blink>0.85){            // closed: a soft smiling arc
      g.lineWidth=3.6;g.strokeStyle="#0b0b0e";g.lineCap="round";g.beginPath();
      if(st.happy>0.5){g.moveTo(-11,4);g.quadraticCurveTo(0,-8,11,4)}else{g.moveTo(-11,0);g.quadraticCurveTo(0,6,11,0)}
      g.stroke();
    }else{
      const open=1-st.blink;
      g.save();g.scale(1,Math.max(0.08,open));
      fillPath(g,g=>g.ellipse(0,0,15,17,0,0,TAU),"#060608");                         // socket
      const iris=g=>g.ellipse(0.5,1,12.8,14.8,0,0,TAU);
      fillPath(g,iris,(()=>{const gr=g.createRadialGradient(0,7,1,0,2,15);
        gr.addColorStop(0,"#c28a52");gr.addColorStop(0.35,"#8c5a30");gr.addColorStop(0.75,PC.iris);gr.addColorStop(1,"#140b05");return gr})());
      g.save();path(g,iris);g.clip();                                                 // iris fibres
      g.strokeStyle="rgba(230,170,110,.28)";g.lineWidth=0.8;
      for(let i=0;i<14;i++){const a=i/14*TAU;g.beginPath();g.moveTo(1+Math.cos(a)*7,2+Math.sin(a)*8);g.lineTo(1+Math.cos(a)*12,2+Math.sin(a)*13.5);g.stroke()}
      const lid=g.createLinearGradient(0,-16,0,-2);lid.addColorStop(0,"rgba(0,0,0,.65)");lid.addColorStop(1,"rgba(0,0,0,0)");
      g.fillStyle=lid;g.fillRect(-16,-17,32,16);g.restore();                          // the upper lid's shadow
      fillPath(g,g=>g.ellipse(1,2.5,8.2,9.2,0,0,TAU),"#040405");                     // pupil
      fillPath(g,g=>g.ellipse(-4.2,-5.8,5.4,6.2,-0.45,0,TAU),"rgba(255,255,255,.97)"); // key light
      fillPath(g,g=>g.ellipse(5,6.5,2.3,2.3,0,0,TAU),"rgba(255,255,255,.9)");          // fill light
      fillPath(g,g=>g.ellipse(2,10.5,5.5,2,0,0,TAU),"rgba(255,210,160,.22)");         // bounce from below
      g.restore();
      g.lineWidth=1.4;g.strokeStyle="rgba(255,255,255,.14)";g.beginPath();            // lower lid
      g.ellipse(0.5,2,15.5,17.5,0,0.35*Math.PI,0.65*Math.PI);g.stroke();
      // a soft brow that lifts when it's curious
      const by=-21-st.brow*4;
      fillPath(g,g=>g.ellipse(-2,by,6.5,3,-0.15-st.brow*0.3,0,TAU),"rgba(120,124,140,.55)");
    }
    g.restore();
  }
  blob(g,30,-44,16,7,0.35);
  // near ear: the same, a little in front
  g.save();g.translate(22,-56);g.rotate(0.1+st.earR);pupEar(g,true);g.restore();

}


P.puppy={name:"Border collie puppy",food:"🦴",coat:BLACK,coatFar:BLACKF,sheen:"#6f7381",bib:WHITE,haunch:BLACK,
  fore:WHITE,foreFar:WHITEF,hind:BLACK,hindFar:BLACKF,hindSock:WHITE,paw:WHITE,
  head(g,S){pupHead(g,S)},
  tail:null,headSit:[30,-136],headWalk:[60,-132],headSleep:[50,-46]};
P.puppy.tail=function(g,pose,S){
  if(pose==="sleep"){fillPath(g,g=>{g.moveTo(-84,-6);g.bezierCurveTo(-60,10,40,12,74,-4);g.bezierCurveTo(80,-10,72,-18,64,-14);g.bezierCurveTo(30,-2,-40,-4,-74,-18)},shade(g,0,-6,70,BLACK));
    fillPath(g,g=>{g.moveTo(58,-12);g.bezierCurveTo(70,-20,84,-12,76,-2);g.bezierCurveTo(70,4,60,0,58,-12)},shade(g,68,-8,12,WHITE));return}
  const sit=pose==="sit";g.save();g.translate(sit?-48:-62,sit?-22:-90);g.rotate((sit?0.05:-0.75)+S.wag*(sit?0.8:1));const len=sit?64:58;
  const t=g=>{g.moveTo(2,-13);g.bezierCurveTo(-len*0.3,-30,-len*0.72,-42,-len*0.98,-38);g.bezierCurveTo(-len-14,-34,-len-16,-14,-len*0.92,-6);g.bezierCurveTo(-len*0.7,4,-len*0.32,13,2,13)};
  fillPath(g,t,shade(g,-len*0.5,-16,len*0.75,BLACK));tufts(g,-len*0.85,-38,-len*0.2,-22,5,9,"#221f20",[0.15,-1]);tufts(g,-len*0.85,-2,-len*0.2,10,4,8,"#221f20",[0,1]);rim(g,t,4,0.16);
  fillPath(g,g=>{g.moveTo(-len*0.8,-38);g.bezierCurveTo(-len-6,-46,-len-22,-30,-len-12,-10);g.bezierCurveTo(-len*0.96,-2,-len*0.82,-10,-len*0.8,-38)},shade(g,-len-4,-26,20,WHITE));
  tufts(g,-len-16,-34,-len-14,-12,3,8,"#f7f4ef",[-1,0]);g.restore()};

// ── a head in the puppy's style, shared by the kitten, the bunny and the fox ──
// o: coat, sheen, muzzle colours; ear(g,near); iris colours, slit; nose: "cat" | "fox"; mouth: "cat" | "bunny" | "fox";
//    mask(g) for markings; whiskers
function cuteHead(g,S,o){
  g.save();g.translate(o.earAt[0][0],o.earAt[0][1]);g.rotate(o.earAt[0][2]+S.earL);o.ear(g,false);g.restore();
  const skull=g=>{g.moveTo(-60,-4);g.bezierCurveTo(-62,-46,-30,-64,2,-64);g.bezierCurveTo(38,-64,62,-44,64,-8);
    g.bezierCurveTo(66,22,54,42,34,50);g.bezierCurveTo(14,58,-16,58,-34,50);g.bezierCurveTo(-52,42,-59,22,-60,-4)};
  if(o.ruff)tufts(g,-60,-8,-40,46,6,14,o.coat[1],[-1,0.4]),tufts(g,44,46,64,-8,6,14,o.coat[1],[1,0.4]);
  fillPath(g,skull,shade(g,0,-10,70,o.coat));strands(g,skull,0,-24,56,36,60,o.sheen,7);rim(g,skull,5,0.28);
  if(o.mask)o.mask(g);
  // muzzle: two soft puffs and a chin
  const muz=g=>{g.moveTo(-6,22);g.bezierCurveTo(-8,8,8,4,16,12);g.bezierCurveTo(24,4,42,8,40,22);g.bezierCurveTo(40,36,28,42,16,42);g.bezierCurveTo(4,42,-6,36,-6,22)};
  fillPath(g,muz,shade(g,16,24,30,o.muzzle));inner(g,muz,7,"rgba(120,110,130,.28)");
  blush(g,[[-32,22],[48,18]],12);
  // nose
  if(o.nose==="fox"){const n=g=>{g.moveTo(8,10);g.bezierCurveTo(8,3,24,3,24,10);g.bezierCurveTo(24,16,19,19,16,19);g.bezierCurveTo(13,19,8,16,8,10)};
    fillPath(g,n,shade(g,16,11,12,["#4a4a55","#15151a","#000"]));fillPath(g,g=>g.ellipse(12.5,7.5,3.2,1.8,-0.3,0,TAU),"rgba(255,255,255,.7)")}
  else{const tw=o.twitch?Math.sin(S.t*9)*0.8*(1-S.happy):0;
    fillPath(g,g=>{g.moveTo(9,12+tw);g.bezierCurveTo(11,7+tw,21,7+tw,23,12+tw);g.bezierCurveTo(21,17+tw,18,19+tw,16,19+tw);g.bezierCurveTo(14,19+tw,11,17+tw,9,12+tw)},
      shade(g,16,12,9,["#ffc0cb","#f093a5","#c4627a"]));fillPath(g,g=>g.ellipse(13.5,10.5,2.6,1.4,-0.3,0,TAU),"rgba(255,255,255,.6)")}
  // mouth
  const jaw=S.chew*2;g.lineCap="round";g.lineWidth=2.2;g.strokeStyle=o.line||"#5a4450";g.beginPath();
  g.moveTo(16,19);g.lineTo(16,24+jaw);g.quadraticCurveTo(11,30+jaw,6,27);g.moveTo(16,24+jaw);g.quadraticCurveTo(21,30+jaw,26,27);g.stroke();
  if(o.mouth==="bunny")fillPath(g,g=>{g.rect(12.5,24+jaw,7,5+jaw*0.5)},"#ffffff"),g.strokeStyle="rgba(150,140,140,.6)",g.lineWidth=0.8,g.beginPath(),g.moveTo(16,24+jaw),g.lineTo(16,29+jaw*1.5),g.stroke();
  if(S.tongue>0.3&&o.mouth!=="bunny")fillPath(g,g=>g.ellipse(16,28+jaw,4.2,3.6*S.tongue,0,0,Math.PI),shade(g,16,29,6,["#ff9aab","#ff8597","#e0566d"]));
  if(o.whiskers){g.strokeStyle="rgba(255,255,255,.88)";g.lineWidth=0.9;g.beginPath();
    for(const s of[-1,1])for(const k of[-1,0,1]){const x=s<0?-2:34;g.moveTo(x,24+k*3);g.quadraticCurveTo(x+s*18,22+k*5,x+s*34,20+k*8)}g.stroke()}
  // eyes and brows, as the puppy's
  const E={iris:o.iris,slit:o.slit,lid:o.lid||"#2a2328"};
  eye(g,-16,-12,0.92,S,E,o.eyeU);eye(g,26,-12,1,S,E,o.eyeU);
  if(o.brow!=="none")for(const [bx,f] of [[-18,0.9],[24,1]])fillPath(g,g=>g.ellipse(bx,-33-S.brow*4,6.2*f,2.8,-0.15-S.brow*0.3,0,TAU),o.brow||"rgba(255,255,255,.35)");
  g.save();g.translate(o.earAt[1][0],o.earAt[1][1]);g.rotate(o.earAt[1][2]+S.earR);o.ear(g,true);g.restore();
  if(o.top)o.top(g);
}

// ── kitten: white, fluffy, blue-eyed ──
const KW=["#ffffff","#f2f0f6","#cfcadb"],KWF=["#efedf3","#dad6e2","#b3aec0"];
function kitEar(g,near){
  const e=g=>{g.moveTo(-17,10);g.bezierCurveTo(-16,-10,-6,-30,4,-40);g.bezierCurveTo(12,-28,18,-10,18,10)};
  fillPath(g,e,shade(g,2,-12,30,near?KW:KWF));
  fillPath(g,g=>{g.moveTo(-9,8);g.bezierCurveTo(-8,-6,-2,-20,4,-28);g.bezierCurveTo(9,-18,11,-6,10,8)},shade(g,2,-8,18,["#ffc8d2","#f2a3b2","#cf7d8e"]));
  tufts(g,-4,6,8,6,4,12,"rgba(255,255,255,.95)",[0,-1]);if(near)rim(g,e,3,0.3)}
P.kitten={name:"Kitten",food:"🐟",coat:KW,coatFar:KWF,sheen:"#ffffff",bib:KW,haunch:KW,
  fore:KW,foreFar:KWF,hind:KW,hindFar:KWF,paw:["#ffffff","#f6eef2","#e3c9d2"],
  head(g,S){cuteHead(g,S,{coat:KW,sheen:"#ffffff",muzzle:["#ffffff","#f6f4f9","#dcd8e6"],ear:kitEar,
    earAt:[[-26,-54,-0.16],[24,-56,0.16]],iris:["#d6f0ff","#7fbcef","#2f72b4"],slit:true,nose:"cat",mouth:"cat",whiskers:true,ruff:true,
    lid:"#8b8697",line:"#a07f8c",brow:"rgba(205,200,215,.6)",
    top:g=>tufts(g,-16,-62,14,-64,4,9,"#ffffff",[0,-1])})},
  tail(g,pose,S){const w=S.wag*0.4;g.save();
    if(pose==="sleep"){tube(g,[-84,-12,-56,10,40,14,80,-6],18,22,shade(g,0,-4,80,KW),6);g.restore();return}
    if(pose==="sit"){g.translate(-48,-18);g.rotate(w*0.5);tube(g,[0,0,-30,-4,-54,-36,-40,-72],18,22,shade(g,-34,-36,50,KW),8);tufts(g,-50,-58,-34,-80,3,8,"#ffffff",[0,-1])}
    else{g.translate(-62,-96);g.rotate(-0.1+w);tube(g,[0,0,-20,-18,-24,-56,-4,-80],18,22,shade(g,-14,-40,50,KW),8);tufts(g,-8,-84,6,-72,3,8,"#ffffff",[0.4,-1])}
    g.restore()},
  headSit:[30,-134],headWalk:[62,-128],headSleep:[50,-46]};

// ── bunny: cinnamon and white, tall ears, a little hop ──
const BC=["#f0d2b0","#d4a473","#9c6d44"],BCF=["#d7b28c","#b98a5c","#81572f"];
function bunEar(g,near){
  const e=g=>{g.moveTo(-11,8);g.bezierCurveTo(-18,-24,-15,-62,-4,-80);g.bezierCurveTo(4,-90,15,-78,14,-58);g.bezierCurveTo(13,-32,11,-10,10,8)};
  fillPath(g,e,shade(g,0,-36,50,near?BC:BCF));
  fillPath(g,g=>{g.moveTo(-4,2);g.bezierCurveTo(-9,-24,-7,-56,0,-70);g.bezierCurveTo(5,-74,8,-60,7,-44);g.bezierCurveTo(6,-26,5,-8,4,2)},shade(g,2,-34,32,["#ffd0d8","#f4a5b4","#cf7c8e"]));
  if(near)rim(g,e,3,0.3)}
P.bunny={name:"Bunny",food:"🥕",coat:BC,coatFar:BCF,sheen:"#fff0e0",bib:WHITE,haunch:BC,
  fore:BC,foreFar:BCF,hind:BC,hindFar:BCF,paw:["#ffffff","#f4eee8","#dcd2c6"],hop:true,short:0.7,
  head(g,S){cuteHead(g,S,{coat:BC,sheen:"#fff0e0",muzzle:["#ffffff","#f8f4ef","#e2d8cc"],ear:bunEar,
    earAt:[[-14,-56,-0.1+Math.sin(S.t*1.7)*0.04],[12,-58,0.06+Math.sin(S.t*1.7+1)*0.04]],
    iris:["#c48a5a","#7a4a28","#3a2214"],nose:"cat",mouth:"bunny",whiskers:true,twitch:true,lid:"#6a4a38",line:"#8a6a5a",brow:"rgba(255,240,225,.45)",
    mask:g=>fillPath(g,g=>{g.moveTo(-2,-62);g.bezierCurveTo(6,-62,8,-44,10,-30);g.bezierCurveTo(11,-20,6,-12,0,-10);g.bezierCurveTo(-6,-14,-8,-24,-7,-34);g.bezierCurveTo(-6,-46,-9,-62,-2,-62)},"rgba(255,255,255,.55)")})},
  tail(g,pose){const p=pose==="sleep"?[-82,-26]:pose==="sit"?[-50,-26]:[-66,-82];
    fillPath(g,g=>g.ellipse(p[0],p[1],13,12,0,0,TAU),shade(g,p[0],p[1],14,WHITE));tufts(g,p[0]-12,p[1]-8,p[0]-10,p[1]+8,4,6,"#ffffff",[-1,0])},
  headSit:[28,-122],headWalk:[60,-116],headSleep:[50,-42]};

// ── fox kit: bright orange, a white mask, black-tipped ears ──
const FX=["#ffb36e","#f07e34","#b5541a"],FXF=["#e8954f","#cf6626","#8d3f12"],FD=["#4a3a33","#2b211d","#140f0d"];
function foxEar(g,near){
  const e=g=>{g.moveTo(-17,10);g.bezierCurveTo(-16,-14,-6,-40,4,-54);g.bezierCurveTo(13,-38,19,-14,18,10)};
  fillPath(g,e,shade(g,2,-16,34,near?FX:FXF));
  fillPath(g,g=>{g.moveTo(-9,8);g.bezierCurveTo(-8,-10,-2,-30,4,-40);g.bezierCurveTo(9,-28,11,-10,10,8)},shade(g,2,-12,22,["#fff6ec","#f1dfcb","#c9b39b"]));
  fillPath(g,g=>{g.moveTo(-6,-36);g.bezierCurveTo(-2,-46,2,-52,4,-54);g.bezierCurveTo(7,-48,10,-42,12,-34);g.bezierCurveTo(6,-36,0,-37,-6,-36)},"#241b18");
  tufts(g,-4,6,8,6,3,10,"rgba(255,255,255,.9)",[0,-1]);if(near)rim(g,e,3,0.28)}
P.fox={name:"Fox kit",food:"🍗",coat:FX,coatFar:FXF,sheen:"#ffdcb8",bib:WHITE,haunch:FX,
  fore:FX,foreFar:FXF,hind:FX,hindFar:FXF,foreSock:FD,hindSock:FD,paw:FD,
  head(g,S){cuteHead(g,S,{coat:FX,sheen:"#ffdcb8",muzzle:["#ffffff","#f8f4ef","#e0d6ca"],ear:foxEar,
    earAt:[[-26,-54,-0.18],[24,-56,0.18]],iris:["#ffe08a","#f0a32c","#9a5a12"],slit:true,nose:"fox",mouth:"fox",whiskers:false,ruff:true,
    lid:"#3a2a24",line:"#4a3028",brow:"rgba(255,230,200,.5)",
    mask:g=>{                                             // the white mask: cheeks, round under the eyes
      const m=g=>{g.moveTo(-58,6);g.bezierCurveTo(-46,0,-30,2,-16,10);g.bezierCurveTo(-4,4,8,4,16,6);g.bezierCurveTo(26,4,40,2,52,8);
        g.bezierCurveTo(60,12,62,22,56,32);g.bezierCurveTo(46,48,24,56,4,56);g.bezierCurveTo(-24,56,-48,44,-56,30);g.bezierCurveTo(-60,22,-61,12,-58,6)};
      fillPath(g,m,shade(g,0,30,60,["#ffffff","#f8f4ef","#e0d6ca"]));tufts(g,-56,20,-40,48,4,11,"#fbf8f4",[-1,0.4]);tufts(g,42,48,58,20,4,11,"#fbf8f4",[1,0.4])}})},
  tail(g,pose,S){const w=S.wag*0.5;g.save();
    if(pose==="sleep"){tube(g,[-84,-14,-50,10,40,14,86,-6],22,26,shade(g,0,-4,80,FX),8);fillPath(g,g=>g.ellipse(84,-8,14,11,-0.3,0,TAU),shade(g,84,-8,14,WHITE));g.restore();return}
    if(pose==="sit"){g.translate(-46,-14);g.rotate(w*0.5);tube(g,[0,0,10,14,70,22,104,10],22,26,shade(g,50,8,60,FX),10);
      fillPath(g,g=>g.ellipse(104,8,15,12,-0.2,0,TAU),shade(g,104,8,15,WHITE));tufts(g,112,0,116,16,3,7,"#f6f3ee",[1,0])}
    else{g.translate(-62,-88);g.rotate(0.15+w);tube(g,[0,0,-24,4,-50,14,-72,8],22,26,shade(g,-36,6,50,FX),12);
      fillPath(g,g=>g.ellipse(-74,8,15,12,0,0,TAU),shade(g,-74,8,15,WHITE));tufts(g,-86,0,-86,16,3,7,"#f6f3ee",[-1,0])}
    g.restore()},
  headSit:[30,-134],headWalk:[62,-130],headSleep:[50,-46]};

function pandaEar(g,near){              // round, black, set on top of the head
  fillPath(g,g=>g.ellipse(0,-6,15,14,0,0,TAU),shade(g,0,-8,16,near?PB:PBF));
  fillPath(g,g=>g.ellipse(1,-4,7.5,6.5,0,0,TAU),"rgba(80,70,80,.45)");if(near)rim(g,g=>g.ellipse(0,-6,15,14,0,0,TAU),3,0.18)}
// ── giant panda cub: creamy white, thick black legs, the band over the shoulders ──
const PW=["#fdfaf2","#eee7d8","#c9bea8"],PWF=["#ece5d6","#d6cdbb","#ada28c"],PB=["#3b393d","#1d1c20","#0a090b"],PBF=["#28272b","#141316","#060506"];
P.panda={name:"Panda cub",food:"🎋",coat:PW,coatFar:PWF,sheen:"#ffffff",bib:PW,haunch:PB,
  fore:PB,foreFar:PBF,hind:PB,hindFar:PBF,paw:PB,chub:1.12,legW:1.5,short:true,
  marks(g,pose){
    if(pose==="walk")fillPath(g,g=>{g.moveTo(8,-118);g.bezierCurveTo(34,-122,62,-110,70,-86);g.bezierCurveTo(74,-64,66,-46,56,-38);g.bezierCurveTo(40,-40,30,-60,26,-76);g.bezierCurveTo(22,-92,14,-106,8,-118)},shade(g,44,-84,44,PB));
    if(pose==="sit")fillPath(g,g=>{g.moveTo(-16,-104);g.bezierCurveTo(16,-124,60,-114,68,-82);g.bezierCurveTo(72,-60,62,-40,50,-30);g.bezierCurveTo(32,-52,6,-78,-16,-104)},shade(g,30,-84,46,PB));
    if(pose==="sleep")fillPath(g,g=>{g.moveTo(16,-88);g.bezierCurveTo(46,-90,78,-72,86,-44);g.bezierCurveTo(72,-32,50,-38,34,-50);g.bezierCurveTo(26,-62,20,-76,16,-88)},shade(g,52,-62,38,PB))},
  head(g,S){cuteHead(g,S,{coat:PW,sheen:"#ffffff",muzzle:["#ffffff","#f6f1e6","#dcd2bf"],ear:pandaEar,
    earAt:[[-28,-52,-0.2],[26,-54,0.2]],iris:["#7a6460","#33292a","#0e0a09"],eyeU:0.82,nose:"fox",mouth:"fox",
    lid:"#efe8da",line:"#2a2228",brow:"none",
    mask:g=>{                                  // eye patches: ovals tipped down and outward, as a real panda's
      for(const[x,y,a,s]of[[-19,-8,0.55,0.92],[29,-8,-0.55,1]]){g.save();g.translate(x,y);g.rotate(a);g.scale(s,1);
        fillPath(g,g=>{g.moveTo(0,-17);g.bezierCurveTo(12,-17,16,-4,14,8);g.bezierCurveTo(12,20,4,24,-2,22);g.bezierCurveTo(-12,20,-16,6,-14,-6);g.bezierCurveTo(-12,-14,-8,-17,0,-17)},shade(g,0,0,20,PB));g.restore()}}})},
  tail(g,pose){const p=pose==="sleep"?[-86,-30]:pose==="sit"?[-52,-30]:[-70,-84];fillPath(g,g=>g.ellipse(p[0],p[1],10,9,0,0,TAU),shade(g,p[0],p[1],11,PW))},
  headSit:[28,-134],headWalk:[62,-108],headSleep:[50,-46]};


// ───────────────────────── poses, shared by every pet ─────────────────────────
function headAt(g,sp,S,pos,tilt,st){g.save();g.translate(pos[0],pos[1]);g.rotate(tilt);sp.head(g,st||S);g.restore()}
function sit(g,S,sp){
  const br=Math.sin(S.t*2.2)*0.012;g.save();g.scale((1+br*0.4)*(sp.slim||1)*(sp.chub||1.08),(1-br)*1.02);
  sp.tail(g,"sit",S);
  fillPath(g,g=>g.ellipse(-4,-36,36,34,0,0,TAU),shade(g,-4,-36,38,sp.haunch===PB?PBF:sp.coatFar));
  const torso=g=>{g.moveTo(-56,-14);g.bezierCurveTo(-64,-70,-28,-116,16,-118);g.bezierCurveTo(54,-120,70,-88,68,-48);g.bezierCurveTo(66,-14,52,0,26,0);g.lineTo(-34,0);g.bezierCurveTo(-50,0,-56,-6,-56,-14)};
  fillPath(g,torso,shade(g,4,-60,80,sp.coat));strands(g,torso,0,-60,60,56,70,sp.sheen,11);rim(g,torso,6,0.22);
  if(sp.marks)sp.marks(g,"sit");
  if(sp.bib!==sp.coat){const bib=g=>{g.moveTo(0,-118);g.bezierCurveTo(36,-120,62,-96,62,-62);g.bezierCurveTo(62,-36,52,-20,40,-12);g.bezierCurveTo(26,-26,8,-44,-2,-66);g.bezierCurveTo(-8,-84,-10,-108,0,-118)};
    fillPath(g,bib,shade(g,30,-76,56,sp.bib));inner(g,bib,8,"rgba(120,130,160,.3)");tufts(g,-2,-66,38,-14,7,10,sp.bib[1],[-0.45,0.85])}
  blob(g,32,-104,40,12,0.4);
  const hh=g=>g.ellipse(-22,-32,36,32,-0.2,0,TAU);
  fillPath(g,hh,shade(g,-20,-32,34,sp.haunch));strands(g,hh,-22,-34,30,26,22,sp.haunch===PB?"#6f7381":sp.sheen,13);rim(g,hh,5,0.18);
  if(sp.marks&&sp===P.kitten){g.save();path(g,hh);g.clip();sp.marks(g,"sit");g.restore()}
  const LW=sp.legW||1.3,LL=sp.legLen||46;
  paw(g,4,1,24*LW,sp.hindSock||sp.paw);
  leg(g,16,-LL-4,LL,18*LW,0.07,sp.foreFar,sp.foreSock&&W3(sp.foreSock));
  leg(g,46,-LL-4,LL,19*LW,-0.05,sp.fore,sp.foreSock);
  if(sp.fluff)sp.fluff(g,"sit");
  g.restore();
  headAt(g,sp,S,[sp.headSit[0],sp.headSit[1]+Math.sin(S.t*2.2+0.6)*1.2],S.tilt);
}
function walk(g,S,sp){
  const ph=S.walk*TAU,hop=sp.hop,sw=hop?Math.sin(ph):Math.sin(ph);
  const bob=hop?-Math.max(0,Math.sin(ph))*16:Math.abs(Math.sin(ph))*-5;
  const lr=typeof sp.short==="number"?sp.short:(sp.short?0.72:0.82);
  g.save();g.translate(0,bob);if(hop)g.rotate(-Math.sin(ph)*0.06);g.scale((sp.slim||1)*(sp.chub||1.08),1.03);
  sp.tail(g,"walk",S);
  const a1=hop?sw*0.5:sw*0.42,a2=hop?-sw*0.6:-sw*0.42;
  g.save();g.translate(0,(1-lr)*60);
  const LW=sp.legW||1.3;
  leg2(g,-40,-66,56*lr,18*LW,a1,Math.max(0,-Math.cos(ph))*0.55,sp.hindFar,sp.hindSock&&WHITEF);
  leg2(g,40,-62,54*lr,16*LW,a2,Math.max(0,Math.cos(ph))*-0.5,sp.foreFar,sp.foreSock&&W3(sp.foreSock));
  g.restore();
  g.save();g.translate(0,(1-lr)*60);fillPath(g,g=>g.ellipse(-46,-74,26,30,0.25,0,TAU),shade(g,-46,-80,30,sp.haunch));g.restore();
  const body=g=>{g.moveTo(-68,-66);g.bezierCurveTo(-74,-104,-38,-118,4,-116);g.bezierCurveTo(44,-114,72,-104,72,-76);g.bezierCurveTo(72,-52,56,-42,24,-42);g.lineTo(-34,-42);g.bezierCurveTo(-58,-42,-66,-50,-68,-66)};
  g.save();g.translate(0,(1-lr)*60);
  fillPath(g,body,shade(g,-4,-92,84,sp.coat));strands(g,body,-6,-86,62,30,70,sp.sheen,17);rim(g,body,6,0.22);
  if(sp.marks)sp.marks(g,"walk");
  if(sp.bib!==sp.coat){const chest=g=>{g.moveTo(36,-112);g.bezierCurveTo(66,-110,78,-88,72,-66);g.bezierCurveTo(66,-48,48,-42,24,-42);g.bezierCurveTo(2,-42,-22,-44,-34,-46);g.bezierCurveTo(-8,-56,20,-72,36,-112)};
    fillPath(g,chest,shade(g,42,-78,50,sp.bib));tufts(g,-28,-45,46,-43,8,7,sp.bib[1],[0,1])}
  blob(g,60,-104,30,10,0.35);
  if(sp.fluff)sp.fluff(g,"walk");
  leg2(g,-48,-62,58*lr,20*LW,hop?a1:-sw*0.48,Math.max(0,Math.cos(ph))*0.6,sp.hind,sp.hindSock);
  leg2(g,48,-58,56*lr,18*LW,hop?a2:sw*0.48,Math.max(0,-Math.cos(ph))*-0.55,sp.fore,sp.foreSock);
  g.restore();
  g.restore();
  headAt(g,sp,S,[sp.headWalk[0],sp.headWalk[1]+(1-lr)*60+bob+Math.sin(ph*2)*1.5],S.tilt*0.5-0.05);
}
function sleep(g,S,sp){
  const br=Math.sin(S.t*1.4)*0.025;g.save();g.scale(1+br*0.3,1-br);
  const body=g=>{g.moveTo(-90,0);g.bezierCurveTo(-100,-56,-52,-88,2,-88);g.bezierCurveTo(54,-88,90,-64,92,-32);g.bezierCurveTo(94,-8,76,0,50,0);g.lineTo(-90,0)};
  fillPath(g,body,shade(g,-6,-46,90,sp.coat));strands(g,body,-6,-44,80,38,80,sp.sheen,19);
  if(sp.marks){g.save();path(g,body);g.clip();sp.marks(g,"sleep");g.restore()}
  rim(g,body,6,0.22);
  sp.tail(g,"sleep",S);if(sp.fluff)sp.fluff(g,"sleep");
  paw(g,40,2,30*(sp.legW||1),sp.paw);paw(g,64,2,28*(sp.legW||1),sp.paw);
  g.restore();
  g.save();g.translate(sp.headSleep[0],sp.headSleep[1]+Math.sin(S.t*1.4)*1.5);g.rotate(0.18);g.scale(0.92,0.92);
  sp.head(g,Object.assign({},S,{blink:1,happy:0,tongue:0,earL:-0.9,earR:1.0,brow:0}));g.restore();
}


// ───────────────────────── what a host uses ─────────────────────────
const PET_KINDS=["puppy","kitten","bunny","fox","panda"];
function petState(kind){return{kind:P[kind]?kind:"puppy",t:0,mode:"sit",face:1,wag:0,tilt:0,blink:0,happy:0,tongue:0,earL:0,earR:0,
  chew:0,walk:0,brow:0,nextBlink:2,nextTilt:4,petUntil:0,feedUntil:0,hop:0,fx:[],earKick:-9}}
function petStep(S,dt){
  S.t+=dt;
  if(S.t>S.nextBlink){S.blinkT=0;S.nextBlink=S.t+2.5+Math.random()*3.5}
  if(S.blinkT!=null){S.blinkT+=dt;const b=S.blinkT/0.16;S.blink=b<1?Math.sin(Math.PI*b):0;if(b>=1)S.blinkT=null}
  if(S.t>S.nextTilt&&S.mode==="sit"){S.tiltT=0;S.tiltTo=(Math.random()<0.5?-1:1)*0.16;S.nextTilt=S.t+5+Math.random()*5}
  if(S.tiltT!=null){S.tiltT+=dt;const k=S.tiltT/2.4;S.tilt=S.tiltTo*Math.sin(Math.PI*Math.min(1,k));if(k>=1){S.tiltT=null;S.tilt=0}}
  const petting=S.t<S.petUntil,eating=S.t<S.feedUntil,moving=S.mode==="walk"||S.mode==="zoom";
  S.happy+=((petting?1:0)-S.happy)*Math.min(1,dt*8);
  S.tongue+=(((petting||(moving&&S.kind==="puppy"))?1:0)-S.tongue)*Math.min(1,dt*5);
  S.chew=eating?Math.abs(Math.sin(S.t*14)):0;
  S.brow=Math.min(1,Math.abs(S.tilt)/0.16)*0.9+S.happy*0.5;
  const wagSpeed=petting?18:(S.mode==="zoom"?16:S.mode==="sleep"?0:6),wagAmp=petting?0.35:(S.mode==="sleep"?0:0.16);
  S.wag=Math.sin(S.t*wagSpeed)*wagAmp*(S.kind==="kitten"||S.kind==="fox"?0.5:1);
  S.earR=moving?Math.sin(S.walk*TAU*2)*0.08:spring(S.t-S.earKick,14,4)*0.25;S.earL=S.earR*0.7;
  if(moving)S.walk+=dt*(S.mode==="zoom"?3.4:(P[S.kind].hop?1.4:1.8));
  if(S.hop>0)S.hop=Math.max(0,S.hop-dt);
  S.fx=S.fx.filter(f=>(f.age+=dt)<1.6);
}
function petPet(S){if(S.mode==="sleep")S.mode="sit";S.petUntil=S.t+1.8;S.hop=0.55;S.earKick=S.t;
  S.fx.push({ch:"\u2764\ufe0f",dx:-0.12,age:0},{ch:"\u2764\ufe0f",dx:0.14,age:-0.25})}
function petFeed(S){if(S.mode==="sleep")S.mode="sit";S.feedUntil=S.t+1.8;S.fx.push({ch:P[S.kind].food,dx:0.3,age:0})}
// draw the pet, with a soft ground shadow and a faint light outline (so a dark pet shows on a dark desktop)
function petDraw(cv,S,cx,gy,size,dpr){
  const g=cv.getContext("2d"),W=cv.width/dpr,H=cv.height/dpr,k=size/260;
  const L=cv._layers||(cv._layers=[document.createElement("canvas"),document.createElement("canvas")]);
  for(const c of L)if(c.width!==cv.width||c.height!==cv.height){c.width=cv.width;c.height=cv.height}
  g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,W,H);
  const hopY=S.hop>0?-Math.sin(Math.PI*(1-S.hop/0.55))*size*0.22:0;
  g.save();g.translate(cx,gy);g.scale(1,0.22);
  const sr=(S.mode==="sleep"?100:S.mode==="walk"||S.mode==="zoom"?80:62)*k*(1+hopY/size*0.8);
  const sg=g.createRadialGradient(0,0,0,0,0,sr);sg.addColorStop(0,"rgba(0,0,0,.34)");sg.addColorStop(1,"rgba(0,0,0,0)");
  g.fillStyle=sg;g.beginPath();g.arc(0,0,sr,0,TAU);g.fill();g.restore();
  const lc=L[0].getContext("2d");lc.setTransform(dpr,0,0,dpr,0,0);lc.clearRect(0,0,W,H);
  lc.save();lc.translate(cx,gy+hopY);lc.scale(k*S.face,k);
  if(S.hop>0){const q=Math.sin(Math.PI*(1-S.hop/0.55));lc.scale(1-q*0.06,1+q*0.08)}
  ({sit,walk,sleep,zoom:walk})[S.mode](lc,S,P[S.kind]);lc.restore();
  const oc=L[1].getContext("2d");oc.setTransform(1,0,0,1,0,0);oc.clearRect(0,0,cv.width,cv.height);oc.drawImage(L[0],0,0);
  oc.globalCompositeOperation="source-in";oc.fillStyle="rgba(235,242,255,.32)";oc.fillRect(0,0,cv.width,cv.height);oc.globalCompositeOperation="source-over";
  g.save();g.setTransform(1,0,0,1,0,0);const o=1.3*dpr;for(let a=0;a<8;a++)g.drawImage(L[1],Math.cos(a/8*TAU)*o,Math.sin(a/8*TAU)*o);g.drawImage(L[0],0,0);g.restore();
  g.setTransform(dpr,0,0,dpr,0,0);
  for(const f of S.fx){if(f.age<0)continue;g.save();g.globalAlpha=Math.max(0,1-f.age/1.6);g.font=Math.round(22*Math.max(0.7,k*1.4))+"px serif";g.textAlign="center";
    g.fillText(f.ch,cx+f.dx*size,gy-size*1.0-f.age*40);g.restore()}
  if(S.mode==="sleep"){g.save();g.fillStyle="rgba(235,240,250,.95)";g.strokeStyle="rgba(40,50,70,.5)";g.lineWidth=2;g.font="bold "+Math.round(16*Math.max(0.8,k*1.3))+"px sans-serif";
    const z=(S.t%2.6)/2.6,z2=(z+0.5)%1;
    g.globalAlpha=Math.sin(Math.PI*z);g.strokeText("z",cx+size*0.4*S.face+z*14,gy-size*0.55-z*26);g.fillText("z",cx+size*0.4*S.face+z*14,gy-size*0.55-z*26);
    g.globalAlpha=Math.sin(Math.PI*z2);g.strokeText("Z",cx+size*0.5*S.face+z2*16,gy-size*0.62-z2*30);g.fillText("Z",cx+size*0.5*S.face+z2*16,gy-size*0.62-z2*30);g.restore()}
}

// <<< pets.js
const KIND=['kitten','puppy','bunny','fox','panda'];        // the board's kind numbers -> the drawings
const FOOD={kitten:'🐟',puppy:'🦴',bunny:'🥕',fox:'🍗',panda:'🎋'};
let last=null,kind=1,os='sh',evSeen=null,busy=false,walkUntil=0,sizing=0,szT=null;

// the pet, alive: the same drawing and animation as on the computer screen (pets.js)
function fitC(cv,w,h){const r=window.devicePixelRatio||1;cv.width=Math.round(w*r);cv.height=Math.round(h*r);cv.style.width=w+'px';cv.style.height=h+'px';cv._dpr=r}
fitC($('big'),300,200);[0,1,2,3,4].forEach(i=>fitC($('kc'+i),112,84));
const big=petState('puppy'),picks=[0,1,2,3,4].map(i=>petState(KIND[i]));
let lastT=performance.now(),pickT=0;
function loop(now){
  const dt=Math.min(0.05,(now-lastT)/1000);lastT=now;
  if(last&&last.pet){const k=KIND[last.pet.k]||'puppy';if(big.kind!==k)Object.assign(big,petState(k));
    const m=last.pet.sleep?'sleep':(now<walkUntil?'walk':'sit');if(big.mode!==m)big.mode=m;
    petStep(big,dt);petDraw($('big'),big,150,192,150,$('big')._dpr)}
  if($('adopt').style.display!=='none'&&now-pickT>70){pickT=now;
    picks.forEach((S,i)=>{petStep(S,0.07);petDraw($('kc'+i),S,58,80,62,$('kc'+i)._dpr)})}
  requestAnimationFrame(loop)}
requestAnimationFrame(loop);
document.querySelectorAll('.kind').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.kind').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');kind=+b.dataset.k});

const MOODS={happy:'is happy',content:'is content',hungry:'is hungry',tired:'is tired',lonely:'is lonely',sleeping:'is asleep'};
const WHY={asleep:'asleep — wake it up first',full:'not hungry right now',tired:'too tired to play',nopet:'no pet yet',offline:'that computer is offline'};

function event(k){
  if(k==='feed')petFeed(big);
  else if(k==='pet'||k==='poke'||k==='adopt')petPet(big);
  else if(k==='play')walkUntil=performance.now()+1800;
}
function bar(id,v){const c=v<25?'#ef4444':v<50?'#f59e0b':'#22c55e';$('b'+id).style.width=v+'%';$('b'+id).style.background=c;$('v'+id).textContent=v}

function draw(d){
  last=d;
  const p=d.pet;
  $('adopt').style.display=p?'none':'block';
  $('home').style.display=p?'block':'none';
  if(!p){$('sub').textContent='A pet that lives on the WiFi';return}
  const k=KIND[p.k]||'puppy';
  if(Date.now()>sizing){$('sz').value=p.sz||120;$('szv').textContent=sizeWord(p.sz||120)}
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
