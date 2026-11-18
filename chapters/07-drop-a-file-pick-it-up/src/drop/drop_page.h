#pragma once
//
// The drop page: the devices here, a file to send, an incoming offer to accept, and notes.
// Same look as every page in the book (ui.h).

static const char DROP_BODY[] PROGMEM = R"HTML(
<style>
.view{display:none}
.err{color:var(--red);font-size:13px;min-height:18px;margin:8px 0 0}
.ok{color:var(--mint);font-size:13px;min-height:18px;margin:8px 0 0}
.me{display:flex;align-items:center;gap:10px}
.me input{flex:1}
.pick{display:block;border:2px dashed var(--line);border-radius:16px;padding:22px 14px;text-align:center;cursor:pointer;
  color:var(--mute);transition:border-color .2s,background .2s}
.pick.over{border-color:rgba(255,184,108,.7);background:rgba(255,184,108,.06)}
.pick b{display:block;color:var(--ink);font-size:15px;margin-bottom:2px;overflow-wrap:anywhere}
.pick input{display:none}
.devs{display:flex;flex-direction:column;gap:8px;margin-top:12px}
.dev{display:flex;align-items:center;gap:10px;width:100%;text-align:left;background:var(--card2);color:var(--ink);
  border:1px solid var(--line);font-weight:600}
.dev .ic{width:28px;text-align:center;font-size:18px}
.dev span.n{flex:1}
.dev.sel{border-color:rgba(255,184,108,.65);box-shadow:0 0 0 3px rgba(255,184,108,.12)}
.none{color:var(--faint);font-size:13px;padding:10px 2px}
.xfer{border-color:rgba(126,231,199,.35)}
.xfer h3{margin:0 0 4px;font-size:16px}
.xfer .row{margin-top:12px}
a.btn{display:block;text-align:center;text-decoration:none;font-weight:750;border-radius:14px;padding:13px 15px;color:#14161d;
  background:linear-gradient(135deg,var(--mint),#5ec8f0)}
.note{border-top:1px solid var(--line);padding:12px 0}
.note:first-child{border-top:0;padding-top:4px}
.note pre{margin:0 0 8px;white-space:pre-wrap;overflow-wrap:anywhere;font:14px/1.5 ui-monospace,Menlo,monospace;
  max-height:9em;overflow:auto;color:var(--ink)}
.note .meta{display:flex;align-items:center;gap:8px;color:var(--faint);font-size:12px}
.note .meta span{flex:1}
.note button{width:auto;padding:7px 12px;font-size:12.5px}
textarea{resize:vertical;min-height:84px}
.small{font-size:12.5px;color:var(--mute)}
details summary{cursor:pointer;color:var(--mute);font-size:13px;padding:4px 0}
.set{display:flex;align-items:center;gap:10px;margin:10px 0}
.set label{flex:0 0 120px;color:var(--mute);font-size:13px}
.set select,.set input{flex:1}
</style>

<header><span class="dot"></span><div><h1>Drop</h1><p class="sub" id="sub">files pass through, nothing is kept</p></div></header>

<!-- ── PIN ───────────────────────────────────────────────────────── -->
<div class="view" id="v-lock">
  <div class="card">
    <p class="label">this drop has a PIN</p>
    <div class="row"><input id="pin" inputmode="numeric" maxlength="8" placeholder="PIN" autocomplete="off">
      <button class="narrow" id="pinGo">open</button></div>
    <p class="err" id="pinErr"></p>
  </div>
</div>

<div class="view" id="v-main">

  <!-- an offer coming in, or the one this device is sending -->
  <div class="card xfer" id="xfer" style="display:none"></div>

  <div class="card">
    <p class="label">this device</p>
    <div class="me"><span id="meIcon" style="font-size:20px"></span>
      <input id="meName" maxlength="20" placeholder="name this device"></div>
  </div>

  <div class="card">
    <p class="label">send a file</p>
    <label class="pick" id="pick"><input type="file" id="file"><b id="fileName">choose a file</b>
      <span id="fileInfo">or drop it here · any size</span></label>
    <div class="devs" id="devs"></div>
    <button id="send" style="margin-top:12px" disabled>send</button>
    <p class="err" id="sendErr"></p>
  </div>

  <div class="card">
    <p class="label">notes — kept for everyone here, until they expire</p>
    <textarea id="text" maxlength="8192" placeholder="paste a link, a note, a code…"></textarea>
    <button class="mint" id="textGo" style="margin-top:10px">save note</button>
    <p class="err" id="textErr"></p>
    <div id="notes" style="margin-top:8px"></div>
  </div>

  <details class="card">
    <summary>settings</summary>
    <div class="set"><label for="hours">notes clear after</label>
      <select id="hours"><option value="1">1 hour</option><option value="24">24 hours</option>
        <option value="168">7 days</option><option value="0">never</option></select></div>
    <div class="set"><label for="newPin">PIN</label><input id="newPin" inputmode="numeric" maxlength="8" placeholder="4–8 digits"></div>
    <div class="set" id="oldRow"><label for="oldPin">current PIN</label><input id="oldPin" inputmode="numeric" maxlength="8"></div>
    <div class="row"><button id="setSave">save settings</button><button class="ghost" id="pinOff">remove PIN</button></div>
    <p class="ok" id="setOk"></p><p class="err" id="setErr"></p>
    <p class="small" id="netInfo" style="margin-top:12px"></p>
    <div class="row" style="margin-top:6px"><a class="ghost" id="setupLink" href="/setup" style="display:none;text-align:center;padding:10px;border-radius:14px;border:1px solid var(--line);color:var(--ink);text-decoration:none">set up WiFi</a>
      <button class="ghost" id="forget" style="display:none">forget WiFi</button></div>
  </details>
</div>

<script>
const $=i=>document.getElementById(i);
const enc=encodeURIComponent;
let S=null,ver=0,key='',file=null,target='',sending=null,sentOffer='';
const store={get:(k)=>{try{return localStorage.getItem(k)}catch(e){return null}},set:(k,v)=>{try{localStorage.setItem(k,v)}catch(e){}}};

// Who this device is: a random id kept in the browser, and a name guessed from it.
let me=store.get('drop_d');
if(!me||me.length!==8){me=Array.from(crypto.getRandomValues(new Uint8Array(4)),b=>b.toString(16).padStart(2,'0')).join('');store.set('drop_d',me)}
const ua=navigator.userAgent;
const guess=/iPhone/.test(ua)?'iPhone':/iPad/.test(ua)?'iPad':/Android/.test(ua)?'Android phone':/Mac/.test(ua)?'Mac':/Windows/.test(ua)?'Windows PC':/Linux/.test(ua)?'Linux PC':'a device';
const icon=n=>/phone|iphone|android/i.test(n)?'📱':/ipad|tab/i.test(n)?'📱':'💻';
$('meName').value=store.get('drop_n')||guess;
$('meIcon').textContent=icon($('meName').value);
$('meName').oninput=()=>{store.set('drop_n',$('meName').value.trim());$('meIcon').textContent=icon($('meName').value)};
key=store.get('drop_k')||'';

const q=(extra)=>'k='+enc(key)+'&d='+me+'&dn='+enc($('meName').value.trim()||guess)+'&now='+Date.now()+(extra||'');
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());
const size=b=>b<1024?b+' B':b<1048576?(b/1024).toFixed(1)+' KB':b<1073741824?(b/1048576).toFixed(1)+' MB':(b/1073741824).toFixed(2)+' GB';
const ago=s=>s<60?'just now':s<3600?Math.floor(s/60)+' min ago':s<86400?Math.floor(s/3600)+' h ago':Math.floor(s/86400)+' d ago';
function view(v){document.querySelectorAll('.view').forEach(e=>e.style.display=e.id==='v-'+v?'block':'none')}

// Copy without https: the clipboard API only works on secure pages, so fall back to the old way.
function copy(t){
  if(navigator.clipboard&&window.isSecureContext)return navigator.clipboard.writeText(t);
  const a=document.createElement('textarea');a.value=t;a.style.position='fixed';a.style.opacity='0';
  document.body.appendChild(a);a.select();let ok=false;try{ok=document.execCommand('copy')}catch(e){}
  a.remove();return ok?Promise.resolve():Promise.reject();
}

// ── choosing a file ──
function choose(f){if(!f)return;file=f;$('fileName').textContent=f.name;$('fileInfo').textContent=size(f.size)+' · tap to change';refreshSend()}
$('file').onchange=()=>choose($('file').files[0]);
const pick=$('pick');
['dragenter','dragover'].forEach(ev=>pick.addEventListener(ev,e=>{e.preventDefault();pick.classList.add('over')}));
['dragleave','drop'].forEach(ev=>pick.addEventListener(ev,e=>{e.preventDefault();pick.classList.remove('over')}));
pick.addEventListener('drop',e=>choose(e.dataTransfer.files[0]));
function refreshSend(){$('send').disabled=!file||!target||(S&&S.busy)}

function drawDevices(list){
  const box=$('devs');box.innerHTML='';
  if(!list.length){
    const p=document.createElement('p');p.className='none';
    p.textContent='No other device here yet. Open http://'+S.host+'.local (or http://'+S.ip+') on it'+(S.mode==='own'?', after joining '+S.net:'')+'.';
    box.appendChild(p);target='';refreshSend();return;
  }
  if(!list.some(d=>d.id===target))target=list.length===1?list[0].id:'';
  for(const d of list){
    const b=document.createElement('button');b.type='button';b.className='dev'+(d.id===target?' sel':'');
    const i=document.createElement('span');i.className='ic';i.textContent=icon(d.n);
    const n=document.createElement('span');n.className='n';n.textContent=d.n;
    b.append(i,n);b.onclick=()=>{target=d.id;drawDevices(list)};box.appendChild(b);
  }
  refreshSend();
}

// ── sending: offer, wait for the other side to accept, then upload through the pipe ──
$('send').onclick=async()=>{
  $('sendErr').textContent='';
  if(!file||!target)return;
  try{
    const d=await post('/api/offer',q('&to='+target+'&name='+enc(file.name)+'&size='+file.size+'&type='+enc(file.type||'')));
    if(!d.ok){$('sendErr').textContent={busy:'Another transfer is running — wait for it to finish.',gone:'That device has left.',pin:'Enter the PIN again.'}[d.err]||'Could not send.';return}
    sending={id:d.id,file,started:false,xhr:null,loaded:0};
    poll();
  }catch(e){$('sendErr').textContent='The board did not answer.'}
};

function startUpload(o){
  if(!sending||sending.started||sending.id!==o.id)return;
  sending.started=true;
  const x=new XMLHttpRequest();sending.xhr=x;
  x.open('POST','http://'+location.hostname+':'+S.pipe+'/put?o='+o.id+'&k='+enc(key));
  x.setRequestHeader('Content-Type','application/octet-stream');
  x.upload.onprogress=e=>{if(sending){sending.loaded=e.loaded;drawOffer(S.offer)}};
  x.onload=()=>{if(sending)sending.result=x.status===200?'ok':'fail';poll()};
  x.onerror=()=>{if(sending)sending.result='fail';poll()};
  x.send(sending.file);
}

async function cancel(){try{await post('/api/cancel',q())}catch(e){} if(sending&&sending.xhr)sending.xhr.abort();sending=null;poll()}
async function decline(id){try{await post('/api/answer',q('&id='+id+'&yes=0'))}catch(e){}poll()}

function drawOffer(o){
  const box=$('xfer');
  if(!o){box.style.display='none';box.innerHTML='';return}
  box.style.display='block';box.innerHTML='';
  const h=document.createElement('h3'),p=document.createElement('p'),bar=document.createElement('div'),act=document.createElement('div');
  p.className='sub';bar.className='bar mint';bar.style.marginTop='12px';const fill=document.createElement('i');bar.appendChild(fill);act.className='row';
  const pct=n=>Math.min(100,Math.round(n/Math.max(1,o.size)*100));
  const moved=o.role==='from'&&sending?Math.max(sending.loaded,o.sent):o.sent;
  fill.style.width=pct(moved)+'%';
  const btn=(label,cls,fn)=>{const b=document.createElement('button');b.textContent=label;if(cls)b.className=cls;b.onclick=fn;act.appendChild(b)};
  if(o.role==='to'){
    h.textContent=o.fromName+' is sending you a file';
    p.textContent=o.name+' · '+size(o.size);
    if(o.state==='offered'){
      // Accept is the download link itself: a download a browser lets through must start from a tap.
      const a=document.createElement('a');a.className='btn';a.textContent='accept and download';
      a.href='http://'+location.hostname+':'+S.pipe+'/get?o='+o.id+'&k='+enc(key);a.rel='noopener';
      act.appendChild(a);btn('decline','ghost',()=>decline(o.id));box.append(h,p,act);return;
    }
    if(o.state==='accepted'||o.state==='sending'){p.textContent=o.name+' · '+size(moved)+' of '+size(o.size)+' — it is saving to your downloads';box.append(h,p,bar);btn('cancel','ghost',cancel);box.appendChild(act);return}
  }else{
    h.textContent='Sending '+o.name;
    if(o.state==='offered')p.textContent='Waiting for the other device to accept…';
    else if(o.state==='accepted')p.textContent='Accepted — starting…';
    else if(o.state==='sending')p.textContent=size(moved)+' of '+size(o.size);
    if(o.state==='offered'||o.state==='accepted'||o.state==='sending'){box.append(h,p,bar);btn('cancel','ghost',cancel);box.appendChild(act);return}
  }
  if(o.state==='done'){h.textContent=o.role==='to'?'Received '+o.name:'Sent '+o.name;p.textContent=size(o.size)+' — the board kept none of it.';fill.style.width='100%'}
  else if(o.state==='declined'){h.textContent='Declined';p.textContent=o.name+' was not accepted.';fill.style.width='0'}
  else {h.textContent='Not sent';p.textContent=o.why||'Something went wrong.';fill.style.width='0'}
  box.append(h,p);
}

// ── notes ──
$('textGo').onclick=async()=>{
  const t=$('text').value;if(!t.trim())return;$('textErr').textContent='';
  try{const d=await post('/api/text',q('&x='+enc(t)));
    if(d.ok){$('text').value='';ver=0;poll()}else $('textErr').textContent={long:'Up to 8 KB of text.',many:'Too many notes — delete some.'}[d.err]||'Not saved.'}
  catch(e){$('textErr').textContent='The board did not answer.'}
};
function drawNotes(list){
  const box=$('notes');box.innerHTML='';
  if(!list.length){box.innerHTML='<p class="none">No notes.</p>';return}
  for(const n of list.slice().sort((a,b)=>b.at-a.at)){
    const d=document.createElement('div');d.className='note';
    const pre=document.createElement('pre');pre.textContent=n.x;
    const m=document.createElement('div');m.className='meta';
    const s=document.createElement('span');
    const age=S.now&&n.at?S.now-n.at:0;
    s.textContent=n.from+' · '+ago(age)+(S.hours&&S.now&&n.at?' · clears in '+Math.max(0,Math.ceil((S.hours*3600-age)/3600))+' h':'');
    const c=document.createElement('button');c.className='mint';c.textContent='copy';
    c.onclick=()=>copy(n.x).then(()=>{c.textContent='copied'},()=>{c.textContent='select it to copy'});
    const x=document.createElement('button');x.className='ghost';x.textContent='delete';
    x.onclick=async()=>{await post('/api/delete',q('&id='+n.id));ver=0;poll()};
    m.append(s,c,x);d.append(pre,m);box.appendChild(d);
  }
}

// ── PIN and settings ──
$('pinGo').onclick=async()=>{
  $('pinErr').textContent='';
  try{const d=await post('/api/login','pin='+enc($('pin').value));
    if(d.ok){key=d.k;store.set('drop_k',key);ver=0;poll()}
    else $('pinErr').textContent=d.err==='locked'?'Too many tries — wait a minute.':'Wrong PIN.'}
  catch(e){$('pinErr').textContent='The board did not answer.'}
};
$('setSave').onclick=async()=>{
  $('setOk').textContent=$('setErr').textContent='';
  let b=q('&hours='+$('hours').value);
  if($('newPin').value)b+='&pin='+enc($('newPin').value)+'&old='+enc($('oldPin').value);
  try{const d=await post('/api/settings',b);
    if(d.ok){if(d.k){key=d.k;store.set('drop_k',key)}$('newPin').value=$('oldPin').value='';$('setOk').textContent='Saved.';ver=0;poll()}
    else $('setErr').textContent={old:'The current PIN is wrong.',format:'A PIN is 4 to 8 digits.'}[d.err]||'Not saved.'}
  catch(e){$('setErr').textContent='The board did not answer.'}
};
$('pinOff').onclick=async()=>{
  try{const d=await post('/api/settings',q('&clearpin=1&old='+enc($('oldPin').value)));
    if(d.ok){$('setOk').textContent='PIN removed.';ver=0;poll()}else $('setErr').textContent='The current PIN is wrong.'}
  catch(e){$('setErr').textContent='The board did not answer.'}
};
$('forget').onclick=async()=>{
  if(!confirm('Forget this WiFi? The board restarts on its own network, devkit-drop.'))return;
  try{await post('/api/forget-wifi',q())}catch(e){}
};

// ── the poll: once a second, so an offer appears on the other device straight away ──
let timer=null;
async function poll(){
  clearTimeout(timer);
  try{
    const d=await (await fetch('/api/state?v='+ver+'&'+q(),{cache:'no-store'})).json();
    S=d;
    $('sub').textContent=(d.mode==='own'?'on its own network '+d.net:'on '+d.net)+' · '+d.host+'.local';
    if(!d.ok){view('lock');if(d.locked)$('pinErr').textContent='Too many tries — wait '+d.locked+' s.';}
    else{
      view('main');
      if(d.devices){ver=d.ver;drawDevices(d.devices);drawNotes(d.notes);$('hours').value=String(d.hours)}
      $('oldRow').style.display=d.pin?'flex':'none';$('pinOff').style.display=d.pin?'block':'none';
      $('netInfo').textContent=d.mode==='own'?'The board is on its own network, '+d.net+'. Set up WiFi to use it on your home network instead.':'On '+d.net+' at http://'+d.host+'.local or http://'+d.ip;
      $('setupLink').style.display=d.mode==='own'?'block':'none';$('forget').style.display=d.mode==='own'?'none':'block';
      const o=d.offer||null;
      if(o&&o.role==='from'&&o.state==='accepted')startUpload(o);
      if(sending&&(!o||o.id!==sending.id||o.state==='done'||o.state==='failed'||o.state==='declined')){
        if(!o||o.state!=='sending')sending=null;
      }
      drawOffer(o);refreshSend();
    }
  }catch(e){$('sub').textContent='cannot reach the board'}
  timer=setTimeout(poll,1000);
}
poll();
</script>
)HTML";
