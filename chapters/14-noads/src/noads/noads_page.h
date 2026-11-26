#pragma once
// The dashboard: what's being blocked, a pause switch, your own lists, and the settings.

static const char NOADS_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>No ads</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
:root{--bg:#f4f6f8;--card:#fff;--line:#e3e7ec;--ink:#121820;--mute:#64707f;--acc:#0f766e;--acc-ink:#fff;--soft:#e6f4f2;
 --bad:#dc2626;--bad-soft:#fdecec;--warn:#b45309;--warn-soft:#fff4e5;--ok:#15803d;--bar:#cbd5df;color-scheme:light}
@media (prefers-color-scheme:dark){:root{--bg:#0c1117;--card:#151b23;--line:#27303b;--ink:#e6ebf1;--mute:#8693a3;--acc:#2dd4bf;--acc-ink:#04201c;
 --soft:#10302d;--bad:#f87171;--bad-soft:#3a1717;--warn:#f59e0b;--warn-soft:#33240d;--ok:#4ade80;--bar:#2f3a47;color-scheme:dark}}
body{background:var(--bg);color:var(--ink);font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,system-ui,sans-serif;
 padding:max(16px,env(safe-area-inset-top)) 16px 40px;-webkit-font-smoothing:antialiased}
.wrap{max-width:760px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin-bottom:16px}
.mark{width:38px;height:38px;border-radius:11px;background:var(--acc);display:grid;place-items:center;flex:none}
svg.i{width:20px;height:20px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.mark svg{stroke:var(--acc-ink)}
h1{font-size:19px;font-weight:650}.sub{color:var(--mute);font-size:13px}
.grow{flex:1;min-width:0}
.pill{display:inline-flex;align-items:center;gap:6px;padding:5px 11px;border-radius:999px;font-size:13px;font-weight:600;background:var(--soft);color:var(--acc);white-space:nowrap}
.pill.paused{background:var(--warn-soft);color:var(--warn)}.pill.off{background:var(--bad-soft);color:var(--bad)}
.pill i{width:7px;height:7px;border-radius:50%;background:currentColor}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
h2{font-size:14px;font-weight:650;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;gap:8px}
h2 small{font-weight:400;color:var(--mute);font-size:12px}
.nums{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}
.num{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.num b{display:block;font-size:24px;font-weight:700;letter-spacing:-.02em;font-variant-numeric:tabular-nums}
.num span{color:var(--mute);font-size:12px}
@media (max-width:560px){.nums{grid-template-columns:repeat(2,1fr)}}
.chart{display:flex;align-items:flex-end;gap:2px;height:90px}
.chart div{flex:1;background:var(--bar);border-radius:2px 2px 0 0;position:relative;min-height:1px}
.chart div i{position:absolute;left:0;right:0;bottom:0;background:var(--bad);border-radius:2px 2px 0 0;opacity:.85}
.legend{display:flex;gap:14px;color:var(--mute);font-size:12px;margin-top:8px}.legend i{display:inline-block;width:9px;height:9px;border-radius:2px;margin-right:5px;vertical-align:-1px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
input,select{font:inherit;padding:9px 11px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--ink);min-width:0}
input{flex:1}
button{font:inherit;font-weight:600;padding:9px 14px;border-radius:10px;border:0;background:var(--acc);color:var(--acc-ink);cursor:pointer;white-space:nowrap}
button.ghost{background:transparent;color:var(--ink);border:1px solid var(--line)}
button.small{padding:4px 9px;font-size:12px;border-radius:8px}
button:disabled{opacity:.5}
.list{display:flex;flex-direction:column}
.li{display:flex;align-items:center;gap:10px;padding:8px 0;border-top:1px solid var(--line);font-size:14px;min-width:0}
.li:first-child{border-top:0}
.li .nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.li .m{color:var(--mute);font-size:12px;font-variant-numeric:tabular-nums;white-space:nowrap}
.tag{font-size:11px;font-weight:650;padding:2px 7px;border-radius:6px;white-space:nowrap}
.tag.b{background:var(--bad-soft);color:var(--bad)}.tag.a{background:var(--soft);color:var(--acc)}.tag.c{background:var(--bg);color:var(--mute)}
.two{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:14px}.two>*{min-width:0}
@media (max-width:640px){.two{grid-template-columns:1fr}}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.chip{display:inline-flex;align-items:center;gap:6px;padding:4px 6px 4px 10px;border-radius:999px;background:var(--bg);border:1px solid var(--line);font-size:13px}
.chip button{background:transparent;color:var(--mute);padding:0 4px;font-size:15px;line-height:1}
.empty{color:var(--mute);font-size:13px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:6px 14px;font-size:14px}.kv span{color:var(--mute)}
.mono{font-family:ui-monospace,Menlo,monospace}
.hint{color:var(--mute);font-size:13px;margin-top:8px}
.test{margin-top:10px;font-size:14px;min-height:20px}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:var(--ink);color:var(--bg);padding:10px 16px;border-radius:10px;font-size:14px;max-width:90vw}
details summary{cursor:pointer;font-weight:650;font-size:14px;list-style:none}details summary::-webkit-details-marker{display:none}
details[open] summary{margin-bottom:12px}
</style></head><body><div class="wrap">
<svg width="0" height="0" style="position:absolute">
<symbol id="i-shield" viewBox="0 0 24 24"><path d="M12 3l8 3v6c0 4.5-3.4 8.3-8 9-4.6-.7-8-4.5-8-9V6z"/><path d="M8.5 12.5l2.5 2.5 4.5-5"/></symbol>
</svg>
<header><div class="mark"><svg class="i"><use href="#i-shield"/></svg></div>
 <div class="grow"><h1>No ads</h1><div class="sub" id="sub">Ad blocking for every device on this WiFi</div></div>
 <span class="pill" id="pill"><i></i><span id="pillText">Blocking</span></span></header>

<div class="nums">
 <div class="num"><b id="nTotal">0</b><span>lookups</span></div>
 <div class="num"><b id="nBlocked">0</b><span id="nPct">blocked</span></div>
 <div class="num"><b id="nDevices">0</b><span>on NoAds now</span></div>
 <div class="num"><b id="nList">0</b><span>names on the list</span></div>
</div>

<div class="card"><h2>Last hour <small id="perMin"></small></h2><div class="chart" id="chart"></div>
 <div class="legend"><span><i style="background:var(--bar)"></i>allowed</span><span><i style="background:var(--bad)"></i>blocked</span></div></div>

<div class="card"><h2>Pause blocking <small>for when a site won't work</small></h2>
 <div class="row"><button class="ghost" data-pause="5">5 minutes</button><button class="ghost" data-pause="30">30 minutes</button>
 <button class="ghost" data-pause="60">1 hour</button><button id="resume" hidden>Resume now</button></div></div>

<div class="two">
 <div class="card"><h2>Live <small>newest first</small></h2><div class="list" id="recent"></div></div>
 <div>
  <div class="card"><h2>Blocked most</h2><div class="list" id="top"></div></div>
  <div class="card"><h2>Devices</h2><div class="list" id="clients"></div></div>
 </div>
</div>

<div class="card"><h2>Check a site</h2>
 <form class="row" id="testForm"><input id="testName" placeholder="ads.example.com" autocapitalize="off" autocomplete="off" spellcheck="false"><button>Check</button></form>
 <div class="test" id="testOut"></div></div>

<div class="two">
 <div class="card"><h2>Always allow</h2>
  <form class="row" id="allowForm"><input id="allowName" placeholder="shop.example.com" autocapitalize="off" spellcheck="false"><button>Add</button></form>
  <div class="chips" id="allowChips"></div><div class="hint">A site and everything under it. Wins over every list.</div></div>
 <div class="card"><h2>Always block</h2>
  <form class="row" id="blockForm"><input id="blockName" placeholder="tiktok.com" autocapitalize="off" spellcheck="false"><button>Add</button></form>
  <div class="chips" id="blockChips"></div><div class="hint">Blocked even while paused.</div></div>
</div>

<div class="card"><h2>Connect a device</h2>
 <div class="kv"><span>WiFi name</span><b id="apName"></b><span>Password</span><b class="mono" id="apPass"></b></div>
 <div class="hint">Join this network on a phone or laptop: its ads are blocked, and the internet comes through the board.
 Turn off <b>Private DNS</b> (Android) or <b>Secure DNS</b> (Chrome), or the device skips the board.</div>
 <details style="margin-top:12px"><summary>Every device at once: router mode</summary>
 <div class="hint">In your router's settings, set its DNS server to <b class="mono" id="homeIp"></b> (and give the board that
 address for good, a "DHCP reservation"). Every device on <b id="homeName"></b> is then covered at full speed, without joining NoAds.
 If the board is ever off, put the router's DNS back.</div></details></div>

<div class="card"><details><summary>Settings</summary>
 <h2>Blocklist <small id="listInfo"></small></h2>
 <div class="row"><select id="listSel"></select><button class="ghost" id="update">Update now</button></div>
 <div class="hint" id="note"></div>
 <h2 style="margin-top:16px">The NoAds network</h2>
 <form class="row" id="apForm"><input id="apNameIn" maxlength="32" placeholder="Name"><input id="apPassIn" maxlength="63" placeholder="New password (8+)"><button>Save</button></form>
 <div class="hint">Devices on NoAds will need to join again.</div>
 <h2 style="margin-top:16px">Asking the internet</h2>
 <form class="row" id="dnsForm"><input id="dns1" class="mono"><input id="dns2" class="mono"><button>Save</button></form>
 <div class="hint">Allowed lookups go to these DNS servers: Cloudflare 1.1.1.1 and Google 8.8.8.8 to start with.</div>
</details></div>
</div>
<div class="toast" id="toast" hidden></div>
<script>
const $=id=>document.getElementById(id);
let S=null,timer=0,toastT=0;
const V={0:['allowed','a'],1:['blocked','b'],2:['your block','b'],3:['your allow','a'],4:['blocked','b'],9:['cached','c']};
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function toast(t){$('toast').textContent=t;$('toast').hidden=false;clearTimeout(toastT);toastT=setTimeout(()=>$('toast').hidden=true,2600);}
function fmt(n){return n>=1e6?(n/1e6).toFixed(1)+'M':n>=1e4?Math.round(n/1e3)+'k':n.toLocaleString();}
function ago(s){return s<5?'now':s<60?s+'s':Math.round(s/60)+'m';}
async function call(path,data){const r=await fetch(path,data?{method:'POST',body:new URLSearchParams(data)}:{});return r.json();}
function render(){
 const s=S;
 const pill=$('pill');if(s.paused){pill.className='pill paused';$('pillText').textContent='Paused, '+Math.ceil(s.paused/60)+' min left';}
 else if(!s.list.count){pill.className='pill off';$('pillText').textContent=s.updating?'Getting the list':'No list yet';}
 else{pill.className='pill';$('pillText').textContent='Blocking';}
 $('resume').hidden=!s.paused;
 $('sub').textContent=s.home?'On '+s.home+' · NoAds at '+s.apIp:'Ad blocking for every device on this WiFi';
 $('nTotal').textContent=fmt(s.total);$('nBlocked').textContent=fmt(s.blocked);
 $('nPct').textContent=s.total?Math.round(s.blocked*100/s.total)+'% blocked':'blocked';
 $('nDevices').textContent=s.devices;$('nList').textContent=fmt(s.list.count);
 const max=Math.max(1,...s.minutes.map(m=>m[0]));
 $('chart').innerHTML=s.minutes.map(m=>'<div style="height:'+(m[0]/max*100)+'%" title="'+m[0]+' lookups, '+m[1]+' blocked"><i style="height:'+(m[0]?m[1]/m[0]*100:0)+'%"></i></div>').join('');
 const last=s.minutes[s.minutes.length-1];$('perMin').textContent=last[0]+' this minute';
 $('recent').innerHTML=s.recent.length?s.recent.slice(0,25).map(r=>{const v=V[r[2]]||V[0],b=v[1]==='b';
  return '<div class="li"><span class="tag '+v[1]+'">'+v[0]+'</span><span class="nm" title="'+esc(r[0])+'">'+esc(r[0])+'</span><span class="m">'+esc(r[1].split('.').pop())+' · '+ago(r[3])+'</span>'+
  '<button class="ghost small" data-act="'+(b?'allow':'block')+'" data-name="'+esc(r[0])+'">'+(b?'Allow':'Block')+'</button></div>';}).join(''):'<div class="empty">Nothing yet. Join NoAds on a device and open a site.</div>';
 $('top').innerHTML=s.top.length?s.top.slice(0,10).map(t=>'<div class="li"><span class="nm" title="'+esc(t[0])+'">'+esc(t[0])+'</span><span class="m">'+fmt(t[1])+'</span><button class="ghost small" data-act="allow" data-name="'+esc(t[0])+'">Allow</button></div>').join(''):'<div class="empty">Nothing blocked yet.</div>';
 $('clients').innerHTML=s.clients.length?s.clients.sort((a,b)=>b[1]-a[1]).map(c=>'<div class="li"><span class="nm mono">'+esc(c[0])+'</span><span class="m">'+fmt(c[1])+' lookups · '+(c[1]?Math.round(c[2]*100/c[1]):0)+'% blocked</span></div>').join(''):'<div class="empty">No devices yet.</div>';
 const chips=(l,kind)=>l.length?l.map(n=>'<span class="chip">'+esc(n)+'<button data-remove="'+kind+'" data-name="'+esc(n)+'" aria-label="Remove">&times;</button></span>').join(''):'<span class="empty">None.</span>';
 $('allowChips').innerHTML=chips(s.allow,'allow');$('blockChips').innerHTML=chips(s.block,'block');
 $('apName').textContent=s.ap;$('apPass').textContent=s.pass;$('homeIp').textContent=s.homeIp;$('homeName').textContent=s.home||'your WiFi';
 if(!$('listSel').options.length)$('listSel').innerHTML=s.lists.map(l=>'<option value="'+l[0]+'">'+esc(l[1])+'</option>').join('');
 if(document.activeElement!==$('listSel'))$('listSel').value=s.list.id;
 $('listInfo').textContent=s.list.count?fmt(s.list.count)+' names'+(s.list.built>1.7e9?', updated '+new Date(s.list.built*1000).toLocaleDateString():''):'';
 $('note').textContent=(s.note||'Updated by itself once a week, in the night.')+(s.detail?' ('+s.detail+')':'');
 for(const [i,id] of [[0,'dns1'],[1,'dns2']])if(document.activeElement!==$(id)&&!$(id).value)$(id).value=s.dns[i];
}
async function poll(){clearTimeout(timer);let again=document.hidden?15000:2000;
 try{const j=await call('/state');if(j.ok){S=j;render();}}catch(e){again=4000;}timer=setTimeout(poll,again);}
async function act(path,data,ok){try{const j=await call(path,data);if(j.ok){if(j.total!==undefined){S=j;render();}if(ok)toast(ok);}
 else toast({full:'That list is full (32 names)',name:'That is not a site name',gone:'Already removed',pass:'The password needs 8 to 63 characters',ap:'Name too long',busy:'Already updating'}[j.err]||'Could not do that');return j;}catch(e){toast('Board not reachable');}}
document.addEventListener('click',e=>{const p=e.target.closest('[data-pause]');if(p)act('/pause',{min:p.dataset.pause},'Paused for '+p.dataset.pause+' minutes');
 const a=e.target.closest('[data-act]');if(a)act('/'+a.dataset.act,{name:a.dataset.name},(a.dataset.act==='allow'?'Allowed: ':'Blocked: ')+a.dataset.name);
 const r=e.target.closest('[data-remove]');if(r)act('/'+r.dataset.remove,{name:r.dataset.name,do:'remove'});});
$('resume').onclick=()=>act('/pause',{min:0},'Blocking again');
$('allowForm').onsubmit=e=>{e.preventDefault();act('/allow',{name:$('allowName').value}).then(j=>{if(j&&j.ok)$('allowName').value='';});};
$('blockForm').onsubmit=e=>{e.preventDefault();act('/block',{name:$('blockName').value}).then(j=>{if(j&&j.ok)$('blockName').value='';});};
$('testForm').onsubmit=async e=>{e.preventDefault();const j=await act('/lookup',{name:$('testName').value});
 if(j&&j.ok){const v=V[j.verdict]||V[0];$('testOut').innerHTML='<span class="tag '+v[1]+'">'+v[0]+'</span> '+esc(j.name)+
  (j.verdict===1?' is on the blocklist':j.verdict===0?' loads normally':j.verdict===2?' is on your block list':j.verdict===3?' is on your allow list':'');}};
$('listSel').onchange=()=>{if(!confirm('Switching lists restarts the board to download it: NoAds is off for about 3 minutes. Go ahead?')){$('listSel').value=S.list.id;return;}act('/settings',{list:$('listSel').value},'Restarting to fetch the new list: back in about 3 minutes');};
$('update').onclick=()=>{if(confirm('The board restarts to update the list: NoAds is off for about 3 minutes. Go ahead?'))act('/update',null,'Restarting to update: back in about 3 minutes');};
$('apForm').onsubmit=e=>{e.preventDefault();const d={};if($('apNameIn').value)d.ap=$('apNameIn').value;if($('apPassIn').value)d.pass=$('apPassIn').value;
 if(!d.ap&&!d.pass)return;if(!confirm('Devices on NoAds will be disconnected and must join again. Change it?'))return;act('/settings',d,'Saved');};
$('dnsForm').onsubmit=e=>{e.preventDefault();act('/settings',{dns1:$('dns1').value,dns2:$('dns2').value},'Saved');};
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll();});
// the quiet hours for weekly updates are the board's night by this browser's clock
call('/settings',{tz:-new Date().getTimezoneOffset()}).catch(()=>{});
poll();
</script></body></html>)HTML";
