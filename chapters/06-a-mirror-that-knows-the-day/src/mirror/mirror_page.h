#pragma once
//
// The mirror itself: the page a tablet, phone or laptop shows behind the glass.
//
// It is drawn for any screen — portrait or landscape, a phone or a monitor — by sizing
// everything from the screen's smaller side (vmin) and moving blocks between a three-column
// and a two-column layout by orientation. Nothing scrolls.
//
// By day it is white on black; at night, black on white — switched at the city's own sunrise
// and sunset, which the weather brings with it. A setting can pin either.
//
// The board sends data, never pixels: the page asks /api/data every five seconds and draws
// everything itself, including the ticking clock.

static const char MIRROR_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000">
<title>Mirror</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Roboto+Condensed:wght@300;400&display=swap">
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#000;--fg:#fff;--dim:#9e9e9e;--faint:#616161;--rule:#1c1c1c;
  --u:min(1vh,1vw)}
body.night{--bg:#fff;--fg:#000;--dim:#555;--faint:#8a8a8a;--rule:#e6e6e6}
html,body{height:100%;background:var(--bg)}
body{color:var(--fg);font-family:'Roboto Condensed','Arial Narrow',sans-serif;font-weight:300;
  height:100dvh;overflow:hidden;padding:calc(var(--u)*5) calc(var(--u)*6);
  display:flex;flex-direction:column;gap:calc(var(--u)*2);
  transition:background-color 2s ease,color 2s ease;-webkit-font-smoothing:antialiased;
  user-select:none;-webkit-user-select:none}
body.idle{cursor:none}
.dim{color:var(--dim)}.faint{color:var(--faint)}
.caps{text-transform:uppercase;letter-spacing:.12em;font-weight:400}

.top{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) minmax(0,1fr);
  grid-template-areas:"clock side wx" "clock side fc";column-gap:calc(var(--u)*4);row-gap:0;align-items:start}
.clock{grid-area:clock;min-width:0}.side{grid-area:side;min-width:0}.wx{grid-area:wx;min-width:0}
.fcw{grid-area:fc;justify-self:end;min-width:0}
.clock .date{font-size:max(14px,calc(var(--u)*3.4));font-weight:400}
.clock .time{display:flex;align-items:flex-start;line-height:1;margin-top:calc(var(--u)*.8)}
.clock .hm{font-size:max(44px,calc(var(--u)*17));letter-spacing:-.02em}
.clock .sec{font-size:max(16px,calc(var(--u)*5.5));margin:calc(var(--u)*1.6) 0 0 calc(var(--u)*1)}
.clock .ampm{font-size:max(14px,calc(var(--u)*3.6));margin:calc(var(--u)*1.6) 0 0 calc(var(--u)*1)}
.clock .hol{font-size:max(12px,calc(var(--u)*2.5));margin-top:calc(var(--u)*1.6)}

.side{text-align:center;padding-top:calc(var(--u)*4)}
.side .lab,.news .lab{font-size:max(11px,calc(var(--u)*2.1))}
.side .otd{font-size:max(13px,calc(var(--u)*3));line-height:1.4;margin-top:calc(var(--u)*1.2);color:var(--dim)}

.wx{display:flex;flex-direction:column;align-items:flex-end;text-align:right}
.wx .city{font-size:max(12px,calc(var(--u)*2.6))}
.wx .now{display:flex;align-items:center;gap:calc(var(--u)*1.6);line-height:1}
.wx .now svg{width:max(34px,calc(var(--u)*10));height:max(34px,calc(var(--u)*10))}
.wx .temp{font-size:max(40px,calc(var(--u)*14))}
.wx .desc{font-size:max(13px,calc(var(--u)*3))}
.wx .more{font-size:max(11px,calc(var(--u)*2.4));margin-top:calc(var(--u)*.4)}
.fc{margin-top:calc(var(--u)*2.4);width:calc(var(--u)*40);min-width:200px;border-collapse:collapse}
.fc td{font-size:max(13px,calc(var(--u)*3));padding:calc(var(--u)*.9) 0;border-bottom:1px solid var(--rule)}
.fc td:first-child{text-align:left}
.fc td.ic{width:calc(var(--u)*6);text-align:center}
.fc td.ic svg{width:max(16px,calc(var(--u)*3.6));height:max(16px,calc(var(--u)*3.6));vertical-align:middle}
.fc td.n{text-align:right;width:calc(var(--u)*8)}

.mid{flex:1;min-height:0;display:flex;flex-direction:column;align-items:center;justify-content:center;
  text-align:center;gap:calc(var(--u)*2)}
.greet{font-size:max(26px,calc(var(--u)*8));line-height:1.15}
.mid .otd{display:none;font-size:max(14px,calc(var(--u)*3.2));color:var(--dim);max-width:80%;line-height:1.4}

.news{text-align:center;display:flex;flex-direction:column;align-items:center;gap:calc(var(--u)*1)}
.news .head{font-size:max(15px,calc(var(--u)*3.8));line-height:1.3;max-width:90%;
  transition:opacity .8s ease;min-height:2.6em}
.news ol{display:none}
.foot{margin-top:auto;display:flex;justify-content:space-between;gap:16px;font-size:max(10px,calc(var(--u)*1.8));
  border-top:1px solid var(--rule);padding-top:calc(var(--u)*1.2)}

/* Portrait: the clock and the weather now share the top row, the forecast gets a row of its
   own across the full width — a phone has height to spare and no width. "On this day" moves
   under the greeting. */
@media (orientation:portrait){
  .top{grid-template-columns:minmax(0,1fr) auto;grid-template-areas:"clock wx" "fc fc"}
  .side{display:none}
  .fcw{justify-self:stretch}
  .fc{width:100%;min-width:0;margin-top:calc(var(--u)*5)}
  .clock .hm{font-size:max(40px,calc(var(--u)*15))}
  .wx .temp{font-size:max(36px,calc(var(--u)*12))}
  .mid .otd{display:block}
}

/* The BOOT button cycles these views. */
body.v1 .mid,body.v1 .news,body.v1 .side{display:none}
body.v1 .top{grid-template-columns:minmax(0,1fr) minmax(0,1fr);grid-template-areas:"clock wx" "fc fc"}
body.v1 .fcw{justify-self:stretch}
body.v1 .fc{width:100%;margin-top:calc(var(--u)*5)}
body.v1 .fc td{font-size:max(16px,calc(var(--u)*4.2))}
body.v2 .mid,body.v2 .side,body.v2 .fcw{display:none}
body.v2 .news{flex:1;min-height:0;overflow:hidden;justify-content:flex-start;padding-top:calc(var(--u)*3)}
body.v2 .news .head{display:none}
body.v2 .news ol{display:block;list-style:none;text-align:left;max-width:92%}
body.v2 .news li{font-size:max(14px,calc(var(--u)*3.4));line-height:1.35;padding:calc(var(--u)*1) 0;
  border-bottom:1px solid var(--rule)}
body.v3 .top .side,body.v3 .wx,body.v3 .fcw,body.v3 .mid,body.v3 .news,body.v3 .clock .hol{display:none}
body.v3 .top{grid-template-columns:1fr;grid-template-areas:"clock";flex:1;align-content:center;justify-items:center;text-align:center}
body.v3 .clock .time{justify-content:center}
body.v3 .clock .hm{font-size:max(64px,calc(var(--u)*30))}

.hint{position:fixed;left:50%;bottom:calc(var(--u)*9);transform:translateX(-50%);
  font-size:max(12px,calc(var(--u)*2.2));color:var(--dim);border:1px solid var(--rule);border-radius:999px;
  padding:8px 16px;background:var(--bg);transition:opacity 1s;white-space:nowrap}
.hint.gone{opacity:0;pointer-events:none}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head>
<body class="day v0">

<div class="top">
  <div class="clock">
    <div class="date dim" id="date">&nbsp;</div>
    <div class="time"><span class="hm" id="hm">--:--</span><span class="sec faint" id="sec"></span><span class="ampm faint" id="ampm"></span></div>
    <div class="hol faint" id="hol"></div>
  </div>
  <div class="side">
    <div class="lab caps faint" id="otdlab"></div>
    <div class="otd" id="otd1"></div>
  </div>
  <div class="wx" id="wx">
    <div class="city caps dim" id="city"></div>
    <div class="now"><span id="icon"></span><span class="temp" id="temp"></span></div>
    <div class="desc dim" id="desc"></div>
    <div class="more faint" id="more1"></div>
    <div class="more faint" id="more2"></div>
  </div>
  <div class="fcw"><table class="fc" id="fc"></table></div>
</div>

<div class="mid">
  <div class="greet" id="greet"></div>
  <div class="otd" id="otd2"></div>
</div>

<div class="news">
  <div class="lab caps faint" id="newslab"></div>
  <div class="head" id="head"></div>
  <ol id="newslist"></ol>
</div>

<div class="foot faint"><span id="foot1">connecting to the board…</span><span id="foot2"></span></div>
<div class="hint" id="hint">tap for full screen · settings at <span id="hinturl"></span></div>

<script>
const $=i=>document.getElementById(i);
const DAYS=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const MONTHS=['January','February','March','April','May','June','July','August','September','October','November','December'];
let S=null,delta=0,got=0,headAt=0,headI=0,lastPaint='';

// WMO weather codes, grouped the way a person would describe the sky.
function sky(c){
  if(c===0)return['Clear','clear'];if(c===1)return['Mainly clear','clear'];if(c===2)return['Partly cloudy','part'];
  if(c===3)return['Overcast','cloud'];if(c===45||c===48)return['Fog','fog'];
  if(c>=51&&c<=57)return['Drizzle','drizzle'];if(c>=61&&c<=67)return['Rain','rain'];
  if(c>=71&&c<=77)return['Snow','snow'];if(c>=80&&c<=82)return['Showers','rain'];
  if(c===85||c===86)return['Snow showers','snow'];if(c>=95)return['Thunderstorm','storm'];return['—','cloud'];
}
// Line icons in the text colour, so they follow the theme.
const SUN='<circle cx="12" cy="12" r="4.2"/><path d="M12 2.6v2.2M12 19.2v2.2M2.6 12h2.2M19.2 12h2.2M5.4 5.4l1.6 1.6M17 17l1.6 1.6M18.6 5.4L17 7M7 17l-1.6 1.6"/>';
const MOON='<path d="M19.5 14.6A8 8 0 0 1 9.4 4.5a8 8 0 1 0 10.1 10.1z"/>';
const CLOUD='<path d="M7 18.5h10.5a3.8 3.8 0 0 0 .3-7.6 5.3 5.3 0 0 0-10.2 1.3A3.2 3.2 0 0 0 7 18.5z"/>';
const SMALLSUN='<circle cx="8.5" cy="8.5" r="3"/><path d="M8.5 2.8v1.4M2.8 8.5h1.4M4.5 4.5l1 1M12.5 4.5l-1 1"/>';
const SMALLMOON='<path d="M11.4 9.8A4.6 4.6 0 0 1 5.6 4a4.6 4.6 0 1 0 5.8 5.8z"/>';
const CLOUD2='<path d="M9.5 20h8a3.3 3.3 0 0 0 .2-6.6 4.6 4.6 0 0 0-8.7 1.1A2.8 2.8 0 0 0 9.5 20z"/>';
function icon(kind,day){
  let p;
  switch(kind){
    case'clear':p=day?SUN:MOON;break;
    case'part':p=(day?SMALLSUN:SMALLMOON)+CLOUD2;break;
    case'fog':p='<path d="M4 9h16M3 13h18M5 17h14"/>';break;
    case'drizzle':p=CLOUD+'<path d="M9 21v.5M13 21v.5M17 21v.5"/>';break;
    case'rain':p=CLOUD.replace('18.5','15.5').replace('A3.2 3.2 0 0 0 7 18.5','A3.2 3.2 0 0 0 7 15.5')+'<path d="M8.5 18.5l-1 3M12.5 18.5l-1 3M16.5 18.5l-1 3"/>';break;
    case'snow':p=CLOUD.replace('18.5','15.5').replace('A3.2 3.2 0 0 0 7 18.5','A3.2 3.2 0 0 0 7 15.5')+'<path d="M9 19.5h.01M13 21h.01M17 19.5h.01M11 22.5h.01M15 22.5h.01"/>';break;
    case'storm':p=CLOUD.replace('18.5','15.5').replace('A3.2 3.2 0 0 0 7 18.5','A3.2 3.2 0 0 0 7 15.5')+'<path d="M12.5 15.5l-2 4h3l-2 4"/>';break;
    default:p=CLOUD;
  }
  return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'+p+'</svg>';
}
function aqiWord(a){return a<=50?'good':a<=100?'moderate':a<=150?'unhealthy for some':a<=200?'unhealthy':a<=300?'very unhealthy':'hazardous'}
const deg=v=>Math.round(v)+'°';
const pad=n=>String(n).padStart(2,'0');

// The board's clock, kept here: its time at the moment of the last answer, plus how long ago
// that was. Shown in the city's own time zone — the weather brings the offset.
function boardNow(){return Date.now()+delta}
function local(ms){return new Date(ms+(S?S.off:0)*1000)}   // read with getUTC*

function tick(){
  if(!S)return;
  const synced=S.now>1700000000;
  const L=local(boardNow());
  const h=L.getUTCHours(),m=L.getUTCMinutes(),s=L.getUTCSeconds();
  if(synced){
    const h12=S.clock24?pad(h):String((h+11)%12+1);
    $('hm').textContent=h12+':'+pad(m);
    $('sec').textContent=pad(s);
    $('ampm').textContent=S.clock24?'':(h<12?'am':'pm');
    $('date').textContent=DAYS[L.getUTCDay()]+', '+L.getUTCDate()+' '+MONTHS[L.getUTCMonth()];
  }
  const part=h<5?'night':h<12?'morning':h<17?'afternoon':h<21?'evening':'night';
  $('greet').textContent='Good '+part+(S.name?', '+S.name:'');
  theme();
  if(S.news&&S.news.t.length&&Date.now()-headAt>15000){
    headAt=Date.now();headI=(headI+1)%S.news.t.length;
    $('head').style.opacity=0;
    setTimeout(()=>{$('head').textContent=S.news.t[headI];$('head').style.opacity=1},800);
  }
}

// Day: white on black. Night: black on white. Decided by the city's sunrise and sunset; before
// the first weather arrives, 07:00–19:00 local stands in.
function theme(){
  let day;
  if(S.theme===1)day=true;else if(S.theme===2)day=false;
  else if(S.days&&S.days.length){
    // Each day's `t` is the epoch of that day's local midnight (timezone=auto, unixtime).
    const t=boardNow()/1000,d=S.days.find(x=>t>=x.t&&t<x.t+86400)||S.days[0];
    day=t>=d.r&&t<d.s;
  } else {const h=local(boardNow()).getUTCHours();day=h>=7&&h<19}
  document.body.classList.toggle('day',day);
  document.body.classList.toggle('night',!day);
}

function render(){
  document.body.className=document.body.className.replace(/\bv\d\b/,'v'+S.view);
  $('hinturl').textContent='http://'+S.host+'.local/settings';
  // weather
  if(S.wx){
    const [word,kind]=sky(S.wx.c);
    $('city').textContent=S.city;
    $('icon').innerHTML=icon(kind,S.wx.d);
    $('temp').textContent=deg(S.wx.t);
    $('desc').textContent=word+' · feels '+deg(S.wx.f);
    $('more1').textContent='Wind '+Math.round(S.wx.w)+(S.units==='f'?' mph':' km/h')+' · Humidity '+S.wx.h+'%'+(S.aqi>=0?' · Air '+aqiWord(S.aqi):'');
    if(S.days.length){const d=S.days[0],f=t=>{const L=local(t*1000);return pad(S.clock24?L.getUTCHours():(L.getUTCHours()+11)%12+1)+':'+pad(L.getUTCMinutes())};
      $('more2').textContent='Sunrise '+f(d.r)+' · Sunset '+f(d.s)}
    const rows=S.days.slice(1,6).map((d,i)=>{const L=local(d.t*1000);
      return '<tr style="opacity:'+(1-i*.16)+'"><td>'+DAYS[L.getUTCDay()]+'</td><td class="ic">'+icon(sky(d.c)[1],1)+'</td><td class="n">'+deg(d.hi)+'</td><td class="n faint">'+deg(d.lo)+'</td></tr>'});
    $('fc').innerHTML=rows.join('');
  } else {
    $('city').textContent=S.place?S.city:'';
    $('icon').innerHTML='';$('temp').textContent='';$('fc').innerHTML='';$('more2').textContent='';
    $('desc').textContent=S.place?'fetching the weather…':'';
    $('more1').textContent=S.place?'':'Choose your city at http://'+S.host+'.local/settings';
  }
  // holiday
  if(S.hol){const p=S.hol.d.split('-');$('hol').textContent='Next holiday · '+S.hol.n+' · '+(+p[2])+' '+MONTHS[+p[1]-1]}
  else $('hol').textContent='';
  // on this day — one of the five, changing each hour
  if(S.otd.length){
    const e=S.otd[local(boardNow()).getUTCHours()%S.otd.length];
    $('otdlab').textContent='On this day';
    $('otd1').textContent=e.y+' — '+e.t;
    $('otd2').textContent='On this day in '+e.y+': '+e.t;
  }
  // news
  if(S.news&&S.news.t.length){
    $('newslab').textContent='Headlines · '+S.news.src;
    if(!$('head').textContent){headI=0;headAt=Date.now();$('head').textContent=S.news.t[0]}
    const key=S.news.t.join('|');
    if(key!==lastPaint){lastPaint=key;$('newslist').innerHTML='';
      for(const t of S.news.t){const li=document.createElement('li');li.textContent=t;$('newslist').appendChild(li)}}
  }
  // status
  const age=S.age.wx>=0?S.age.wx:-1;
  $('foot1').textContent=S.host+'.local · '+(S.err?S.err:'on your WiFi');
  $('foot2').textContent=age<0?'':age<90?'updated just now':'updated '+Math.round(age/60)+' min ago';
}

async function poll(){
  try{
    const t0=Date.now();
    const d=await (await fetch('/api/data',{cache:'no-store'})).json();
    const t1=Date.now();
    delta=d.now*1000-(t0+t1)/2;
    S=d;got=Date.now();render();tick();
  }catch(e){
    if(Date.now()-got>20000)$('foot1').textContent='cannot reach the board';
  }
  setTimeout(poll,5000);
}

// Full screen needs a tap — browsers do not allow it otherwise. The cursor hides when idle.
document.addEventListener('click',()=>{
  const el=document.documentElement;
  if(!document.fullscreenElement&&el.requestFullscreen)el.requestFullscreen().catch(()=>{});
  $('hint').classList.add('gone');
});
let idle;
document.addEventListener('mousemove',()=>{document.body.classList.remove('idle');clearTimeout(idle);idle=setTimeout(()=>document.body.classList.add('idle'),3000)});
setTimeout(()=>$('hint').classList.add('gone'),8000);
setInterval(tick,1000);
poll();
</script>
</body></html>
)HTML";
