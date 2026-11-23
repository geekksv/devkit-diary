#pragma once
// The test page. The same page is served over http and https, and it reports what the phone
// allows on each: whether it counts as secure, whether speech recognition exists, whether the
// microphone was allowed, and which voices the phone has.

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Talk: mic test</title>
<style>
:root{--bg:#0e1016;--card:#181b24;--line:#2a2f3d;--text:#e9ebf2;--dim:#9aa1b4;--acc:#5b8cff;--ok:#3ecf8e;--bad:#ff6b6b;--warn:#f5b83d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 2px}p{margin:6px 0}.dim{color:var(--dim);font-size:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:12px 0}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.chk{display:flex;justify-content:space-between;gap:10px;padding:5px 0;border-bottom:1px solid var(--line);font-size:15px}.chk:last-child{border:0}
.ok{color:var(--ok)}.bad{color:var(--bad)}.warn{color:var(--warn)}
select,input,button{font:inherit;color:var(--text);background:#11141c;border:1px solid var(--line);border-radius:10px;padding:10px}
select{flex:1;min-width:0}input{flex:1;min-width:0}
button{background:var(--acc);border-color:var(--acc);font-weight:600;cursor:pointer}
button.ghost{background:transparent;color:var(--text);border-color:var(--line)}
#mic{width:100%;padding:22px;font-size:20px;border-radius:16px;margin-top:10px}
#mic.on{background:var(--bad);border-color:var(--bad)}
.big{font-size:20px;margin:8px 0;min-height:1.4em;word-wrap:break-word}
.tr{font-size:22px;color:#fff}
a{color:var(--acc)}
.log div{padding:6px 0;border-bottom:1px solid var(--line);font-size:14px}.log div:last-child{border:0}
#go{display:none;border-color:var(--warn)}
pre{white-space:pre-wrap;font-size:12px;color:var(--dim);margin:0}
</style></head><body><div class="wrap">
<h1>Talk: microphone test</h1>
<p class="dim">Step 1 of chapter 12. Speak, see the words, have the board translate them, hear them back.</p>

<div class="card" id="go">
<p><b>The microphone needs the secure page.</b> Open one of these, and when the phone warns that it is
"not private", choose <i>Advanced</i> then <i>Proceed</i> (Android) or <i>Show details</i> then <i>visit this website</i> (iPhone).</p>
<p class="big"><a id="go1" href="#">https://talk.local</a><br><a id="go2" href="#"></a></p>
</div>

<div class="card">
<div class="chk"><span>Page came over</span><b id="c-proto"></b></div>
<div class="chk"><span>Phone counts it as secure</span><b id="c-sec"></b></div>
<div class="chk"><span>Speech recognition in this browser</span><b id="c-sr"></b></div>
<div class="chk"><span>Microphone</span><b id="c-mic">not tried yet</b></div>
<div class="chk"><span>Voices on this phone</span><b id="c-voices"></b></div>
<div class="chk"><span>Board translation</span><b id="c-tr">not tried yet</b></div>
</div>

<div class="card">
<div class="row"><span class="dim">I speak</span><select id="from"></select></div>
<div class="row" style="margin-top:8px"><span class="dim">Translate to</span><select id="to"></select></div>
<button id="mic">🎤 Tap and speak</button>
<p class="big dim" id="interim"></p>
<p class="big" id="heard"></p>
<p class="big tr" id="out"></p>
<div class="row"><button class="ghost" id="say">🔊 Say it again</button><label class="dim"><input type="checkbox" id="auto" checked style="flex:none"> speak automatically</label></div>
<div class="row" style="margin-top:10px"><input id="typed" placeholder="Or type here"><button id="send">Translate</button></div>
</div>

<div class="card"><b>What the board received</b><div class="log" id="log"><div class="dim">nothing yet</div></div></div>

<div class="card"><b>Report</b> <span class="dim">(copy this and send it to me if something fails)</span>
<pre id="report"></pre><button class="ghost" id="copy" style="margin-top:8px">Copy report</button></div>
</div>
<script>
const LANGS=[["English (India)","en-IN","en"],["English (US)","en-US","en"],["Hindi","hi-IN","hi"],["Bengali","bn-IN","bn"],["Tamil","ta-IN","ta"],
["Telugu","te-IN","te"],["Marathi","mr-IN","mr"],["Gujarati","gu-IN","gu"],["Kannada","kn-IN","kn"],["Malayalam","ml-IN","ml"],
["Punjabi","pa-IN","pa"],["Urdu","ur-IN","ur"],["Odia","or-IN","or"],["Spanish","es-ES","es"],["French","fr-FR","fr"],["German","de-DE","de"],
["Arabic","ar-SA","ar"],["Chinese","zh-CN","zh-CN"],["Japanese","ja-JP","ja"],["Russian","ru-RU","ru"],["Portuguese","pt-BR","pt"]];
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
for(const [n,c] of LANGS){$('from').add(new Option(n,c));$('to').add(new Option(n,c));}
$('from').value='hi-IN';$('to').value='en-IN';
try{const s=JSON.parse(localStorage.getItem('mt')||'{}');if(s.f)$('from').value=s.f;if(s.t)$('to').value=s.t;}catch(e){}
const keep=()=>{try{localStorage.setItem('mt',JSON.stringify({f:$('from').value,t:$('to').value}))}catch(e){}};
$('from').onchange=$('to').onchange=keep;

const R={ua:navigator.userAgent,proto:location.protocol,secure:window.isSecureContext,sr:'',mic:'not tried',voices:0,langVoices:'',tr:'not tried'};
function mark(id,ok,text){const e=$(id);e.textContent=text;e.className=ok===true?'ok':ok===false?'bad':'warn';report();}
function report(){$('report').textContent=Object.entries(R).map(([k,v])=>k+': '+v).join('\n');}

mark('c-proto',location.protocol==='https:',location.protocol==='https:'?'https':'http');
mark('c-sec',isSecureContext,isSecureContext?'yes':'no');
const SR=window.SpeechRecognition||window.webkitSpeechRecognition;
R.sr=SR?'yes':'no';mark('c-sr',!!SR,SR?'yes':'no (try Chrome or Safari)');
if(!isSecureContext){$('go').style.display='block';const h=location.hostname;
 $('go1').href='https://talk.local/';if(h!=='talk.local'){$('go2').href='https://'+h+'/';$('go2').textContent='https://'+h;}}

function voicesNow(){const v=speechSynthesis.getVoices();R.voices=v.length;
 const want=['hi','bn','ta','te','mr','gu','kn','ml','pa','ur','en'];
 R.langVoices=want.filter(l=>v.some(x=>x.lang.toLowerCase().startsWith(l))).join(' ');
 mark('c-voices',v.length>0,v.length?v.length+' ('+(R.langVoices||'none of ours')+')':'none found yet');}
if(window.speechSynthesis){voicesNow();speechSynthesis.onvoiceschanged=voicesNow;}else mark('c-voices',false,'no speech at all');

function speak(text,lang){if(!window.speechSynthesis||!text)return;speechSynthesis.cancel();
 const u=new SpeechSynthesisUtterance(text);u.lang=lang;const base=lang.split('-')[0];
 const v=speechSynthesis.getVoices();u.voice=v.find(x=>x.lang===lang)||v.find(x=>x.lang.replace('_','-').toLowerCase().startsWith(base))||null;
 if(!u.voice&&base!=='en'){$('out').insertAdjacentHTML('beforeend','<br><span class="dim" style="font-size:14px">(this phone has no '+esc(base)+' voice)</span>');}
 speechSynthesis.speak(u);}
$('say').onclick=()=>speak($('out').dataset.t||'',$('to').value);

async function translate(text){
 if(!text.trim())return;$('heard').textContent=text;$('out').textContent='…';
 const tl=LANGS.find(l=>l[1]===$('to').value)[2];const t0=performance.now();
 try{const r=await fetch('/said',{method:'POST',body:tl+'\n'+text});const j=await r.json();
  const ms=Math.round(performance.now()-t0);
  if(j.tr){$('out').textContent=j.tr;$('out').dataset.t=j.tr;R.tr='ok via '+j.via+' in '+ms+' ms (board '+j.ms+' ms), heap '+j.heap+' min '+j.min;
   mark('c-tr',true,'ok, '+ms+' ms ('+j.via+')');if($('auto').checked)speak(j.tr,$('to').value);}
  else{$('out').textContent='';R.tr='failed: '+j.err;mark('c-tr',false,'failed: '+j.err);}
 }catch(e){R.tr='failed: '+e;mark('c-tr',false,'failed: '+e);}
 poll();}
$('send').onclick=()=>{translate($('typed').value);$('typed').value='';};
$('typed').onkeydown=e=>{if(e.key==='Enter')$('send').click();};

let rec=null,on=false,finalText='';
$('mic').onclick=()=>{
 if(on){rec&&rec.stop();return;}
 if(!SR){mark('c-mic',false,'no speech recognition here');return;}
 if(window.speechSynthesis){speechSynthesis.cancel();speechSynthesis.speak(new SpeechSynthesisUtterance(''));} // unlocks speech on iPhone
 rec=new SR();rec.lang=$('from').value;rec.interimResults=true;rec.continuous=false;finalText='';
 rec.onstart=()=>{on=true;$('mic').classList.add('on');$('mic').textContent='● Listening… tap to stop';R.mic='allowed';mark('c-mic',true,'allowed');};
 rec.onresult=e=>{let inter='';for(let i=e.resultIndex;i<e.results.length;i++){const r=e.results[i];if(r.isFinal)finalText+=r[0].transcript;else inter+=r[0].transcript;}
  $('interim').textContent=inter;};
 rec.onerror=e=>{const why={'not-allowed':'blocked (the page is not secure, or permission was denied)','service-not-allowed':'blocked by the browser',
  'network':'needs internet for recognition','no-speech':'heard nothing','audio-capture':'no microphone','language-not-supported':'this language is not supported here'}[e.error]||e.error;
  R.mic=e.error;mark('c-mic',e.error==='no-speech'?null:false,why);};
 rec.onend=()=>{on=false;$('mic').classList.remove('on');$('mic').textContent='🎤 Tap and speak';$('interim').textContent='';if(finalText)translate(finalText);};
 try{rec.start();}catch(e){R.mic=String(e);mark('c-mic',false,String(e));}
};

async function poll(){try{const j=await (await fetch('/heard')).json();
 $('log').innerHTML=j.log.length?j.log.map(m=>'<div>'+esc(m.t)+'<br><span class="dim">→ '+esc(m.tr||m.err)+' · '+esc(m.via)+', '+m.ms+' ms</span></div>').join(''):'<div class="dim">nothing yet</div>';}catch(e){}}
poll();setInterval(poll,3000);
$('copy').onclick=()=>{navigator.clipboard?navigator.clipboard.writeText($('report').textContent).then(()=>$('copy').textContent='Copied'):0;};
report();
</script></body></html>)HTML";
