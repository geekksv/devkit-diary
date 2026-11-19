#pragma once
// The page: the lobby, the Ludo board drawn from the board's state, a big tap-to-roll dice,
// reaction emojis, the turn clock, and a spectator view. Built to make it obvious whose turn it
// is and what to do next.

static const char LUDO_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0f1a"><title>Ludo</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
:root{--bg:#0b0f1a;--card:#141a28;--card2:#1b2334;--line:#28324a;--ink:#f2f5fc;--mute:#9aa6c2;--faint:#5d6a86;
  --p0:#ff5d6c;--p1:#2fd6a6;--p2:#ffb020;--p3:#9d7bff;color-scheme:dark}
html,body{height:100%}
body{overflow-x:hidden;background:radial-gradient(120% 80% at 50% -10%,#141b2e,#0b0f1a);color:var(--ink);
  font:15px/1.5 ui-rounded,-apple-system,"Segoe UI",system-ui,sans-serif;min-height:100dvh;
  padding:max(10px,env(safe-area-inset-top)) 10px calc(14px + env(safe-area-inset-bottom));-webkit-font-smoothing:antialiased}
.wrap{max-width:520px;margin:0 auto}
a{color:var(--p2)}
header{display:flex;align-items:center;gap:10px;margin:2px 0 14px}
.dot{width:12px;height:12px;border-radius:50%;background:linear-gradient(135deg,var(--p2),var(--p0));flex:none}
h1{font-size:19px;font-weight:800;letter-spacing:-.02em}
.sub{color:var(--faint);font-size:12.5px}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px}
.label{color:var(--mute);font-size:11px;letter-spacing:.09em;text-transform:uppercase;font-weight:700;margin:0 0 10px}
input,button,select{font:inherit;color:var(--ink);border-radius:13px;border:1px solid var(--line);background:var(--card2);padding:12px 14px;outline:0}
button{cursor:pointer;font-weight:750;border:0;color:#0b0f1a;background:linear-gradient(135deg,var(--p2),var(--p0))}
button:active{transform:scale(.97)}button:disabled{background:var(--card2);color:var(--faint);border:1px solid var(--line)}
button.ghost{background:var(--card2);color:var(--ink);border:1px solid var(--line)}
button.mint{background:linear-gradient(135deg,var(--p1),#39c0ff);color:#06121a}
.row{display:flex;gap:10px}.row>*{flex:1}.narrow{flex:0 0 108px}
.err{color:var(--p0);font-size:13px;min-height:18px;margin:8px 0 0}
#lobby,#game{display:none}body.lobby #lobby,body.game #game{display:block}
body.game header{display:none}
.modes{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:10px 0 14px}
.mode{background:var(--card2);border:1px solid var(--line);border-radius:14px;padding:12px;cursor:pointer;color:var(--ink);text-align:left}
.mode b{display:block;font-size:14px}.mode span{font-size:12px;color:var(--mute)}
.mode.sel{border-color:rgba(255,176,32,.7);box-shadow:0 0 0 3px rgba(255,176,32,.14)}
.open{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid var(--line)}.open:last-child{border-bottom:0}
.open .code{font-weight:800;letter-spacing:.12em}.open .who{flex:1;color:var(--mute);font-size:13px}.open button{width:auto;padding:8px 14px}
#codein{text-transform:uppercase;letter-spacing:.28em;font-weight:800;text-align:center}

/* ── game ── */
.top{display:flex;align-items:center;gap:8px;margin:0 0 10px;font-size:13px;color:var(--mute)}
.top b{color:var(--ink);letter-spacing:.14em;font-size:15px}
.top .ghost{margin-left:auto;width:auto;padding:7px 12px;font-size:12px}
.chips{display:flex;gap:6px;margin:0 0 10px}
.chip{flex:1;display:flex;align-items:center;gap:5px;background:var(--card);border:1px solid var(--line);border-radius:12px;
  padding:6px 7px;min-width:0;position:relative;transition:box-shadow .2s,border-color .2s}
.chip .av{width:22px;height:22px;border-radius:50%;flex:none;display:flex;align-items:center;justify-content:center;
  font-weight:800;font-size:11px;color:#0b0f1a}
.chip .nm{flex:1;min-width:0;font-weight:700;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.chip .ct{flex:none;font-size:11px;color:var(--mute);font-variant-numeric:tabular-nums;font-weight:700}
.chip.turn{border-color:var(--cc);box-shadow:0 0 0 2px var(--cc),0 0 18px -4px var(--cc)}
.chip.out{opacity:.4}.chip.empty{opacity:.45}
.chip .em{position:absolute;right:4px;top:-14px;font-size:20px;animation:pop .3s ease}

#boardwrap{position:relative;width:100%;max-width:380px;margin:0 auto}
#board{width:100%;height:auto;display:block;touch-action:manipulation}
.bubble{position:absolute;font-size:30px;animation:pop .35s ease,rise 1.8s ease forwards;pointer-events:none;z-index:4}
@keyframes pop{from{transform:scale(0)}to{transform:scale(1)}}
@keyframes rise{to{transform:translateY(-36px);opacity:0}}
#board g.mv .halo{stroke-width:.2}

.stage{display:flex;align-items:center;gap:14px;margin:12px 0 0;min-height:76px}
.dice{width:68px;height:68px;border-radius:16px;flex:none;position:relative;cursor:default;
  background:linear-gradient(145deg,#ffffff,#d7def0);box-shadow:0 6px 16px -4px #000a,inset 0 2px 2px #fff;transition:transform .15s}
.dice.can{cursor:pointer;box-shadow:0 0 0 3px var(--cc),0 8px 20px -4px var(--cc)}
.dice.can{animation:nudge 1.1s ease-in-out infinite}
@keyframes nudge{50%{transform:translateY(-4px)}}
.dice.rolling{animation:shake .5s ease}
@keyframes shake{0%,100%{transform:rotate(0)}20%{transform:rotate(-18deg)}40%{transform:rotate(14deg)}60%{transform:rotate(-10deg)}80%{transform:rotate(6deg)}}
.dice i{position:absolute;width:11px;height:11px;border-radius:50%;background:#1a2030;box-shadow:inset 0 1px 1px #0006}
.dice .q{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:19px;font-weight:800;letter-spacing:.02em;color:#2a3550}
.prompt{flex:1;min-width:0}
.prompt .big{font-size:17px;font-weight:800;letter-spacing:-.01em;line-height:1.2}
.prompt .small{font-size:12.5px;color:var(--mute);margin-top:3px}
.prompt .big.you{color:var(--p1)}
.clock{height:6px;border-radius:999px;background:var(--card2);overflow:hidden;margin:10px 0 0}
.clock i{display:block;height:100%;background:linear-gradient(90deg,var(--p1),#39c0ff);transition:width .15s linear}
.clock.low i{background:var(--p0)}
.emobar{display:flex;gap:6px;flex-wrap:wrap;justify-content:center;margin:12px 0 0}
.emobar button{background:var(--card2);border:1px solid var(--line);color:var(--ink);font-size:20px;padding:5px 9px;border-radius:12px}
#again{display:none;margin:12px auto 0;max-width:280px}
.rule{text-align:center;color:var(--faint);font-size:12px;margin:12px auto 0;max-width:430px}
@media (prefers-reduced-motion:reduce){*{animation:none!important}}
</style></head>
<body class="lobby"><div class="wrap">

<header><span class="dot"></span><div><h1>Ludo</h1><p class="sub" id="sub">two to four players, refereed by the board</p></div></header>

<div id="lobby">
  <div class="card">
    <p class="label">new game</p>
    <input id="nm" placeholder="your name" maxlength="12" autocomplete="off">
    <div class="modes">
      <button class="mode sel" data-m="0" type="button"><b>Classic</b><span>4 tokens each</span></button>
      <button class="mode" data-m="1" type="button"><b>Quick</b><span>2 tokens · fast</span></button>
    </div>
    <button id="mk">create a room</button>
    <p class="err" id="mkerr"></p>
  </div>
  <div class="card" style="margin-top:12px">
    <p class="label">join a game</p>
    <div class="row"><input id="codein" placeholder="CODE" maxlength="4" autocomplete="off">
      <button class="narrow mint" id="jb">join</button></div>
    <div class="row" style="margin-top:8px"><button class="ghost" id="watchb">watch a game</button></div>
    <p class="err" id="jerr"></p>
    <div id="opens"></div>
  </div>
</div>

<div id="game">
  <div class="top">room<b id="code">----</b>
    <span id="modepill">· classic</span><span id="watchpill" style="display:none">· watching</span>
    <button class="ghost" onclick="leave()">leave</button></div>
  <div class="chips" id="chips"></div>
  <div id="boardwrap"><svg id="board" viewBox="-0.3 -0.3 15.6 15.6"></svg></div>
  <div class="stage">
    <div class="dice" id="dice" onclick="diceTap()"></div>
    <div class="prompt"><div class="big" id="pbig">—</div><div class="small" id="psmall"></div></div>
  </div>
  <div class="clock" id="clock"><i id="ck"></i></div>
  <div class="emobar" id="emobar"></div>
  <button id="again" class="mint" onclick="again()">play again</button>
  <p class="rule">Roll a 6 to bring a token out. Land on an opponent to send it home. Safe on the ★ stars.</p>
</div>

<script>
const $=i=>document.getElementById(i);
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());
const get=u=>fetch(u,{cache:'no-store'}).then(r=>r.json());
const enc=encodeURIComponent;
const NS='http://www.w3.org/2000/svg';
const COL=['#e8392b','#1ca84f','#f4c016','#2a7ade'];
const DK=['#9e2017','#0f7334','#b08a00','#17508f'];
const EMO=['😂','🔥','😮','😎','😭','👏','🎲','💀'];
let code='',token='',mode=0,me=-1,timer=null,last=null,busy=false,lastEmojiAt=0,lastDie=-1;

const RING=[[1,6],[2,6],[3,6],[4,6],[5,6],[6,5],[6,4],[6,3],[6,2],[6,1],[6,0],[7,0],[8,0],
 [8,1],[8,2],[8,3],[8,4],[8,5],[9,6],[10,6],[11,6],[12,6],[13,6],[14,6],[14,7],[14,8],
 [13,8],[12,8],[11,8],[10,8],[9,8],[8,9],[8,10],[8,11],[8,12],[8,13],[8,14],[7,14],[6,14],
 [6,13],[6,12],[6,11],[6,10],[6,9],[5,8],[4,8],[3,8],[2,8],[1,8],[0,8],[0,7],[0,6]];
const START=[0,13,26,39];
const SAFE=[0,8,13,21,26,34,39,47];
const HOME=[[[1,7],[2,7],[3,7],[4,7],[5,7]],[[7,1],[7,2],[7,3],[7,4],[7,5]],
            [[13,7],[12,7],[11,7],[10,7],[9,7]],[[7,13],[7,12],[7,11],[7,10],[7,9]]];
const CENTER=[[6.0,7],[7,6.0],[8.0,7],[7,8.0]];
const BASE=[[[2,2],[4,2],[2,4],[4,4]],[[11,2],[13,2],[11,4],[13,4]],
            [[11,11],[13,11],[11,13],[13,13]],[[2,11],[4,11],[2,13],[4,13]]];
const el=(t,a)=>{const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e};
function cellCenter(seat,prog,tokenIdx){
  if(prog===0)return BASE[seat][tokenIdx];
  if(prog>=1&&prog<=51){const r=RING[(START[seat]+prog-1)%52];return [r[0]+0.5,r[1]+0.5]}
  if(prog>=52&&prog<=56){const h=HOME[seat][prog-52];return [h[0]+0.5,h[1]+0.5]}
  return CENTER[seat];
}

let tokenEls=[];
function buildBoard(){
  const svg=$('board');svg.innerHTML='';
  svg.appendChild(el('rect',{x:-0.3,y:-0.3,width:15.6,height:15.6,rx:0.6,fill:'#ffffff'}));
  const corners=[[0,0],[9,0],[9,9],[0,9]];
  for(let s=0;s<4;s++){const[c,r]=corners[s];
    svg.appendChild(el('rect',{x:c,y:r,width:6,height:6,rx:0.3,fill:COL[s]}));
    svg.appendChild(el('rect',{x:c+0.85,y:r+0.85,width:4.3,height:4.3,rx:0.4,fill:'#ffffff'}));
    for(const[px,py]of BASE[s])svg.appendChild(el('circle',{cx:px,cy:py,r:0.52,fill:'none',stroke:COL[s],'stroke-opacity':0.5,'stroke-width':0.12}));
  }
  for(let i=0;i<52;i++){const[x,y]=RING[i];const owner=START.indexOf(i);const star=SAFE.includes(i)&&owner<0;
    svg.appendChild(el('rect',{x,y,width:1,height:1,fill:owner>=0?COL[owner]:'#ffffff',stroke:'#c7cedb','stroke-width':0.045}));
    if(star){const t=el('text',{x:x+0.5,y:y+0.77,'text-anchor':'middle','font-size':0.68,fill:'#9098a8'});t.textContent='\u2605';svg.appendChild(t)}
  }
  for(let s=0;s<4;s++)for(const[x,y]of HOME[s])
    svg.appendChild(el('rect',{x,y,width:1,height:1,fill:COL[s],stroke:'#ffffff','stroke-width':0.06}));
  // direction arrows into each home column, in the player's colour
  const arrows=[['0.2,7.15 0.2,7.85 0.85,7.5',0],['6.15,0.2 6.85,0.2 6.5,0.85',1],['14.8,7.15 14.8,7.85 14.15,7.5',2],['6.15,14.8 6.85,14.8 6.5,14.15',3]];
  for(const[pts,s]of arrows)svg.appendChild(el('polygon',{points:pts,fill:COL[s]}));
  svg.appendChild(el('polygon',{points:'7,7 6,6 6,8',fill:COL[0]}));
  svg.appendChild(el('polygon',{points:'7,7 6,6 8,6',fill:COL[1]}));
  svg.appendChild(el('polygon',{points:'7,7 8,6 8,8',fill:COL[2]}));
  svg.appendChild(el('polygon',{points:'7,7 6,8 8,8',fill:COL[3]}));
  tokenEls=[];
  for(let s=0;s<4;s++){tokenEls[s]=[];for(let t=0;t<4;t++){
    const g=el('g',{});
    const halo=el('circle',{r:0.52,fill:'none',stroke:'#ffd21a','stroke-width':0.16,opacity:0});
    const body=el('circle',{r:0.4,fill:COL[s],stroke:'#141a28','stroke-width':0.09});
    const inner=el('circle',{r:0.19,fill:'none',stroke:DK[s],'stroke-width':0.13});
    g.append(halo,body,inner);g.style.cursor='pointer';
    g.addEventListener('click',()=>tapToken(s,t));
    svg.appendChild(g);tokenEls[s][t]={g,halo};
  }}
}
function tapToken(s,t){
  if(busy||me<0||!last||last.phase!==1||last.turn!==me||last.gphase!==2)return;
  if(s!==me||!(last.movable||[]).includes(t))return;
  move(t);
}
function place(d){
  const stack={};
  for(let s=0;s<4;s++)for(let t=0;t<4;t++){
    const te=tokenEls[s][t];const prog=d.prog[s][t];
    if(t>=d.mode){te.g.style.display='none';continue}
    te.g.style.display='';
    let [x,y]=cellCenter(s,prog,t);
    const key=prog>=1&&prog<=51?('r'+((START[s]+prog-1)%52)):(prog===57?'c'+s:'');
    if(key){const n=stack[key]||0;stack[key]=n+1;x+=(n%2)*0.3-0.15;y+=(n>1?0.3:0)-0.08}
    te.g.setAttribute('transform','translate('+x+' '+y+') scale('+(prog>=52?0.84:1)+')');
    const movable=me===s&&d.phase===1&&d.turn===me&&d.gphase===2&&(d.movable||[]).includes(t);
    te.g.setAttribute('class',movable?'mv':'');
    te.halo.setAttribute('opacity',movable?1:0);
  }
}

function pips(v,q){const d=$('dice');d.innerHTML='';
  if(q){const s=document.createElement('div');s.className='q';s.textContent=q;s.style.color='#2a3550';d.appendChild(s);return}
  const P={1:[[1,1]],2:[[0,0],[2,2]],3:[[0,0],[1,1],[2,2]],4:[[0,0],[2,0],[0,2],[2,2]],
    5:[[0,0],[2,0],[1,1],[0,2],[2,2]],6:[[0,0],[2,0],[0,1],[2,1],[0,2],[2,2]]}[v]||[];
  for(const[c,r]of P){const i=document.createElement('i');i.style.left=(13+c*16)+'px';i.style.top=(13+r*16)+'px';d.appendChild(i)}}

function bubble(seat,e){
  const b=document.createElement('div');b.className='bubble';b.textContent=EMO[e];
  const pos=[[14,10],[72,10],[72,70],[14,70]][seat];
  b.style.left=pos[0]+'%';b.style.top=pos[1]+'%';$('boardwrap').appendChild(b);setTimeout(()=>b.remove(),2000);
}

function draw(d){
  last=d;me=d.me;
  $('code').textContent=d.code;$('modepill').textContent='· '+(d.mode===2?'quick':'classic');
  $('watchpill').style.display=me<0?'inline':'none';
  const C=$('chips');C.innerHTML='';
  for(let s=0;s<4;s++){
    const here=d.here[s],home=d.prog[s].filter((p,i)=>i<d.mode&&p===57).length;
    const c=document.createElement('div');
    c.className='chip'+(d.phase===1&&d.turn===s?' turn':'')+(d.phase>0&&!here?' out':'')+(!here&&d.phase===0?' empty':'');
    c.style.setProperty('--cc',COL[s]);
    const av=document.createElement('span');av.className='av';av.style.background=COL[s];av.textContent=here?(d.names[s][0]||'?').toUpperCase():'+';
    const nm=document.createElement('span');nm.className='nm';nm.textContent=here?(s===me?'You':d.names[s]):'open';
    const ct=document.createElement('span');ct.className='ct';ct.textContent=!here?'':(d.phase>0?home+'/'+d.mode:(d.wins[s]?'★'+d.wins[s]:''));
    c.append(av,nm,ct);
    if(d.emoji&&d.emoji.s===s){const e=document.createElement('span');e.className='em';e.textContent=EMO[d.emoji.e];c.appendChild(e)}
    C.appendChild(c);
  }
  place(d);
  if(d.emoji&&d.emoji.at!==lastEmojiAt){lastEmojiAt=d.emoji.at;bubble(d.emoji.s,d.emoji.e)}

  // dice: shake when the value changes
  const dice=$('dice');
  if(d.phase===1&&d.die!==lastDie&&d.die>0){dice.classList.remove('rolling');void dice.offsetWidth;dice.classList.add('rolling')}
  lastDie=d.phase===1?d.die:-1;

  const mine=d.phase===1&&d.turn===me;
  const canRoll=mine&&d.gphase===1;
  dice.classList.toggle('can',canRoll);
  const name=s=>s===me?'you':(d.names[s]||'a player');
  let big='',small='',pq='';
  if(d.phase===0){
    pq='LUDO';
    big=me===d.host?'Your room is ready':'Waiting to start';
    const n=d.here.filter(Boolean).length;
    small=me===d.host?(n<2?'Share code '+d.code+' · need one more':n+' players in — tap START'):'Waiting for '+d.names[d.host]+' to start';
  }else if(d.phase===2){
    pq='🏆';big=d.winner===me?'You win! 🎉':name(d.winner)+' wins';small='Tap play again for another round';
    pips(0,'🏆');
  }else if(canRoll){
    pips(0,'?');big='Your turn';big+='';small='Tap the dice to roll';$('pbig').className='big you';
  }else if(mine){                       // my move
    pips(d.die);
    big=d.die===6?'You rolled a 6!':'You rolled '+d.die;
    small='Tap a glowing token to move';$('pbig').className='big you';
  }else{
    pips(d.die||0, d.die?'':'…');
    big=name(d.turn)+"'s turn"; big=big.charAt(0).toUpperCase()+big.slice(1);
    small=d.gphase===1?'rolling…':(me<0?'watching the game':'they are moving');
  }
  if(d.phase!==1||!mine)$('pbig').className='big';
  if(d.phase===1&&!canRoll&&!mine&&d.die>0)pips(d.die);
  $('pbig').textContent=big;$('psmall').textContent=small;
  if(d.phase===1&&d.auto)$('psmall').textContent=name(d.turn)+' ran out of time — the board played';

  // controls
  if(d.phase===0&&me===d.host){const b=$('dice');} // dice inert in lobby
  // a Start button lives inside the dice area as a tap when host & waiting
  dice.classList.toggle('can',canRoll||(d.phase===0&&me===d.host&&d.here.filter(Boolean).length>=2));
  if(d.phase===0&&me===d.host&&d.here.filter(Boolean).length>=2){pips(0,'GO')}
  $('again').style.display=d.phase===2&&me>=0?'block':'none';
  $('emobar').style.display=me>=0&&d.phase>0?'flex':'none';
  if(d.clock>=0&&d.phase===1){deadline=Date.now()+d.clock;limit=d.limit;$('clock').style.display='block'}
  else{deadline=0;$('clock').style.display='none'}
  $('sub').textContent=d.phase===1?(mine?'your turn':name(d.turn)+' playing'):(me<0?'watching':'get everyone in and start');
}

let deadline=0,limit=25000;
setInterval(()=>{if(!deadline)return;const left=Math.max(0,deadline-Date.now());
  $('ck').style.width=(left/limit*100)+'%';$('clock').classList.toggle('low',left<6000)},100);

function diceTap(){
  if(!last)return;
  if(last.phase===0&&me===last.host&&last.here.filter(Boolean).length>=2){start();return}
  if(last.phase===1&&last.turn===me&&last.gphase===1)roll();
}

EMO.forEach((e,i)=>{const b=document.createElement('button');b.textContent=e;
  b.onclick=()=>post('/emoji','c='+enc(code)+'&t='+enc(token)+'&e='+i);$('emobar').appendChild(b)});

document.querySelectorAll('.mode').forEach(b=>b.onclick=()=>{document.querySelectorAll('.mode').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');mode=+b.dataset.m});
function store(){try{localStorage.setItem('ludo',JSON.stringify({code,token,me}))}catch(e){}}
function forget(){try{localStorage.removeItem('ludo')}catch(e){}}
function myName(){const n=$('nm').value.trim();try{localStorage.setItem('ludo_n',n)}catch(e){};return n}
const ERRS={name:'Pick a name first.',full:'All rooms are in use.',nocode:'No room with that code.',busy:'That room is full.',started:'That game has started.',alone:'Need at least two players.'};
function show(v){document.body.className=v}
function toLobby(m){clearTimeout(timer);code='';token='';me=-1;last=null;lastDie=-1;forget();show('lobby');$('jerr').textContent=m||'';history.replaceState(null,'','/');lobby()}
function enter(d){code=d.code;token=d.token;me=d.seat;store();last=null;lastDie=-1;buildBoard();history.replaceState(null,'','/?c='+code);show('game');poll()}

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
    if(d.rooms.length){const l=document.createElement('p');l.className='label';l.style.marginTop='14px';l.textContent='rooms to join or watch';box.appendChild(l)}
    for(const r of d.rooms){const row=document.createElement('div');row.className='open';
      const c=document.createElement('span');c.className='code';c.textContent=r.c;
      const w=document.createElement('span');w.className='who';w.textContent=r.h+' · '+(r.m===2?'quick':'classic')+' · '+r.n+'/4';
      const b=document.createElement('button');b.className='mint';b.textContent='join';b.onclick=()=>join(r.c);
      row.append(c,w,b);box.appendChild(row)}
  }catch(e){}
  setTimeout(lobby,2500)}

async function start(){try{const d=await post('/start','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function roll(){if(busy)return;busy=true;$('dice').classList.add('rolling');
  try{const d=await post('/roll','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}busy=false;poll()}
async function move(k){if(busy)return;busy=true;
  try{const d=await post('/move','c='+enc(code)+'&t='+enc(token)+'&k='+k);if(d.ok)draw(d)}catch(e){}busy=false;poll()}
async function again(){try{const d=await post('/again','c='+enc(code)+'&t='+enc(token));if(d.ok)draw(d)}catch(e){}}
async function leave(){try{await post('/leave','c='+enc(code)+'&t='+enc(token))}catch(e){};toLobby('')}

async function poll(){clearTimeout(timer);if(!code)return;
  try{const d=await get('/state?c='+enc(code)+'&t='+enc(token));
    if(!d.ok){toLobby('The game ended.');return}
    if(!busy)draw(d);
  }catch(e){$('sub').textContent='lost the board'}
  timer=setTimeout(poll,600)}

(async function boot(){try{$('nm').value=localStorage.getItem('ludo_n')||''}catch(e){}
  let saved=null;try{saved=JSON.parse(localStorage.getItem('ludo')||'null')}catch(e){}
  if(saved&&saved.code&&saved.token){try{const d=await post('/join','c='+enc(saved.code)+'&t='+enc(saved.token));if(d.ok){enter(d);return}}catch(e){}forget()}
  const q=new URLSearchParams(location.search).get('c');if(q)$('codein').value=q.toUpperCase();
  show('lobby');lobby()})();
</script>
</body></html>
)HTML";
