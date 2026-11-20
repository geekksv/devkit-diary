#pragma once
// The page: the lobby, the Ludo board drawn from the board's state, a player panel with a die at
// each player's side, reaction emojis, the turn clock, and a spectator view.

static const char LUDO_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><title>Ludo</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
:root{--bg:#0d1117;--card:#161b24;--line:#2a3140;--ink:#eef1f6;--mute:#8b95a7;--faint:#5b6476;--acc:#3b82f6;color-scheme:dark}
body{background:var(--bg);color:var(--ink);min-height:100vh;overflow-x:hidden;-webkit-font-smoothing:antialiased;
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Inter,system-ui,sans-serif;
  padding:max(12px,env(safe-area-inset-top)) 12px calc(16px + env(safe-area-inset-bottom))}
.wrap{max-width:440px;margin:0 auto}
body.game .wrap{max-width:clamp(320px,calc(100vh - 310px),620px);max-width:clamp(320px,calc(100dvh - 310px),620px)}
header{display:flex;align-items:center;gap:12px;margin:6px 0 18px}
.logo{width:30px;height:30px;border-radius:8px;overflow:hidden;display:grid;grid-template-columns:1fr 1fr;flex:none}
.logo i:nth-child(1){background:#e5483d}.logo i:nth-child(2){background:#22a45d}
.logo i:nth-child(3){background:#2f7cf6}.logo i:nth-child(4){background:#f2b705}
h1{font-size:20px;font-weight:700;letter-spacing:-.02em;line-height:1.2}
.sub{color:var(--mute);font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px}
.label{color:var(--mute);font-size:12px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;margin:0 0 10px}
input{width:100%;font:inherit;color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:12px 14px;outline:0}
input:focus{border-color:var(--acc)}
button{font:inherit;font-weight:600;cursor:pointer;width:100%;border:1px solid transparent;border-radius:10px;padding:12px 16px;background:var(--acc);color:#fff}
button:active{transform:translateY(1px)}button:disabled{opacity:.5}
button.ghost{background:transparent;border-color:var(--line);color:var(--ink)}
.row{display:flex;gap:8px}.row>*{flex:1}.narrow{flex:0 0 96px}
.err{color:#f87171;font-size:13px;min-height:18px;margin:8px 0 0}
#lobby,#game{display:none}body.lobby #lobby,body.game #game{display:block}body.game header{display:none}
.modes{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px 0 14px}
.mode{background:var(--bg);border-color:var(--line);color:var(--ink);text-align:left;padding:12px}
.mode b{display:block;font-size:14px}.mode span{font-size:12px;color:var(--mute);font-weight:400}
.mode.sel{border-color:var(--acc);box-shadow:inset 0 0 0 1px var(--acc)}
.open{display:flex;align-items:center;gap:10px;padding:10px 0;border-top:1px solid var(--line)}
.open .code{font-weight:700;letter-spacing:.12em}.open .who{flex:1;color:var(--mute);font-size:13px}
.open button{width:auto;padding:8px 14px}
#codein{text-transform:uppercase;letter-spacing:.3em;font-weight:700;text-align:center}

/* game */
.top{display:flex;align-items:center;gap:8px;margin:0 0 10px;font-size:13px;color:var(--mute)}
.top b{color:var(--ink);letter-spacing:.14em}
.top .ghost{margin-left:auto;width:auto;padding:6px 12px;font-size:13px}
.pl{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.pl.up{margin:0 0 8px}.pl.dn{margin:8px 0 0}
.p{--c:#888;position:relative;display:flex;align-items:center;gap:10px;min-width:0;overflow:hidden;
  background:var(--card);border:1px solid var(--line);border-radius:12px;padding:7px 8px;cursor:default}
.p.r{flex-direction:row-reverse;text-align:right}
.p .av{width:30px;height:30px;border-radius:50%;flex:none;display:grid;place-items:center;font-weight:700;font-size:13px;color:#fff;background:var(--c)}
.p .tx{flex:1;min-width:0}
.p .nm{font-weight:600;font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.p .st{font-size:12px;color:var(--mute);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.p.turn{border-color:var(--c);box-shadow:inset 0 0 0 1px var(--c)}
.p.win{border-color:#f2b705;box-shadow:inset 0 0 0 1px #f2b705}
.p.empty .av,.p.out .av{background:var(--line)}.p.empty,.p.out{opacity:.5}
.p .bar{position:absolute;left:0;bottom:0;height:4px;width:0;background:#e2e8f0;transition:width .1s linear}
.p .bar.mid{background:#f59e0b}.p .bar.low{background:#ef4444}
.p .sec{font-weight:700;color:#e2e8f0;font-variant-numeric:tabular-nums}
.p .sec.mid{color:#f59e0b}.p .sec.low{color:#ef4444}
.p .em{position:absolute;top:-2px;font-size:18px;animation:pop .3s ease}
.p .em{right:52px}.p.r .em{right:auto;left:52px}
.die{width:40px;height:40px;flex:none;position:relative;border-radius:10px;background:#fff;visibility:hidden;
  box-shadow:inset 0 -3px 0 rgba(0,0,0,.12)}
.die.show{visibility:visible}
.die.can{cursor:pointer;box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--c);animation:breathe 1.3s ease-in-out infinite}
.die.spin{animation:roll .42s ease-out}
.die i{position:absolute;width:18%;height:18%;border-radius:50%;background:#151a23;transform:translate(-50%,-50%)}
@keyframes breathe{50%{transform:scale(1.08)}}
@keyframes roll{0%{transform:rotate(0) scale(1)}50%{transform:rotate(200deg) scale(.82)}100%{transform:rotate(360deg) scale(1)}}
@keyframes pop{from{transform:scale(0)}to{transform:scale(1)}}

#boardwrap{position:relative}
#board{display:block;width:100%;height:auto;border-radius:14px;background:#fff;touch-action:manipulation;
  box-shadow:0 12px 32px -12px rgba(0,0,0,.7)}
#board .tk{cursor:pointer;transition:transform .13s ease-out}
#board .mark{cursor:pointer}
#board.noanim .tk{transition:none}
#board .tk .halo{opacity:0}
#board .tk.mv .halo{opacity:1;animation:halo 1s ease-in-out infinite}
@keyframes halo{50%{opacity:.25}}
.bubble{position:absolute;font-size:30px;pointer-events:none;z-index:5;animation:pop .3s ease,rise 1.8s ease forwards}
@keyframes rise{to{transform:translateY(-36px);opacity:0}}

.status{text-align:center;margin:14px 0 0;min-height:42px}
.status .big{font-size:16px;font-weight:600}
.status .small{font-size:13px;color:var(--mute);margin-top:2px}
#startb,#again{display:none;margin:12px auto 0;max-width:240px}
.emobar{display:flex;gap:4px;flex-wrap:wrap;justify-content:center;margin:12px 0 0}
.emobar button{width:auto;background:transparent;border-color:var(--line);border-radius:999px;padding:5px 9px;font-size:17px;line-height:1.2}
.rule{text-align:center;color:var(--faint);font-size:12px;margin:14px auto 0;max-width:420px}
@media (max-width:420px){.p{gap:7px;padding:6px 7px}.p .av{width:24px;height:24px;font-size:11px}
  .die{width:34px;height:34px;border-radius:8px}.p .nm{font-size:13px}.p .st{font-size:11.5px}
  .p .em{right:44px}.p.r .em{left:44px}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head>
<body class="lobby"><div class="wrap">

<header><span class="logo"><i></i><i></i><i></i><i></i></span>
  <div><h1>Ludo</h1><p class="sub" id="sub">Two to four players, refereed by the board</p></div></header>

<div id="lobby">
  <div class="card">
    <p class="label">New game</p>
    <input id="nm" placeholder="Your name" maxlength="12" autocomplete="off">
    <div class="modes">
      <button class="mode sel" data-m="0" type="button"><b>Classic</b><span>Four tokens each</span></button>
      <button class="mode" data-m="1" type="button"><b>Quick</b><span>Two tokens, a short game</span></button>
    </div>
    <button id="mk">Create a room</button>
    <p class="err" id="mkerr"></p>
  </div>
  <div class="card" style="margin-top:12px">
    <p class="label">Join a game</p>
    <div class="row"><input id="codein" placeholder="CODE" maxlength="4" autocomplete="off">
      <button class="narrow" id="jb">Join</button></div>
    <div class="row" style="margin-top:8px"><button class="ghost" id="watchb">Watch a game</button></div>
    <p class="err" id="jerr"></p>
    <div id="opens"></div>
  </div>
</div>

<div id="game">
  <div class="top">Room <b id="code">----</b><span id="modepill">Classic</span>
    <span id="watchpill" style="display:none">· watching</span>
    <button class="ghost" onclick="leave()">Leave</button></div>
  <div class="pl up"><div class="p" id="p0"></div><div class="p r" id="p1"></div></div>
  <div id="boardwrap"><svg id="board" viewBox="-0.25 -0.25 15.5 15.5"></svg></div>
  <div class="pl dn"><div class="p" id="p3"></div><div class="p r" id="p2"></div></div>
  <div class="status"><div class="big" id="pbig">&nbsp;</div><div class="small" id="psmall"></div>
    <button id="startb" onclick="start()">Start game</button></div>
  <div class="emobar" id="emobar"></div>
  <button id="again" onclick="again()">Play again</button>
  <p class="rule">Roll a 6 to bring a token out. Land on an opponent to send it back. Stars are safe.</p>
</div>

<script>
const $=i=>document.getElementById(i);
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());
const get=u=>fetch(u,{cache:'no-store'}).then(r=>r.json());
const enc=encodeURIComponent;
const NS='http://www.w3.org/2000/svg';
const COL=['#e5483d','#22a45d','#f2b705','#2f7cf6'];
const DK=['#a8281f','#137240','#b58700','#1a54b8'];
const TINT=['#fbd9d6','#d2eedd','#fcefbe','#d3e3fd'];
const EMO=['😂','🔥','😮','😎','😭','👏','🎲','💀'];
let code='',token='',mode=0,me=-1,timer=null,last=null,busy=false,lastEmojiAt=0;
let lastRid=-1,primed=false,rollHoldSeat=-1,rollHoldUntil=0,lastFace=[0,0,0,0],prevProg=null,note='',noteUntil=0,wasMyTurn=false,marks=null;
const cap=s=>s.charAt(0).toUpperCase()+s.slice(1);
const mk=(t,c)=>{const e=document.createElement(t);if(c)e.className=c;return e};

// The board, on a 15x15 grid of [column,row] cells. The ring is walked clockwise from red's start.
const RING=[[1,6],[2,6],[3,6],[4,6],[5,6],[6,5],[6,4],[6,3],[6,2],[6,1],[6,0],[7,0],[8,0],
 [8,1],[8,2],[8,3],[8,4],[8,5],[9,6],[10,6],[11,6],[12,6],[13,6],[14,6],[14,7],[14,8],
 [13,8],[12,8],[11,8],[10,8],[9,8],[8,9],[8,10],[8,11],[8,12],[8,13],[8,14],[7,14],[6,14],
 [6,13],[6,12],[6,11],[6,10],[6,9],[5,8],[4,8],[3,8],[2,8],[1,8],[0,8],[0,7],[0,6]];
const START=[0,13,26,39];
const SAFE=[0,8,13,21,26,34,39,47];
const HOME=[[[1,7],[2,7],[3,7],[4,7],[5,7]],[[7,1],[7,2],[7,3],[7,4],[7,5]],
            [[13,7],[12,7],[11,7],[10,7],[9,7]],[[7,13],[7,12],[7,11],[7,10],[7,9]]];
const BASE=[[[2,2],[4,2],[2,4],[4,4]],[[11,2],[13,2],[11,4],[13,4]],
            [[11,11],[13,11],[11,13],[13,13]],[[2,11],[4,11],[2,13],[4,13]]];
const el=(t,a)=>{const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e};
function cellCenter(seat,prog){
  if(prog>=1&&prog<=51){const r=RING[(START[seat]+prog-1)%52];return [r[0]+0.5,r[1]+0.5]}
  if(prog>=52&&prog<=56){const h=HOME[seat][prog-52];return [h[0]+0.5,h[1]+0.5]}
  return [7.5,7.5];
}
// a finished token sits in its colour's triangle of the centre, lined up along the triangle's base
function finishSpot(seat,i,n){
  const o=(i-(n-1)/2)*0.42;
  return [[6.45,7.5+o],[7.5+o,6.45],[8.55,7.5+o],[7.5+o,8.55]][seat];
}
function star(cx,cy,fill){
  let p='';for(let i=0;i<10;i++){const a=-Math.PI/2+i*Math.PI/5,r=i%2?0.14:0.33;p+=(cx+r*Math.cos(a)).toFixed(3)+','+(cy+r*Math.sin(a)).toFixed(3)+' '}
  return el('polygon',{points:p,fill});
}

let tokenEls=[];
function buildBoard(){
  const svg=$('board');svg.innerHTML='';
  svg.appendChild(el('rect',{x:-0.25,y:-0.25,width:15.5,height:15.5,fill:'#ffffff'}));
  const corners=[[0,0],[9,0],[9,9],[0,9]];
  for(let s=0;s<4;s++){const[c,r]=corners[s];
    svg.appendChild(el('rect',{x:c,y:r,width:6,height:6,rx:0.35,fill:COL[s]}));
    svg.appendChild(el('rect',{x:c+0.9,y:r+0.9,width:4.2,height:4.2,rx:0.5,fill:'#ffffff'}));
    for(const[px,py]of BASE[s])svg.appendChild(el('circle',{cx:px,cy:py,r:0.5,fill:TINT[s]}));
  }
  for(let i=0;i<52;i++){const[x,y]=RING[i];const owner=START.indexOf(i);
    svg.appendChild(el('rect',{x,y,width:1,height:1,fill:owner>=0?COL[owner]:'#ffffff',stroke:'#d5dae3','stroke-width':0.035}));
    if(SAFE.includes(i))svg.appendChild(star(x+0.5,y+0.5,owner>=0?'#ffffff':'#a3abb9'));
  }
  for(let s=0;s<4;s++)for(const[x,y]of HOME[s])
    svg.appendChild(el('rect',{x,y,width:1,height:1,fill:COL[s],stroke:'#ffffff','stroke-width':0.05}));
  // the square where each colour turns off the ring into its own home column
  const arrows=['0.27,7.2 0.27,7.8 0.8,7.5','7.2,0.27 7.8,0.27 7.5,0.8','14.73,7.2 14.73,7.8 14.2,7.5','7.2,14.73 7.8,14.73 7.5,14.2'];
  arrows.forEach((pts,s)=>svg.appendChild(el('polygon',{points:pts,fill:COL[s]})));
  // the centre: one triangle per colour, meeting in the middle of the 3x3 square
  const tri=['7.5,7.5 6,6 6,9','7.5,7.5 6,6 9,6','7.5,7.5 9,6 9,9','7.5,7.5 6,9 9,9'];
  tri.forEach((pts,s)=>svg.appendChild(el('polygon',{points:pts,fill:COL[s],stroke:'#ffffff','stroke-width':0.04})));
  marks=el('g',{});svg.appendChild(marks);
  tokenEls=[];
  for(let s=0;s<4;s++){tokenEls[s]=[];for(let t=0;t<4;t++){
    const g=el('g',{class:'tk'});
    g.append(el('ellipse',{cx:0,cy:0.16,rx:0.34,ry:0.14,fill:'rgba(0,0,0,.22)'}),
             el('circle',{class:'halo',r:0.5,fill:'none',stroke:'#111827','stroke-width':0.09}),
             el('circle',{r:0.38,fill:COL[s],stroke:DK[s],'stroke-width':0.05}),
             el('circle',{r:0.3,fill:'none',stroke:'#ffffff','stroke-width':0.07}),
             el('circle',{r:0.11,fill:'#ffffff'}),
             el('circle',{r:0.55,fill:'transparent'}));
    g.addEventListener('click',()=>tapToken(s,t));
    svg.appendChild(g);tokenEls[s][t]={g,tm:[],until:0};
  }}
}
function tapToken(s,t){
  if(busy||me<0||!last||last.phase!==1||last.turn!==me||last.gphase!==2)return;
  if(s!==me||!(last.movable||[]).includes(t))return;
  move(t);
}
const tf=(te,x,y,sc)=>{te.g.style.transform='translate('+x+'px,'+y+'px) scale('+sc+')'};
const OFF={2:[[-0.2,0],[0.2,0]],3:[[-0.2,-0.18],[0.2,-0.18],[0,0.2]],4:[[-0.2,-0.2],[0.2,-0.2],[-0.2,0.2],[0.2,0.2]]};
function place(d){
  const svg=$('board'),first=!prevProg;
  if(first)svg.classList.add('noanim');
  const groups={},items=[];marks.innerHTML='';
  for(let s=0;s<4;s++)for(let t=0;t<4;t++){
    const te=tokenEls[s][t];
    if(!d.here[s]||t>=d.mode){te.g.style.display='none';continue}   // empty seats show no tokens
    te.g.style.display='';
    const prog=d.prog[s][t];
    const key=prog===0?'':prog<=51?'r'+((START[s]+prog-1)%52):prog<=56?'h'+s+'.'+prog:'c'+s;
    const it={s,t,prog,key};items.push(it);
    if(key)(groups[key]=groups[key]||[]).push(it);
  }
  const movable=[];
  for(const it of items){
    const{s,t,prog,key}=it,te=tokenEls[s][t];
    let x,y,sc=1;
    if(prog===0){[x,y]=BASE[s][t]}
    else if(prog===57){const g=groups[key];[x,y]=finishSpot(s,g.indexOf(it),d.mode);sc=0.55}
    else{[x,y]=cellCenter(s,prog);const g=groups[key];
      if(g.length>1){const o=OFF[Math.min(g.length,4)][Math.min(g.indexOf(it),3)];x+=o[0];y+=o[1];sc=0.66}
      else if(prog>=52)sc=0.9}
    // a token that moved forward along the track hops cell by cell
    const old=prevProg?prevProg[s][t]:prog;
    if(old!==prog&&old>=1&&prog>old&&prog-old<=6){
      te.tm.forEach(clearTimeout);te.tm=[];
      const hops=[];for(let p=old+1;p<prog;p++)hops.push(p);
      hops.forEach((p,i)=>te.tm.push(setTimeout(()=>{const c=cellCenter(s,p);tf(te,c[0],c[1],p>=52?0.9:1)},i*130)));
      te.tm.push(setTimeout(()=>tf(te,x,y,sc),hops.length*130));
      te.until=Date.now()+hops.length*130+60;
    }else if(Date.now()>te.until){te.tm.forEach(clearTimeout);te.tm=[];tf(te,x,y,sc)}
    const mv=me===s&&d.phase===1&&d.turn===me&&d.gphase===2&&(d.movable||[]).includes(t);
    te.g.setAttribute('class',mv?'tk mv':'tk');
    if(mv)movable.push(te.g);
    if(mv){                                        // where this token would land
      const to=prog===0?1:prog+d.die;
      const[mx,my]=to===57?finishSpot(s,0,1):cellCenter(s,to);
      const m=el('circle',{class:'mark',cx:mx,cy:my,r:0.2,fill:'#ffffff',stroke:COL[s],'stroke-width':0.09});
      m.addEventListener('click',()=>tapToken(s,t));marks.appendChild(m);
    }
  }
  movable.forEach(g=>svg.appendChild(g));          // tokens you can move sit on top
  prevProg=d.prog.map(a=>a.slice());
  if(first)requestAnimationFrame(()=>requestAnimationFrame(()=>svg.classList.remove('noanim')));
}

// a die face drawn with pips as percentages, so it scales on any screen
const PIP={1:[[50,50]],2:[[27,27],[73,73]],3:[[27,27],[50,50],[73,73]],
  4:[[27,27],[73,27],[27,73],[73,73]],5:[[27,27],[73,27],[50,50],[27,73],[73,73]],
  6:[[27,25],[73,25],[27,50],[73,50],[27,75],[73,75]]};
function setDie(e,v){e.innerHTML='';for(const[x,y]of(PIP[v]||[])){const i=mk('i');i.style.left=x+'%';i.style.top=y+'%';e.appendChild(i)}}

const panels=[0,1,2,3].map(s=>{
  const root=$('p'+s),av=mk('div','av'),tx=mk('div','tx'),nm=mk('div','nm'),st=mk('div','st'),die=mk('div','die'),bar=mk('div','bar');
  const stt=mk('span'),sec=mk('span','sec');st.append(stt,sec);
  tx.append(nm,st);root.append(av,tx,die,bar);root.style.setProperty('--c',COL[s]);
  root.onclick=()=>{if(last&&last.phase===1&&last.turn===me&&last.gphase===1&&s===me)roll()};
  return{root,av,nm,st:stt,sec,die,bar,em:null};
});
function renderPanels(d){
  for(let s=0;s<4;s++){
    const P=panels[s],here=!!d.here[s];
    const home=d.prog[s].filter((p,i)=>i<d.mode&&p===57).length;
    P.root.className='p'+(s===1||s===2?' r':'')+(d.phase===1&&d.turn===s&&here?' turn':'')
      +(d.phase===2&&d.winner===s?' win':'')+(!here?(d.phase>0?' out':' empty'):'');
    P.av.textContent=here?(d.names[s][0]||'?').toUpperCase():'';
    P.nm.textContent=here?(s===me?'You':d.names[s]):'Open seat';
    let st;
    if(!here)st=d.phase>0?'Not playing':'Waiting for a player';
    else if(d.phase===2&&d.winner===s)st='Winner';
    else if(d.phase>0)st=home+'/'+d.mode+' home';
    else st=s===d.host?'Host':'Ready';
    if(here&&d.phase===0&&d.wins[s])st+=' · '+d.wins[s]+(d.wins[s]>1?' wins':' win');
    P.st.textContent=st;
    const want=d.emoji&&d.emoji.s===s?EMO[d.emoji.e]:'';
    if(want&&(!P.em||P.em.textContent!==want)){if(P.em)P.em.remove();P.em=mk('span','em');P.em.textContent=want;P.root.appendChild(P.em)}
    if(!want&&P.em){P.em.remove();P.em=null}
  }
}
// spin a player's die for a moment, then settle on the rolled number
function animateDie(s,val){
  const e=panels[s].die;let n=0;clearInterval(e._iv);
  e.classList.remove('spin');void e.offsetWidth;e.classList.add('spin','show');
  e._iv=setInterval(()=>{setDie(e,1+Math.floor(Math.random()*6));
    if(++n>=7){clearInterval(e._iv);e._iv=0;e.classList.remove('spin');lastFace[s]=val;setDie(e,val)}},60);
}
// only the player whose turn it is shows a die (plus a short linger after a roll that had no move)
function renderDice(d){
  const now=Date.now();
  for(let s=0;s<4;s++){
    const e=panels[s].die,spinning=!!e._iv;let show=false,can=false;
    if(d.phase===1&&s===d.turn&&d.here[s]){
      show=true;
      if(!spinning)setDie(e,d.gphase===2&&d.die?d.die:(lastFace[s]||6));
      can=d.gphase===1&&s===me;
    }
    if(s===rollHoldSeat&&now<rollHoldUntil)show=true;
    e.classList.toggle('show',show||spinning);
    e.classList.toggle('can',can);
  }
}

function bubble(seat,e){
  const b=mk('div','bubble');b.textContent=EMO[e];
  const pos=[[6,4],[80,4],[80,80],[6,80]][seat];
  b.style.left=pos[0]+'%';b.style.top=pos[1]+'%';$('boardwrap').appendChild(b);setTimeout(()=>b.remove(),2000);
}

function draw(d){
  last=d;me=d.me;
  $('code').textContent=d.code;$('modepill').textContent=d.mode===2?'Quick':'Classic';
  $('watchpill').style.display=me<0?'inline':'none';
  const name=s=>s===me?'you':(d.names[s]||'a player');
  renderPanels(d);
  if(prevProg&&d.phase===1&&d.last&&d.last.caps>0&&d.last.s>=0)
    for(let o=0;o<4;o++)for(let t=0;t<d.mode;t++)
      if(prevProg[o][t]>=1&&prevProg[o][t]<=51&&d.prog[o][t]===0){
        const whose=o===me?'your':(d.names[o]||'a')+"'s";
        note=cap(name(d.last.s))+' sent '+whose+' token home';noteUntil=Date.now()+2600}
  place(d);
  if(d.emoji&&d.emoji.at!==lastEmojiAt){lastEmojiAt=d.emoji.at;bubble(d.emoji.s,d.emoji.e)}

  // a new roll from the board: spin that player's die, then settle it
  if(d.rid!==lastRid){
    if(primed&&d.phase===1&&d.rs>=0&&d.rv>0){
      animateDie(d.rs,d.rv);
      rollHoldSeat=d.rs;rollHoldUntil=Date.now()+1500;
      setTimeout(()=>{if(last)renderDice(last)},1560);
      if(d.turn!==d.rs&&!d.auto){note=cap(name(d.rs))+' rolled a '+d.rv+' — no move';noteUntil=Date.now()+2200}
    }
    lastRid=d.rid;primed=true;
  }
  renderDice(d);

  const mine=d.phase===1&&d.turn===me,canRoll=mine&&d.gphase===1;
  let big='',small='';
  if(d.phase===0){
    const n=d.here.filter(Boolean).length;
    big=me===d.host?'Your room is ready':'Waiting to start';
    small=me===d.host?(n<2?'Share the code '+d.code+' — one more player needed':n+' players in — start when everyone is here')
                     :'Waiting for '+d.names[d.host]+' to start';
  }else if(d.phase===2){
    big=d.winner===me?'You won!':cap(name(d.winner))+' won';small=me>=0?'Play again for another round':'';
  }else if(canRoll){big='Your turn';small='Tap your dice to roll'}
  else if(mine){big='You rolled a '+d.die;
    small=d.pend?'Only one move — playing it for you':(d.movable||[]).length>1?'Tap a token, or the dot where it lands':'Tap the token to move it'}
  else{big=cap(name(d.turn))+"'s turn";small=d.gphase===1?'Rolling…':d.pend?'Moving…':'Choosing a move…'}
  if(d.phase===1&&Date.now()<noteUntil)small=note;
  if(d.phase===1&&d.auto)small='Time ran out — the board played the turn';
  $('pbig').textContent=big;$('psmall').textContent=small;
  if(canRoll&&!wasMyTurn&&navigator.vibrate)try{navigator.vibrate(60)}catch(e){}
  wasMyTurn=canRoll;
  document.title=canRoll?'● Your turn · Ludo':'Ludo';

  $('startb').style.display=d.phase===0&&me===d.host&&d.here.filter(Boolean).length>=2?'block':'none';
  $('again').style.display=d.phase===2&&me>=0?'block':'none';
  $('emobar').style.display=me>=0&&d.phase>0?'flex':'none';
  if(d.clock>=0&&d.phase===1){deadline=Date.now()+d.clock;limit=d.limit}else deadline=0;
}

let deadline=0,limit=25000;
setInterval(()=>{
  const left=deadline?Math.max(0,deadline-Date.now()):0;
  panels.forEach((P,s)=>{const on=!!(deadline&&last&&last.phase===1&&last.turn===s);
    const low=on&&left<5000,mid=on&&!low&&left<10000;
    P.bar.style.width=on?(left/limit*100)+'%':'0';
    P.sec.textContent=on?' · '+Math.ceil(left/1000)+'s':'';
    for(const e of[P.bar,P.sec]){e.classList.toggle('low',low);e.classList.toggle('mid',mid)}});
},100);

EMO.forEach((e,i)=>{const b=mk('button');b.textContent=e;
  b.onclick=()=>post('/emoji','c='+enc(code)+'&t='+enc(token)+'&e='+i);$('emobar').appendChild(b)});

document.querySelectorAll('.mode').forEach(b=>b.onclick=()=>{document.querySelectorAll('.mode').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');mode=+b.dataset.m});
function store(){try{localStorage.setItem('ludo',JSON.stringify({code,token,me}))}catch(e){}}
function forget(){try{localStorage.removeItem('ludo')}catch(e){}}
function myName(){const n=$('nm').value.trim();try{localStorage.setItem('ludo_n',n)}catch(e){};return n}
const ERRS={name:'Pick a name first.',full:'All rooms are in use.',nocode:'No room with that code.',busy:'That room is full.',started:'That game has started.',alone:'Need at least two players.'};
function show(v){document.body.className=v}
function reset(){wasMyTurn=false;document.title='Ludo';last=null;lastRid=-1;primed=false;rollHoldSeat=-1;lastFace=[0,0,0,0];prevProg=null;noteUntil=0;deadline=0;
  panels.forEach(P=>{clearInterval(P.die._iv);P.die._iv=0;P.die.className='die'})}
function toLobby(m){clearTimeout(timer);code='';token='';me=-1;reset();forget();show('lobby');$('jerr').textContent=m||'';history.replaceState(null,'','/');lobby()}
function enter(d){code=d.code;token=d.token;me=d.seat;store();reset();buildBoard();history.replaceState(null,'','/?c='+code);show('game');poll()}

$('mk').onclick=async()=>{const n=myName();if(!n){$('mkerr').textContent=ERRS.name;return}
  $('mk').disabled=true;$('mkerr').textContent='';
  try{const d=await post('/create','n='+enc(n)+'&m='+mode);d.ok?enter(d):$('mkerr').textContent=ERRS[d.err]||'Could not create.'}
  catch(e){$('mkerr').textContent='The board did not answer.'}finally{$('mk').disabled=false}};
async function join(c){const n=myName();if(!n){$('jerr').textContent=ERRS.name;return}$('jerr').textContent='';
  try{const d=await post('/join','c='+enc(c)+'&n='+enc(n));d.ok?enter(d):$('jerr').textContent=ERRS[d.err]||'Could not join.'}
  catch(e){$('jerr').textContent='The board did not answer.'}}
$('jb').onclick=()=>{const c=$('codein').value.trim().toUpperCase();if(c.length===4)join(c)};
$('watchb').onclick=async()=>{const c=$('codein').value.trim().toUpperCase();if(c.length!==4){$('jerr').textContent='Type a room code to watch.';return}
  try{const d=await post('/watch','c='+enc(c));d.ok?enter(d):$('jerr').textContent=ERRS[d.err]||'No such room.'}catch(e){$('jerr').textContent='The board did not answer.'}};
async function lobby(){if(document.body.className!=='lobby')return;
  try{const d=await get('/rooms');const box=$('opens');box.innerHTML='';
    if(d.rooms.length){const l=mk('p','label');l.style.marginTop='14px';l.textContent='Open rooms';box.appendChild(l)}
    for(const r of d.rooms){const row=mk('div','open');
      const c=mk('span','code');c.textContent=r.c;
      const w=mk('span','who');w.textContent=r.h+' · '+(r.m===2?'quick':'classic')+' · '+r.n+'/4';
      const b=mk('button');b.textContent='Join';b.onclick=()=>join(r.c);
      row.append(c,w,b);box.appendChild(row)}
  }catch(e){}
  setTimeout(lobby,2500)}

async function start(){try{const d=await post('/start','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function roll(){if(busy)return;busy=true;
  try{const d=await post('/roll','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}busy=false;poll()}
async function move(k){if(busy)return;busy=true;
  try{const d=await post('/move','c='+enc(code)+'&t='+enc(token)+'&k='+k);if(d.ok)draw(d)}catch(e){}busy=false;poll()}
async function again(){try{const d=await post('/again','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function leave(){try{await post('/leave','c='+enc(code)+'&t='+enc(token))}catch(e){};toLobby('')}

async function poll(){clearTimeout(timer);if(!code)return;
  try{const d=await get('/state?c='+enc(code)+'&t='+enc(token));
    if(!d.ok){toLobby('The game ended.');return}
    if(!busy)draw(d);
  }catch(e){$('psmall').textContent='Reconnecting to the board…'}
  timer=setTimeout(poll,600)}

(async function boot(){try{$('nm').value=localStorage.getItem('ludo_n')||''}catch(e){}
  let saved=null;try{saved=JSON.parse(localStorage.getItem('ludo')||'null')}catch(e){}
  if(saved&&saved.code&&saved.token){try{const d=await post('/join','c='+enc(saved.code)+'&t='+enc(saved.token));if(d.ok){enter(d);return}}catch(e){}forget()}
  const q=new URLSearchParams(location.search).get('c');if(q)$('codein').value=q.toUpperCase();
  show('lobby');lobby()})();
</script>
</body></html>
)HTML";
