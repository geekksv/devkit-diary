#pragma once
//
// The page: it asks the board what it hears four times a second and turns the answer into
// sound (Web Audio) and a living picture (canvas). The board sends numbers; the music is made
// here, because a browser is far better at sound than a chip with no speaker.

static const char AIR_BODY[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#05060a">
<title>The sound of the room</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:#05060a;color:#e8ecf6;overflow:hidden;
  font:14px/1.5 ui-rounded,-apple-system,"Segoe UI",system-ui,sans-serif;-webkit-font-smoothing:antialiased}
#sky{position:fixed;inset:0;display:block}
.bar{position:fixed;left:0;right:0;top:0;display:flex;align-items:center;gap:12px;flex-wrap:wrap;
  padding:max(10px,env(safe-area-inset-top)) 16px 10px;background:linear-gradient(#05060acc,#05060a00);z-index:3}
.bar h1{font-size:15px;font-weight:700;letter-spacing:.01em}
.bar .sub{color:#8b93ab;font-size:12px}
.spacer{flex:1}
.ctrl{display:flex;align-items:center;gap:8px}
button,select{font:inherit;color:#e8ecf6;background:#141824;border:1px solid #273043;border-radius:11px;
  padding:8px 13px;cursor:pointer;font-weight:600}
button.on{background:linear-gradient(135deg,#7ee7c7,#5ec8f0);color:#06121a;border:0}
select{padding:7px 10px}
input[type=range]{width:110px;accent-color:#7ee7c7}
.read{position:fixed;left:16px;bottom:calc(14px + env(safe-area-inset-bottom));z-index:3;color:#9aa3bb;
  font:12px/1.6 ui-monospace,Menlo,monospace;text-shadow:0 1px 6px #000}
.read b{color:#e8ecf6}
.hint{position:fixed;right:16px;bottom:calc(14px + env(safe-area-inset-bottom));z-index:3;color:#5b6680;
  font-size:11.5px;text-align:right;max-width:40vw}
#gate{position:fixed;inset:0;z-index:5;display:flex;flex-direction:column;align-items:center;justify-content:center;
  gap:18px;background:radial-gradient(60% 60% at 50% 42%,#0b1020,#05060a);text-align:center;padding:24px;transition:opacity .6s}
#gate.gone{opacity:0;pointer-events:none}
#gate h2{font-size:clamp(26px,6vw,44px);font-weight:800;letter-spacing:-.02em}
#gate p{color:#9aa3bb;max-width:460px;line-height:1.6}
#play{font-size:17px;padding:15px 30px;border-radius:16px;background:linear-gradient(135deg,#7ee7c7,#5ec8f0);
  color:#06121a;border:0;font-weight:800}
#chip{color:#5b6680;font-size:12px}
@media (prefers-reduced-motion:reduce){#gate{transition:none}}
</style></head>
<body>
<canvas id="sky"></canvas>

<div class="bar">
  <div><h1>The sound of the room</h1><div class="sub" id="status">listening…</div></div>
  <div class="spacer"></div>
  <div class="ctrl">
    <button id="mute" class="on" title="sound on/off">♪ on</button>
    <input type="range" id="vol" min="0" max="100" value="70" title="volume">
    <select id="key" title="musical key">
      <option value="0">key of A</option><option value="3">C</option><option value="5">D</option>
      <option value="7">E</option><option value="-2">G</option>
    </select>
  </div>
</div>

<div class="read" id="read"></div>
<div class="hint">pitch = channel · loudness = signal · shimmer = live traffic</div>

<div id="gate">
  <h2>The sound of the room</h2>
  <p>Every WiFi network around you becomes a held note; the live traffic on the board's channel
     becomes a shimmer. Turn the volume up a little — it is meant to be ambient.</p>
  <button id="play">▶  play the air</button>
  <div id="chip">needs sound · best with headphones or a speaker</div>
</div>

<script>
const $=i=>document.getElementById(i);

// ── the pure mapping: a channel to a pitch, a signal to a loudness ──
// A pentatonic scale, so however the networks fall it stays consonant. Channel 1 is the lowest
// note, channel 13 the highest, climbing the scale across two octaves.
const PENTA=[0,2,4,7,9];
function chanSemitone(ch){const i=Math.max(0,Math.min(12,ch-1));return PENTA[i%5]+12*Math.floor(i/5)}
function chanFreq(ch,key){const base=220*Math.pow(2,key/12);return base*Math.pow(2,chanSemitone(ch)/12)}
// Signal strength to a gain 0..1. -90 dBm is the edge of hearing, -45 and up is full.
function rssiGain(r){const g=(r+90)/45;return Math.max(0,Math.min(1,g))*Math.max(0,Math.min(1,g))}
// A stable little detune per network, from its address, so two networks on one channel beat
// gently against each other instead of ringing as one dead tone.
function detuneOf(bssid){let h=0;for(const c of bssid)h=(h*31+c.charCodeAt(0))&255;return (h/255-0.5)*18}
const hueOf=ch=>((ch-1)/12*300)|0;

// ── the synth ──
let ctx=null,master=null,comp=null,verb=null,shimmer=null,shimGain=null;
const voices=new Map();   // key -> {osc,gain,pan,lfo,lfoGain}
let audioOn=true,lastPing=0,rateAvg=0;

function reverbBuffer(c){
  const len=c.sampleRate*2.6,b=c.createBuffer(2,len,c.sampleRate);
  for(let ch=0;ch<2;ch++){const d=b.getChannelData(ch);
    for(let i=0;i<len;i++)d[i]=(Math.random()*2-1)*Math.pow(1-i/len,2.5)}
  return b;
}
function buildAudio(){
  const AC=window.AudioContext||window.webkitAudioContext;
  ctx=new AC();
  master=ctx.createGain();master.gain.value=0.7;
  comp=ctx.createDynamicsCompressor();comp.threshold.value=-18;comp.ratio.value=4;
  verb=ctx.createConvolver();verb.buffer=reverbBuffer(ctx);
  const vg=ctx.createGain();vg.gain.value=0.5;
  master.connect(comp);comp.connect(ctx.destination);
  comp.connect(verb);verb.connect(vg);vg.connect(ctx.destination);
  // the shimmer: filtered noise whose loudness follows the live traffic
  const n=ctx.createBufferSource(),nb=ctx.createBuffer(1,ctx.sampleRate*2,ctx.sampleRate),d=nb.getChannelData(0);
  for(let i=0;i<d.length;i++)d[i]=Math.random()*2-1;
  n.buffer=nb;n.loop=true;
  const bp=ctx.createBiquadFilter();bp.type='bandpass';bp.frequency.value=3200;bp.Q.value=0.7;
  shimGain=ctx.createGain();shimGain.gain.value=0;
  n.connect(bp);bp.connect(shimGain);shimGain.connect(master);n.start();
  shimmer=n;
}

function voiceFor(net){
  let v=voices.get(net.b);
  if(v)return v;
  const osc=ctx.createOscillator();osc.type=net.o?'triangle':'sine';   // secured a touch brighter
  osc.frequency.value=chanFreq(net.c,0);osc.detune.value=detuneOf(net.b);
  const gain=ctx.createGain();gain.gain.value=0;
  const pan=(ctx.createStereoPanner?ctx.createStereoPanner():null);
  if(pan)pan.pan.value=Math.max(-1,Math.min(1,(net.c-7)/6));
  // a slow vibrato, deepened by how busy that channel is, so a busy channel glitters
  const lfo=ctx.createOscillator();lfo.frequency.value=0.2+Math.random()*0.3;
  const lfoGain=ctx.createGain();lfoGain.gain.value=0;
  lfo.connect(lfoGain);lfoGain.connect(osc.detune);
  osc.connect(gain);gain.connect(pan||master);if(pan)pan.connect(master);
  osc.start();lfo.start();
  v={osc,gain,pan,lfo,lfoGain};voices.set(net.b,v);return v;
}

function updateAudio(d){
  if(!ctx)return;
  const now=ctx.currentTime,on=audioOn?1:0;
  const nets=d.nets||[];
  const count=Math.max(1,nets.length);
  const head=0.9/Math.sqrt(count);          // share the headroom so more networks never clip
  const seen=new Set();
  const per=d.live.per||[];
  const busiest=Math.max(1,d.live.busiest||1);
  for(const net of nets){
    seen.add(net.b);
    const v=voiceFor(net);v.dead=undefined;        // alive again: cancel any pending fade-out
    v.osc.frequency.setTargetAtTime(chanFreq(net.c,CUR_KEY),now,0.4);
    v.gain.gain.setTargetAtTime(rssiGain(net.r)*head*on,now,0.5);
    if(v.pan)v.pan.pan.setTargetAtTime(Math.max(-1,Math.min(1,(net.c-7)/6)),now,0.8);
    const act=(per[net.c-1]||0)/busiest;    // 0..1 activity on this network's channel
    v.lfoGain.gain.setTargetAtTime(6+act*40,now,0.6);
  }
  // networks that have gone: fade out, then free them
  for(const [b,v] of voices){
    if(seen.has(b))continue;
    v.gain.gain.setTargetAtTime(0,now,0.8);
    if(v.dead===undefined){v.dead=now;}
    else if(now-v.dead>2.5){try{v.osc.stop();v.lfo.stop()}catch(e){}voices.delete(b)}
  }
  // the shimmer tracks the live frame rate; sudden bursts ring a soft bell
  const rate=d.live.rate||0;
  shimGain.gain.setTargetAtTime(Math.min(0.28,rate/400)*on,now,0.25);
  if(audioOn&&rate>rateAvg*1.8+20&&now-lastPing>0.4){ping(rate);lastPing=now}
  rateAvg=rateAvg*0.9+rate*0.1;
}
function ping(rate){
  const now=ctx.currentTime,o=ctx.createOscillator(),g=ctx.createGain();
  const notes=[0,2,4,7,9,12,16];o.type='sine';
  o.frequency.value=chanFreq(7,CUR_KEY)*Math.pow(2,notes[(Math.random()*notes.length)|0]/12);
  g.gain.setValueAtTime(0,now);g.gain.linearRampToValueAtTime(Math.min(0.2,rate/1500),now+0.01);
  g.gain.exponentialRampToValueAtTime(0.0005,now+1.2);
  const p=ctx.createStereoPanner?ctx.createStereoPanner():null;if(p)p.pan.value=Math.random()*2-1;
  o.connect(g);g.connect(p||master);if(p)p.connect(master);o.start();o.stop(now+1.3);
}
let CUR_KEY=0;

// ── the picture ──
const cv=$('sky'),g=cv.getContext('2d');let W=0,H=0,DPR=1;
function size(){DPR=Math.min(2,devicePixelRatio||1);W=cv.width=innerWidth*DPR;H=cv.height=innerHeight*DPR;}
addEventListener('resize',size);size();
let last=null,orbs=new Map(),t0=performance.now();
function draw(ts){
  const t=(ts-t0)/1000;
  g.fillStyle='rgba(5,6,10,0.28)';g.fillRect(0,0,W,H);   // trails
  const d=last;
  if(d){
    const per=d.live.per||[],busiest=Math.max(1,d.live.busiest||1);
    const nets=(d.nets||[]).slice().sort((a,b)=>a.c-b.c);
    const seen=new Set();
    for(const net of nets){
      seen.add(net.b);
      let orb=orbs.get(net.b);if(!orb){orb={x:0,y:0,r:0,a:0};orbs.set(net.b,orb)}
      const tx=(0.08+0.84*(net.c-1)/12)*W;
      const ty=(0.80-0.60*Math.max(0,Math.min(1,(net.r+90)/50)))*H;
      const tr=(10+42*rssiGain(net.r))*DPR;
      orb.x+=(tx-orb.x)*0.08;orb.y+=(ty-orb.y)*0.08;orb.r+=(tr-orb.r)*0.08;orb.a+=(1-orb.a)*0.05;
      const act=(per[net.c-1]||0)/busiest;
      const pulse=1+0.18*Math.sin(t*(1.2+act*6)+net.c)+act*0.5;
      const hue=hueOf(net.c),rad=orb.r*pulse;
      const grd=g.createRadialGradient(orb.x,orb.y,0,orb.x,orb.y,rad*2.4);
      grd.addColorStop(0,`hsla(${hue},85%,68%,${0.9*orb.a})`);
      grd.addColorStop(0.4,`hsla(${hue},85%,55%,${0.35*orb.a})`);
      grd.addColorStop(1,`hsla(${hue},85%,45%,0)`);
      g.fillStyle=grd;g.beginPath();g.arc(orb.x,orb.y,rad*2.4,0,7);g.fill();
      g.fillStyle=`hsla(${hue},90%,80%,${orb.a})`;g.beginPath();g.arc(orb.x,orb.y,rad*0.5,0,7);g.fill();
    }
    for(const [b,orb] of orbs){if(!seen.has(b)){orb.a*=0.92;if(orb.a<0.02){orbs.delete(b);continue}
      g.fillStyle=`hsla(210,30%,60%,${orb.a*0.4})`;g.beginPath();g.arc(orb.x,orb.y,orb.r*0.5,0,7);g.fill()}}
  }
  requestAnimationFrame(draw);
}
requestAnimationFrame(draw);

// ── polling ──
let poll=null;
async function tick(){
  try{
    const d=await (await fetch('/air',{cache:'no-store'})).json();
    last=d;updateAudio(d);
    $('status').textContent=(d.nets.length)+' networks · listening on channel '+d.camp;
    $('read').innerHTML='<b>'+d.nets.length+'</b> networks &nbsp; <b>'+Math.round(d.live.rate)+
      '</b> frames/s &nbsp; <b>'+d.live.kbps.toFixed(1)+'</b> KB/s'+
      (d.here?' &nbsp; '+d.here+' listening':'');
  }catch(e){$('status').textContent='lost the board'}
  poll=setTimeout(tick,250);
}

// ── controls ──
$('play').onclick=()=>{
  try{buildAudio();ctx.resume&&ctx.resume()}catch(e){}
  $('gate').classList.add('gone');
  tick();
};
$('mute').onclick=()=>{audioOn=!audioOn;$('mute').textContent=audioOn?'♪ on':'♪ off';$('mute').classList.toggle('on',audioOn)};
$('vol').oninput=()=>{if(master)master.gain.setTargetAtTime($('vol').value/100,ctx.currentTime,0.1)};
$('key').onchange=()=>{CUR_KEY=+$('key').value};
// If a browser allows audio without a tap (some do), still wait for play so nobody is startled.
</script>
</body></html>
)HTML";
