#pragma once
//
// /settings — the mirror's settings, from a phone on the same WiFi. Same look as every other
// page in the book (ui.h).

static const char SETTINGS_BODY[] PROGMEM = R"HTML(
<style>
.place{display:block;width:100%;text-align:left;margin:0 0 8px;background:var(--card2);color:var(--ink);
  border:1px solid var(--line);font-weight:600}
.place small{display:block;color:var(--mute);font-weight:500;font-size:12px}
.seg{display:grid;grid-auto-flow:column;grid-auto-columns:1fr;gap:8px}
.seg button{background:var(--card2);color:var(--ink);border:1px solid var(--line);padding:11px 6px;font-size:13.5px}
.seg button.on{border-color:rgba(255,184,108,.6);box-shadow:0 0 0 3px rgba(255,184,108,.12)}
.ok{color:var(--mint);font-size:13px;min-height:18px;margin:10px 0 0}
.err{color:var(--red);font-size:13px;min-height:18px;margin:8px 0 0}
select{appearance:none}
.chosen{font-weight:700;margin:4px 0 10px}
</style>
<header><span class="dot"></span><div><h1>Mirror settings</h1>
<p class="sub"><a href="/">open the mirror</a></p></div></header>

<div class="card">
  <p class="label">your name, for the greeting</p>
  <input id="name" maxlength="20" placeholder="leave empty for just “Good morning”">
</div>

<div class="card">
  <p class="label">city — for the weather, sunrise and sunset, holidays</p>
  <p class="chosen" id="chosen">none chosen yet</p>
  <div class="row"><input id="q" placeholder="type a city"><button class="narrow" id="find">find</button></div>
  <p class="err" id="qerr"></p>
  <div id="places"></div>
</div>

<div class="card">
  <p class="label">temperature</p>
  <div class="seg" id="units"><button data-v="c">°C · km/h</button><button data-v="f">°F · mph</button></div>
  <p class="label" style="margin-top:14px">clock</p>
  <div class="seg" id="clock"><button data-v="1">24-hour</button><button data-v="0">12-hour</button></div>
  <p class="label" style="margin-top:14px">screen</p>
  <div class="seg" id="theme"><button data-v="0">by the sun</button><button data-v="1">always black</button><button data-v="2">always white</button></div>
  <p class="sub" style="margin-top:8px">By the sun: white text on black by day, black text on white at night.</p>
</div>

<div class="card">
  <p class="label">headlines</p>
  <select id="feed"></select>
  <input id="custom" placeholder="https://… any RSS feed" style="margin-top:10px;display:none">
</div>

<button id="save">save</button>
<p class="ok" id="ok"></p><p class="err" id="serr"></p>

<div class="card" style="margin-top:18px">
  <p class="label">WiFi</p>
  <p id="wifi" class="sub">—</p>
  <button class="ghost" id="forget" style="margin-top:10px">forget this WiFi and restart</button>
</div>

<script>
const $=i=>document.getElementById(i);
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());
const enc=encodeURIComponent;
let place=null;

function seg(id,val){document.querySelectorAll('#'+id+' button').forEach(b=>{
  b.classList.toggle('on',b.dataset.v===String(val));
  b.onclick=()=>seg(id,b.dataset.v)})}
const segVal=id=>document.querySelector('#'+id+' button.on').dataset.v;
const label=p=>[p.name,p.region,p.country].filter(Boolean).join(', ');

async function load(){
  const s=await (await fetch('/api/settings',{cache:'no-store'})).json();
  $('name').value=s.name;
  if(s.place){place=s.place;$('chosen').textContent=label(s.place)}
  seg('units',s.units);seg('clock',s.clock24?1:0);seg('theme',s.theme);
  $('feed').innerHTML='';
  s.feeds.forEach((f,i)=>{const o=document.createElement('option');o.value=i;o.textContent=f;$('feed').appendChild(o)});
  const o=document.createElement('option');o.value=-1;o.textContent='my own RSS link…';$('feed').appendChild(o);
  $('feed').value=s.feed;$('custom').value=s.custom;$('custom').style.display=s.feed<0?'block':'none';
  $('wifi').textContent='Connected to '+s.ssid+' · signal '+s.rssi+' dBm · '+s.ip;
}
$('feed').onchange=()=>{$('custom').style.display=$('feed').value==='-1'?'block':'none'};

$('find').onclick=async()=>{
  const q=$('q').value.trim();if(!q)return;
  $('qerr').textContent='';$('places').innerHTML='<p class="sub">searching…</p>';
  try{
    const r=await (await fetch('/api/places?q='+enc(q))).json();
    $('places').innerHTML='';
    if(!r.ok){$('qerr').textContent='The search did not answer — try again.';return}
    if(!r.places.length){$('qerr').textContent='No city by that name.';return}
    for(const p of r.places){
      const b=document.createElement('button');b.type='button';b.className='place';
      b.textContent=p.name;const sm=document.createElement('small');sm.textContent=[p.region,p.country].filter(Boolean).join(', ');
      b.appendChild(sm);
      b.onclick=()=>{place=p;$('chosen').textContent=label(p);$('places').innerHTML=''};
      $('places').appendChild(b);
    }
  }catch(e){$('places').innerHTML='';$('qerr').textContent='The board did not answer.'}
};

$('save').onclick=async()=>{
  $('ok').textContent='';$('serr').textContent='';
  let b='name='+enc($('name').value.trim())+'&units='+segVal('units')+'&clock24='+segVal('clock')+
        '&theme='+segVal('theme')+'&feed='+$('feed').value+'&custom='+enc($('custom').value.trim());
  if(place)b+='&pn='+enc(place.name)+'&pr='+enc(place.region||'')+'&pc='+enc(place.country||'')+
              '&pcc='+enc(place.cc||'')+'&lat='+place.lat+'&lon='+place.lon;
  try{const d=await post('/api/settings',b);
    if(d.ok)$('ok').textContent='Saved — the mirror updates in a few seconds.';else $('serr').textContent=d.err||'Not saved.'}
  catch(e){$('serr').textContent='The board did not answer.'}
};

$('forget').onclick=async()=>{
  if(!confirm('Forget this WiFi? The board restarts into its setup network, devkit-mirror-setup.'))return;
  try{await post('/api/forget-wifi','x=1')}catch(e){}
  $('wifi').textContent='Forgotten — the board is restarting. Join devkit-mirror-setup to set it up again.';
};
load().catch(()=>{$('serr').textContent='Could not load the settings.'});
</script>
)HTML";
