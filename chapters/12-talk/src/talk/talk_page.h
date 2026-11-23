#pragma once
// The page, served the same over http and https. Phones listen, translate and speak here; the
// board (talk.h) only keeps the room. Views: home, room, pass (one phone, two people), tools,
// and the caption screen (#tv=CODE).

static const char TALK_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0e1016">
<title>Talk</title>
<style>
:root{--bg:#0e1016;--card:#171a23;--card2:#1e2230;--line:#2a2f3d;--text:#e9ebf2;--dim:#9aa1b4;--acc:#5b8cff;--ok:#3ecf8e;--bad:#ff6b6b;--warn:#f5b83d;--me:#23315a}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,"Noto Sans","Noto Sans Devanagari",sans-serif}
body.big{font-size:21px}
[hidden]{display:none!important}
.wrap{max-width:640px;margin:0 auto;padding:16px}
h1{font-size:26px;margin:6px 0 2px}h2{font-size:18px;margin:0 0 8px}
p{margin:6px 0}.dim{color:var(--dim)}.small{font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:12px 0}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.row>*{min-width:0}
.grow{flex:1}
select,input,textarea,button{font:inherit;color:var(--text);background:#11141c;border:1px solid var(--line);border-radius:10px;padding:10px 12px}
textarea{width:100%;min-height:110px;resize:vertical}
input{width:100%}
button{background:var(--acc);border-color:var(--acc);font-weight:600;cursor:pointer;touch-action:manipulation}
button.ghost{background:transparent;color:var(--text);border-color:var(--line);font-weight:500}
button.small{padding:6px 10px;font-size:14px}
button:disabled{opacity:.45}
a{color:var(--acc)}
.warnbox{border-color:var(--warn)}
.list>div{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 0;border-bottom:1px solid var(--line)}.list>div:last-child{border:0}
.tag{display:inline-block;background:var(--card2);border:1px solid var(--line);border-radius:999px;padding:1px 8px;font-size:13px;margin:2px 2px 0 0}

/* the room */
#room{display:flex;flex-direction:column;height:100%;max-width:760px;margin:0 auto;width:100%;overflow:hidden}
#room>*{min-width:0}
.top{display:flex;gap:8px;align-items:center;padding:10px 12px;border-bottom:1px solid var(--line)}
.code{font-weight:800;letter-spacing:.12em;font-size:20px}
.people{display:flex;gap:6px;overflow-x:auto;padding:8px 12px;border-bottom:1px solid var(--line);scrollbar-width:none}
.chip{flex:none;display:flex;gap:6px;align-items:center;background:var(--card);border:1px solid var(--line);border-radius:999px;padding:4px 10px 4px 4px;font-size:14px}
.chip .dot{width:22px;height:22px;border-radius:50%;display:grid;place-items:center;font-size:12px;font-weight:700;color:#0e1016}
.chip.away{opacity:.45}.chip .on{color:var(--bad)}
.msgs{flex:1;overflow-y:auto;padding:12px;display:flex;flex-direction:column;gap:10px}
.msg{max-width:88%;background:var(--card);border:1px solid var(--line);border-radius:16px;padding:9px 12px;align-self:flex-start}
.msg.mine{align-self:flex-end;background:var(--me);border-color:#33447a}
.msg .who{font-size:13px;color:var(--dim);display:flex;gap:8px;justify-content:space-between}
.msg .main{font-size:1.15em;margin-top:2px;word-wrap:break-word}
.msg .orig{font-size:.85em;color:var(--dim);margin-top:4px;word-wrap:break-word}
.msg .say{background:none;border:0;padding:0 2px;font-size:16px;color:var(--dim)}
.live{padding:0 12px;min-height:0}
.live div{color:var(--dim);font-style:italic;padding:2px 0}
.phrases{display:flex;gap:6px;overflow-x:auto;padding:6px 12px;scrollbar-width:none}
.phrases button{flex:none;background:var(--card);border-color:var(--line);color:var(--text);font-weight:500;padding:7px 11px;font-size:14px;border-radius:999px}
.bar{display:flex;gap:8px;align-items:flex-end;padding:10px 12px calc(10px + env(safe-area-inset-bottom));border-top:1px solid var(--line)}
.bar input{flex:1;min-width:0;width:auto}
.mic{width:64px;height:64px;border-radius:50%;font-size:26px;padding:0;flex:none}
.mic.on{background:var(--bad);border-color:var(--bad);animation:pulse 1.2s infinite}
@keyframes pulse{50%{box-shadow:0 0 0 10px rgba(255,107,107,.15)}}
.menu{position:fixed;right:10px;top:56px;z-index:5;background:var(--card2);border:1px solid var(--line);border-radius:14px;padding:6px;min-width:240px;box-shadow:0 10px 30px rgba(0,0,0,.5)}
.menu button{display:block;width:100%;text-align:left;background:none;border:0;font-weight:500;padding:11px 12px}
.menu button:hover{background:var(--card)}
.toast{position:fixed;left:50%;bottom:110px;transform:translateX(-50%);background:var(--card2);border:1px solid var(--line);border-radius:12px;padding:10px 14px;z-index:9;max-width:90%;text-align:center}

/* pass: one phone, two people facing each other */
#pass{display:flex;flex-direction:column;height:100%}
.half{flex:1;display:flex;flex-direction:column;padding:12px;gap:8px;min-height:0}
.half.flip{transform:rotate(180deg);border-bottom:2px dashed var(--line)}
.half .lines{flex:1;overflow-y:auto;display:flex;flex-direction:column;justify-content:flex-end;gap:6px}
.half .lines div{font-size:1.2em}.half .lines .theirs{color:#fff}.half .lines .mine{color:var(--dim);font-size:1em}
.half .ctl{display:flex;gap:8px;align-items:center}
.half .mic{width:76px;height:76px}

/* tools */
.tabs{display:flex;gap:6px;margin:10px 0}.tabs button{flex:1}.tabs button.sel{background:var(--acc)}
.tabs button:not(.sel){background:transparent;border-color:var(--line);color:var(--text)}

/* caption screen */
#tv{min-height:100%;padding:3vh 4vw calc(3vh + 48px);display:flex;flex-direction:column}
#tv .head{display:flex;justify-content:space-between;align-items:center;gap:12px;color:var(--dim);font-size:2.2vh}
#tv .caps{flex:1;display:flex;flex-direction:column;justify-content:flex-end;gap:2.4vh;padding:3vh 0}
#tv .cap .n{font-size:2.4vh;color:var(--dim)}
#tv .cap .t{font-size:5vh;line-height:1.25;font-weight:600}
#tv .cap.old{opacity:.5}#tv .cap.old .t{font-size:3.6vh}
#tv .lv{font-size:3.4vh;color:var(--dim);font-style:italic;min-height:4vh}
#tvTap{position:fixed;left:0;right:0;bottom:0;background:var(--acc);color:#fff;text-align:center;padding:12px;font-weight:600}
</style></head><body>

<!-- home -->
<div class="wrap" id="home">
<h1>💬 Talk</h1>
<p class="dim">Everyone speaks and reads in their own language. Your phone listens, translates and speaks; the board keeps the room.</p>
<div class="card warnbox" id="insecure" hidden>
<b>To use the microphone, open the secure page:</b>
<p class="row"><a id="secLink" href="#" style="font-size:20px"></a></p>
<p class="dim small">The first time, the phone warns that the page is "not private", because the board made its own certificate.
Android: <i>Advanced</i> → <i>Proceed</i>. iPhone: <i>Show details</i> → <i>visit this website</i>. Typing and reading work here too.</p>
</div>
<div class="card" id="nosr" hidden><b>This browser can't listen.</b> <span class="dim">Chrome, Edge or Safari can. You can still type, read and hear.</span></div>
<div class="card">
<div class="row"><input id="name" maxlength="24" placeholder="Your name" autocomplete="name"></div>
<div class="row" style="margin-top:8px"><span class="dim">I speak</span><select id="lang" class="grow"></select></div>
<div class="row" style="margin-top:12px"><button id="start" class="grow">Start a room</button></div>
<div class="row" style="margin-top:8px"><input id="code" maxlength="4" placeholder="Code" style="flex:1;text-transform:uppercase;letter-spacing:.15em"><button id="join">Join</button></div>
<p class="dim small" id="homeErr"></p>
</div>
<div class="card" id="openRooms" hidden><h2>Rooms on this board</h2><div class="list" id="roomList"></div></div>
<div class="card">
<div class="row"><button class="ghost grow" id="goPass">📱 One phone, two people</button><button class="ghost grow" id="goTools">🧰 Dictate, read aloud, translate</button></div>
</div>
<p class="dim small">Speech recognition usually sends your voice to Google or Apple, and translation sends the words to Google (or MyMemory).
The board keeps a room's messages in memory only, and forgets them when it restarts.</p>
</div>

<!-- the room -->
<div id="room" hidden>
<div class="top">
 <button class="ghost small" id="back">←</button>
 <div class="grow"><div class="code" id="rCode"></div><div class="dim small" id="rInfo"></div></div>
 <button class="ghost small" id="spk" title="Read messages aloud">🔊</button>
 <button class="ghost small" id="more">⋯</button>
</div>
<div class="people" id="people"></div>
<div class="msgs" id="msgs"></div>
<div class="live" id="live"></div>
<div class="phrases" id="phrases" hidden></div>
<div class="bar">
 <button class="ghost small" id="phr" title="Quick phrases" style="height:44px">⚡</button>
 <input id="typed" placeholder="Type, or tap the mic" enterkeyhint="send">
 <button class="mic" id="mic">🎤</button>
</div>
</div>
<div class="menu" id="menu" hidden>
 <button id="mKeep">🎙️ Keep listening (captions)</button>
 <button id="mBig">🔠 Bigger text</button>
 <button id="mTv">📺 Show captions on a TV</button>
 <button id="mLang">🌐 Change my language</button>
 <button id="mSave">💾 Save the conversation</button>
 <button id="mClear" hidden>🧹 Clear for everyone</button>
 <button id="mLeave">🚪 Leave the room</button>
</div>

<!-- pass -->
<div id="pass" hidden>
<div class="half flip" id="hB">
 <div class="lines" id="lB"></div>
 <div class="ctl"><select class="grow" id="langB"></select><button class="mic" id="micB">🎤</button></div>
</div>
<div class="half" id="hA">
 <div class="lines" id="lA"></div>
 <div class="ctl"><select class="grow" id="langA"></select><button class="mic" id="micA">🎤</button><button class="ghost small" id="passEnd">✕</button></div>
</div>
</div>

<!-- tools -->
<div class="wrap" id="tools" hidden>
<div class="row"><button class="ghost small" id="toolsBack">←</button><h2 style="margin:0">Tools</h2></div>
<div class="tabs"><button data-t="dict" class="sel">🎤 Dictate</button><button data-t="read">🔊 Read aloud</button><button data-t="tran">🌐 Translate</button></div>
<div class="card" data-p="dict">
 <div class="row"><select id="dLang" class="grow"></select><button id="dMic">🎤 Start</button></div>
 <p class="dim" id="dInter"></p>
 <textarea id="dText" placeholder="What you say appears here"></textarea>
 <div class="row" style="margin-top:8px"><button class="ghost" id="dCopy">Copy</button><button class="ghost" id="dClear">Clear</button></div>
</div>
<div class="card" data-p="read" hidden>
 <textarea id="rText" placeholder="Paste or type text to hear it"></textarea>
 <div class="row" style="margin-top:8px"><select id="rLang" class="grow"></select></div>
 <div class="row" style="margin-top:8px"><span class="dim">Speed</span><input id="rRate" type="range" min="0.5" max="1.6" step="0.1" value="1" class="grow" style="padding:0"></div>
 <div class="row" style="margin-top:8px"><button id="rGo">🔊 Read</button><button class="ghost" id="rStop">Stop</button></div>
 <p class="dim small" id="rNote"></p>
</div>
<div class="card" data-p="tran" hidden>
 <textarea id="tText" placeholder="Type in any language"></textarea>
 <div class="row" style="margin-top:8px"><span class="dim">To</span><select id="tLang" class="grow"></select><button id="tGo">Translate</button></div>
 <p style="font-size:1.25em" id="tOut"></p>
 <div class="row"><button class="ghost" id="tSay" hidden>🔊 Say it</button><button class="ghost" id="tCopy" hidden>Copy</button></div>
</div>
</div>

<!-- caption screen -->
<div id="tv" hidden>
<div class="head"><div><b id="tvCode" style="color:var(--text);letter-spacing:.12em"></b> · <span id="tvAddr"></span></div><select id="tvLang"></select></div>
<div class="caps" id="caps"></div>
<div class="lv" id="tvLive"></div>
<div id="tvTap">Tap anywhere to hear the captions read aloud</div>
</div>

<div class="toast" id="toast" hidden></div>

<script>
// [name, native name, speech code, translation code]
const LANGS=[["English (India)","English","en-IN","en"],["English (US)","English","en-US","en"],["English (UK)","English","en-GB","en"],
["Hindi","हिन्दी","hi-IN","hi"],["Bengali","বাংলা","bn-IN","bn"],["Tamil","தமிழ்","ta-IN","ta"],["Telugu","తెలుగు","te-IN","te"],
["Marathi","मराठी","mr-IN","mr"],["Gujarati","ગુજરાતી","gu-IN","gu"],["Kannada","ಕನ್ನಡ","kn-IN","kn"],["Malayalam","മലയാളം","ml-IN","ml"],
["Punjabi","ਪੰਜਾਬੀ","pa-IN","pa"],["Urdu","اردو","ur-IN","ur"],["Odia","ଓଡ଼ିଆ","or-IN","or"],["Assamese","অসমীয়া","as-IN","as"],["Nepali","नेपाली","ne-NP","ne"],
["Spanish","Español","es-ES","es"],["French","Français","fr-FR","fr"],["German","Deutsch","de-DE","de"],["Italian","Italiano","it-IT","it"],
["Portuguese","Português","pt-BR","pt"],["Russian","Русский","ru-RU","ru"],["Arabic","العربية","ar-SA","ar"],["Turkish","Türkçe","tr-TR","tr"],
["Chinese","中文","zh-CN","zh-CN"],["Japanese","日本語","ja-JP","ja"],["Korean","한국어","ko-KR","ko"]];
const PHRASES=["Hello","Yes","No","Thank you","Please","Sorry","Wait a moment, please","Please speak slowly","Please say that again",
"I don't understand","Can you help me?","Where is the bathroom?","How much is it?","Are you hungry?","Please sit down","Let's eat",
"Welcome to our home","Good night","Goodbye"];
const COLORS=['#5b8cff','#3ecf8e','#f5b83d','#ff6b6b','#c58cff','#4dd0e1','#ff9f68','#a3d95b','#f78fb3','#9aa1b4'];

const $=id=>document.getElementById(id);
const esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const enc=encodeURIComponent;
const L=code=>LANGS.find(l=>l[2]===code)||LANGS.find(l=>l[3]===code)||LANGS[0];
const tlOf=code=>L(code)[3];
const base=c=>String(c||'').split('-')[0].toLowerCase();
const SR=window.SpeechRecognition||window.webkitSpeechRecognition;
const store={get(k,d){try{const v=localStorage.getItem('talk.'+k);return v==null?d:JSON.parse(v)}catch(e){return d}},
 set(k,v){try{localStorage.setItem('talk.'+k,JSON.stringify(v))}catch(e){}}};
function fillLangs(sel,val){sel.innerHTML='';for(const l of LANGS)sel.add(new Option(l[0]+(l[1]!==l[0].split(' ')[0]?' · '+l[1]:''),l[2]));sel.value=val||'en-IN';}
let toastT;function toast(t,ms){$('toast').textContent=t;$('toast').hidden=false;clearTimeout(toastT);toastT=setTimeout(()=>$('toast').hidden=true,ms||3000);}
function show(id){for(const v of ['home','room','pass','tools','tv'])$(v).hidden=v!==id;$('menu').hidden=true;}

// ── translating, from this phone ─────────────────────────────────────────────
const trCache=new Map(),trWait=new Map();
async function fetchT(url,ms){const c=new AbortController();const t=setTimeout(()=>c.abort(),ms||8000);
 try{return await fetch(url,{signal:c.signal,credentials:'omit'})}finally{clearTimeout(t)}}
async function translate(text,tl){
 const k=tl+'\n'+text;if(trCache.has(k))return trCache.get(k);if(trWait.has(k))return trWait.get(k);
 const p=(async()=>{
  try{const j=await (await fetchT('https://clients5.google.com/translate_a/t?client=dict-chrome-ex&sl=auto&tl='+enc(tl)+'&q='+enc(text))).json();
   const t=Array.isArray(j[0])?j[0][0]:j[0];if(typeof t==='string'&&t)return t;}catch(e){}
  try{const j=await (await fetchT('https://api.mymemory.translated.net/get?q='+enc(text)+'&langpair=Autodetect%7C'+enc(tl))).json();
   const t=j&&j.responseStatus==200&&!j.quotaFinished&&j.responseData&&j.responseData.translatedText;
   if(t&&!/^MYMEMORY WARNING/.test(t))return t;}catch(e){}
  return null;})();
 trWait.set(k,p);const t=await p;trWait.delete(k);if(t){trCache.set(k,t);if(trCache.size>300)trCache.delete(trCache.keys().next().value);}return t;}

// ── speaking ─────────────────────────────────────────────────────────────────
const noVoice=new Set();let unlocked=false;
function unlock(){if(unlocked||!window.speechSynthesis)return;unlocked=true;try{speechSynthesis.speak(new SpeechSynthesisUtterance(''))}catch(e){}}
document.addEventListener('pointerdown',unlock,{capture:true});
function pickVoice(code){const v=speechSynthesis.getVoices().map(x=>({x,l:x.lang.replace('_','-').toLowerCase()}));const c=code.toLowerCase(),b=base(code);
 return (v.find(o=>o.l===c)||v.find(o=>o.l.split('-')[0]===b)||{}).x||null;}
function speak(text,code,opt){if(!window.speechSynthesis||!text)return;opt=opt||{};
 const u=new SpeechSynthesisUtterance(text);u.lang=code;u.rate=opt.rate||1;const v=pickVoice(code),b=base(code);
 if(v)u.voice=v;
 else{if(!noVoice.has(b)){noVoice.add(b);toast('This device has no '+L(code)[0]+' voice, so it shows the text only',4000);}
  if(b!=='en')return;}   // another language's voice reading this would be worse than silence
 if(opt.now)speechSynthesis.cancel();speechSynthesis.speak(u);}
if(window.speechSynthesis){speechSynthesis.getVoices();speechSynthesis.onvoiceschanged=()=>speechSynthesis.getVoices();}

// ── listening ────────────────────────────────────────────────────────────────
const MICERR={'not-allowed':'The microphone is blocked. Open the secure (https) page, and allow the microphone.',
 'service-not-allowed':'This browser blocked speech recognition.','network':'Speech recognition needs the internet.',
 'audio-capture':'No microphone found.','language-not-supported':'This language cannot be heard on this device. Try typing.'};
function listen(lang,continuous,h){
 if(!SR){toast('This browser cannot listen. Try Chrome or Safari, or type.',4000);return null;}
 if(!isSecureContext){toast('The microphone needs the secure page: '+secureUrl(),5000);return null;}
 if(window.speechSynthesis)speechSynthesis.cancel();
 const r=new SR();r.lang=lang;r.interimResults=true;r.continuous=continuous;r.maxAlternatives=1;
 r.onresult=e=>{let inter='';for(let i=e.resultIndex;i<e.results.length;i++){const x=e.results[i];
  if(x.isFinal){const t=x[0].transcript.trim();if(t)h.final(t);}else inter+=x[0].transcript;}h.interim&&h.interim(inter);};
 r.onerror=e=>{if(e.error!=='no-speech'&&e.error!=='aborted'){r.fatal=e.error;toast(MICERR[e.error]||('Microphone: '+e.error),4500);}};
 r.onend=()=>h.end&&h.end(r.fatal);
 try{r.start();}catch(e){toast(String(e));return null;}return r;}
function secureUrl(){return 'https://'+(location.hostname||'talk.local')+'/'+location.hash;}

// ── talking to the board ─────────────────────────────────────────────────────
async function api(path,data){const r=await fetch(path,{method:'POST',body:new URLSearchParams(data||{})});return r.json();}

// ── home ─────────────────────────────────────────────────────────────────────
fillLangs($('lang'),store.get('lang','en-IN'));$('name').value=store.get('name','');
$('lang').onchange=()=>store.set('lang',$('lang').value);$('name').onchange=()=>store.set('name',$('name').value.trim());
if(!isSecureContext){$('insecure').hidden=false;$('secLink').href=secureUrl();$('secLink').textContent='https://'+(location.hostname||'talk.local');}
if(!SR)$('nosr').hidden=false;
function needName(){const n=$('name').value.trim();if(!n){$('name').focus();$('homeErr').textContent='Type your name first.';return null;}store.set('name',n);return n;}
$('start').onclick=async()=>{const n=needName();if(!n)return;const j=await api('/create',{n,l:$('lang').value});
 if(!j.ok){$('homeErr').textContent=j.err==='full'?'The board already has its rooms open. Join one of them.':'Could not start: '+j.err;return;}enterRoom(j.c,j.t);};
async function joinCode(c){const n=needName();if(!n)return;c=c.toUpperCase();const seat=store.get('seat',{});
 const j=await api('/join',{c,n,l:$('lang').value,t:seat[c]||''});
 if(!j.ok){$('homeErr').textContent={noroom:'No room with that code.',roomfull:'That room is full.'}[j.err]||j.err;return;}enterRoom(j.c,j.t);}
$('join').onclick=()=>{const c=$('code').value.trim();if(c.length===4)joinCode(c);else $('homeErr').textContent='The code has four letters.';};
$('code').onkeydown=e=>{if(e.key==='Enter')$('join').click();};
async function listRooms(){if($('home').hidden)return;try{const j=await (await fetch('/rooms')).json();
 $('openRooms').hidden=!j.rooms.length;
 $('roomList').innerHTML=j.rooms.map(r=>'<div><span><b style="letter-spacing:.1em">'+r.c+'</b> · '+esc(r.host)+' <span class="dim small">'+r.n+' here</span><br>'+
  [...new Set(r.langs.map(l=>L(l)[0].split(' ')[0]))].map(n=>'<span class="tag">'+esc(n)+'</span>').join('')+'</span><button class="small" data-c="'+r.c+'">Join</button></div>').join('');
 $('roomList').querySelectorAll('button').forEach(b=>b.onclick=()=>joinCode(b.dataset.c));}catch(e){}}
setInterval(listRooms,4000);

// ── the room ─────────────────────────────────────────────────────────────────
const R={code:'',tok:'',me:-1,host:-1,ver:0,msgs:new Map(),people:[],timer:0,polling:false,loaded:false,openAt:0,speakOn:true,
 mic:null,mode:'off',sent:new Set(),spoken:new Set(),trying:new Set()};
function myLang(){const p=R.people.find(p=>p.i===R.me);return p?p.l:$('lang').value;}
function enterRoom(c,t){Object.assign(R,{code:c,tok:t,me:-1,ver:0,warned:false,msgs:new Map(),people:[],loaded:false,sent:new Set(),spoken:new Set(),trying:new Set()});
 const seat=store.get('seat',{});seat[c]=t;store.set('seat',seat);
 R.speakOn=store.get('speak',true);$('spk').textContent=R.speakOn?'🔊':'🔇';
 location.hash='c='+c;show('room');$('rCode').textContent=c;$('msgs').innerHTML='';poll();}
async function poll(){clearTimeout(R.timer);if(R.polling||!R.code)return;R.polling=true;let again=1000;
 try{const j=await (await fetch('/state?c='+R.code+'&t='+R.tok+'&v='+R.ver+'&tl='+enc(tlOf(myLang())))).json();
  if(!j.ok){if(j.err==='noroom'){toast('That room has closed.');leaveRoom(true);}R.polling=false;return;}
  if(j.reset){R.msgs.clear();$('msgs').innerHTML='';}
  R.me=j.me;R.host=j.host;R.people=j.people;R.openAt=Date.now()-j.now*1000;
  if(R.me<0&&!R.warned){R.warned=true;toast('You are watching this room. Join from the start screen to talk.');}
  for(const m of j.msgs){const old=R.msgs.get(m.id);if(old&&old.t&&!m.t)m.t=old.t;R.msgs.set(m.id,m);if(!R.loaded)R.spoken.add(m.id);}
  R.ver=j.v;if(j.more)again=0;else R.loaded=true;
  renderRoom();
 }catch(e){again=2500;}
 R.polling=false;if(R.code&&!$('room').hidden)R.timer=setTimeout(poll,again);}
function needsTr(m,tl){return base(m.l)!==base(tl);}
function renderRoom(){
 const tl=tlOf(myLang());
 $('rInfo').textContent=L(myLang())[0]+(R.me===R.host?' · you host':'');$('mClear').hidden=R.me!==R.host;
 $('people').innerHTML=R.people.map(p=>'<div class="chip'+(p.away?' away':'')+'"><span class="dot" style="background:'+COLORS[p.i%10]+'">'+esc([...p.n][0]||'?')+'</span>'+
  esc(p.n)+(p.i===R.me?' (you)':'')+' <span class="dim">'+esc(L(p.l)[1])+'</span>'+(p.on?' <span class="on">●</span>':'')+'</div>').join('');
 const box=$('msgs'),stick=box.scrollTop+box.clientHeight>=box.scrollHeight-60;
 const ids=[...R.msgs.keys()].sort((a,b)=>a-b);
 for(const id of ids){const m=R.msgs.get(id);let el=document.getElementById('m'+id);
  if(!el){el=document.createElement('div');el.id='m'+id;el.className='msg'+(m.w===R.me&&m.w>=0?' mine':'');box.appendChild(el);}
  const tr=needsTr(m,tl);if(tr&&!m.t)fetchTr(m,tl);
  const main=tr?(m.t||'…'):m.x,sub=tr?m.x:'';
  const time=new Date(R.openAt+m.at*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});
  const html='<div class="who"><span style="color:'+(m.w>=0?COLORS[m.w%10]:'var(--dim)')+'">'+esc(m.n)+(m.k===2?' ⚡':'')+'</span><span>'+time+
   ' <button class="say" data-id="'+id+'">🔊</button></span></div><div class="main">'+esc(main)+'</div>'+(sub?'<div class="orig">'+esc(L(m.l)[1])+': '+esc(sub)+'</div>':'');
  if(el.dataset.h!==html){el.innerHTML=html;el.dataset.h=html;el.querySelector('.say').onclick=()=>sayMsg(m,true);}
  maybeSpeak(m);}
 for(const el of [...box.children])if(!R.msgs.has(+el.id.slice(1)))el.remove();
 if(stick)box.scrollTop=box.scrollHeight;
 $('live').innerHTML=R.people.filter(p=>p.live&&p.i!==R.me).map(p=>'<div>'+esc(p.n)+': '+esc(p.live)+'…</div>').join('');}
async function fetchTr(m,tl){const k=m.id+'|'+tl;if(R.trying.has(k))return;R.trying.add(k);
 const t=await translate(m.x,tl);
 if(t){const cur=R.msgs.get(m.id);if(cur&&!cur.t){cur.t=t;renderRoom();}
  api('/tr',{c:R.code,t:R.tok,id:m.id,tl,x:t}).catch(()=>{});}
 else setTimeout(()=>R.trying.delete(k),8000);}
function sayMsg(m,now){const tl=tlOf(myLang());const tr=needsTr(m,tl);
 if(tr&&!m.t)return;speak(tr?m.t:m.x,tr?myLang():m.l,{now});}
function maybeSpeak(m){if(R.spoken.has(m.id))return;const tl=tlOf(myLang());
 if(m.w===R.me&&m.w>=0){R.spoken.add(m.id);return;}
 if(needsTr(m,tl)&&!m.t)return;R.spoken.add(m.id);
 if(R.speakOn&&R.mode==='off')sayMsg(m,false);}
async function send(text,kind,lang){text=(text||'').trim();if(!text||!R.code)return;
 const j=await api('/say',{c:R.code,t:R.tok,x:text,l:lang||myLang(),k:kind||0}).catch(()=>null);
 if(!j||!j.ok){toast('Not sent: '+((j&&j.err)||'the board did not answer'));return;}poll();}
$('typed').onkeydown=e=>{if(e.key==='Enter'){send($('typed').value,1);$('typed').value='';}};
$('spk').onclick=()=>{R.speakOn=!R.speakOn;store.set('speak',R.speakOn);$('spk').textContent=R.speakOn?'🔊':'🔇';if(!R.speakOn&&window.speechSynthesis)speechSynthesis.cancel();
 toast(R.speakOn?'New messages will be read aloud':'Messages will not be read aloud');};
// The mic: tap for one sentence; "keep listening" for captions.
let liveT=0,liveText='';
function sendLive(t,on){liveText=t;const now=Date.now();if(now-liveT<700&&t)return;liveT=now;
 api('/live',{c:R.code,t:R.tok,x:t,on:on?1:0}).catch(()=>{});}
function micStart(mode){if(R.mic)return;R.mode=mode;
 R.mic=listen(myLang(),mode==='keep',{final:t=>{send(t,0);sendLive('',mode==='keep');},interim:t=>sendLive(t,true),
  end:fatal=>{R.mic=null;if(R.mode==='keep'&&!fatal){setTimeout(()=>{if(R.mode==='keep')micStart('keep');},250);return;}
   R.mode='off';micUi();sendLive('',false);}});
 if(!R.mic)R.mode='off';micUi();}
function micStop(){R.mode='off';if(R.mic){try{R.mic.stop()}catch(e){}}micUi();}
function micUi(){$('mic').classList.toggle('on',R.mode!=='off');$('mKeep').textContent=R.mode==='keep'?'⏹ Stop listening':'🎙️ Keep listening (captions)';}
$('mic').onclick=()=>R.mode!=='off'?micStop():micStart('once');
$('more').onclick=e=>{e.stopPropagation();$('menu').hidden=!$('menu').hidden;};
document.addEventListener('click',e=>{if(!$('menu').hidden&&!$('menu').contains(e.target))$('menu').hidden=true;});
$('mKeep').onclick=()=>{$('menu').hidden=true;if(R.mode==='keep')micStop();else{micStop();setTimeout(()=>micStart('keep'),150);
 toast('Listening until you stop. Everyone sees captions in their own language.',4000);}};
$('mBig').onclick=()=>{document.body.classList.toggle('big');store.set('big',document.body.classList.contains('big'));$('menu').hidden=true;};
if(store.get('big',false))document.body.classList.add('big');
$('mTv').onclick=()=>{$('menu').hidden=true;const u='http://'+location.host+'/#tv='+R.code;
 toast('On the TV or laptop, open '+u,8000);try{navigator.clipboard.writeText(u)}catch(e){}};
$('mLang').onclick=()=>{$('menu').hidden=true;show('home');toast('Pick your language, then join '+R.code+' again',4000);$('code').value=R.code;};
$('mSave').onclick=()=>{$('menu').hidden=true;const tl=tlOf(myLang());
 const lines=[...R.msgs.values()].sort((a,b)=>a.id-b.id).map(m=>{const t=new Date(R.openAt+m.at*1000).toLocaleString();
  let s='['+t+'] '+m.n+' ('+L(m.l)[0]+'): '+m.x;if(needsTr(m,tl)&&m.t)s+='\n    '+L(myLang())[0]+': '+m.t;return s;});
 const b=new Blob(['Talk, room '+R.code+'\n\n'+lines.join('\n')+'\n'],{type:'text/plain;charset=utf-8'});
 const a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='talk-'+R.code+'.txt';a.click();};
$('mClear').onclick=async()=>{$('menu').hidden=true;if(confirm('Clear the conversation for everyone?'))await api('/clear',{c:R.code,t:R.tok});poll();};
$('mLeave').onclick=()=>{$('menu').hidden=true;leaveRoom(false);};
$('back').onclick=()=>{micStop();R.code='';clearTimeout(R.timer);location.hash='';show('home');listRooms();};
async function leaveRoom(gone){micStop();const c=R.code;if(!gone&&c)await api('/leave',{c,t:R.tok}).catch(()=>{});
 const seat=store.get('seat',{});delete seat[c];store.set('seat',seat);R.code='';clearTimeout(R.timer);location.hash='';show('home');listRooms();}
$('phr').onclick=()=>{$('phrases').hidden=!$('phrases').hidden;};
$('phrases').innerHTML=PHRASES.map(p=>'<button>'+esc(p)+'</button>').join('');
$('phrases').querySelectorAll('button').forEach(b=>b.onclick=()=>send(b.textContent,2,'en-US'));

// ── pass: one phone, two people ──────────────────────────────────────────────
const P={code:'',a:'',b:'',ver:0,msgs:new Map(),timer:0,mic:null,who:'',openAt:0,loaded:false,spoken:new Set()};
fillLangs($('langA'),store.get('passA',$('lang').value));fillLangs($('langB'),store.get('passB','en-IN'));
$('goPass').onclick=async()=>{const n=($('name').value.trim()||'Me');
 const j=await api('/create',{n,l:$('langA').value});if(!j.ok){$('homeErr').textContent=j.err==='full'?'The board already has its rooms open.':j.err;return;}
 const k=await api('/join',{c:j.c,n:'Guest',l:$('langB').value});
 Object.assign(P,{code:j.c,a:j.t,b:k.t,ver:0,msgs:new Map(),loaded:false,spoken:new Set()});show('pass');passPoll();};
$('langA').onchange=()=>{store.set('passA',$('langA').value);api('/me',{c:P.code,t:P.a,l:$('langA').value});passRender();};
$('langB').onchange=()=>{store.set('passB',$('langB').value);api('/me',{c:P.code,t:P.b,l:$('langB').value});passRender();};
async function passPoll(){clearTimeout(P.timer);if(!P.code)return;let again=1000;
 try{const j=await (await fetch('/state?c='+P.code+'&t='+P.a+'&v='+P.ver+'&tl='+enc(tlOf($('langA').value)))).json();
  if(j.ok){if(j.reset)P.msgs.clear();P.openAt=Date.now()-j.now*1000;const tl=tlOf($('langA').value);
   for(const m of j.msgs){if(!P.loaded)P.spoken.add(m.id);if(m.t)m['t_'+tl]=m.t;P.msgs.set(m.id,Object.assign(P.msgs.get(m.id)||{},m));}
   P.ver=j.v;if(j.more)again=0;else P.loaded=true;passRender();}}catch(e){again=2500;}
 if(P.code)P.timer=setTimeout(passPoll,again);}
function passLine(m,side){const lang=$(side==='A'?'langA':'langB').value,tl=tlOf(lang);const mine=(side==='A')===(m.w===0);
 if(!needsTr(m,tl))return {t:m.x,mine};const k='t_'+tl;if(m[k])return {t:m[k],mine};
 if(!m['w_'+tl]){m['w_'+tl]=1;translate(m.x,tl).then(t=>{if(t){m[k]=t;api('/tr',{c:P.code,t:P.a,id:m.id,tl,x:t}).catch(()=>{});passRender();}else m['w_'+tl]=0;});}
 return {t:'…',mine};}
function passRender(){const ids=[...P.msgs.keys()].sort((a,b)=>a-b).slice(-6);
 for(const side of ['A','B']){$('l'+side).innerHTML=ids.map(id=>{const r=passLine(P.msgs.get(id),side);return '<div class="'+(r.mine?'mine':'theirs')+'">'+esc(r.t)+'</div>';}).join('');}
 for(const id of ids){const m=P.msgs.get(id);if(P.spoken.has(id))continue;
  const to=m.w===0?'B':'A',lang=$(to==='A'?'langA':'langB').value,r=passLine(m,to);
  if(r.t==='…')continue;P.spoken.add(id);if(!P.mic)speak(r.t,lang);}}
function passMic(side){if(P.mic){try{P.mic.stop()}catch(e){}return;}
 const lang=$(side==='A'?'langA':'langB').value,tok=side==='A'?P.a:P.b;P.who=side;
 P.mic=listen(lang,false,{final:t=>api('/say',{c:P.code,t:tok,x:t,l:lang,k:0}).then(passPoll),
  end:()=>{P.mic=null;$('micA').classList.remove('on');$('micB').classList.remove('on');}});
 if(P.mic)$('mic'+side).classList.add('on');}
$('micA').onclick=()=>passMic('A');$('micB').onclick=()=>passMic('B');
$('passEnd').onclick=async()=>{if(P.mic)try{P.mic.stop()}catch(e){};const c=P.code;P.code='';clearTimeout(P.timer);
 await api('/leave',{c,t:P.b}).catch(()=>{});await api('/leave',{c,t:P.a}).catch(()=>{});show('home');};

// ── tools ────────────────────────────────────────────────────────────────────
$('goTools').onclick=()=>show('tools');$('toolsBack').onclick=()=>show('home');
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tabs button').forEach(x=>x.classList.toggle('sel',x===b));
 document.querySelectorAll('[data-p]').forEach(p=>p.hidden=p.dataset.p!==b.dataset.t);});
fillLangs($('dLang'),$('lang').value);fillLangs($('rLang'),$('lang').value);fillLangs($('tLang'),store.get('tLang','en-IN'));
let dRec=null,dOn=false;
$('dMic').onclick=()=>{if(dOn){dOn=false;dRec&&dRec.stop();return;}dOn=true;
 const go=()=>{dRec=listen($('dLang').value,true,{final:t=>{const a=$('dText');a.value+=(a.value&&!/\s$/.test(a.value)?' ':'')+t;$('dInter').textContent='';},
  interim:t=>$('dInter').textContent=t,end:f=>{dRec=null;if(dOn&&!f)setTimeout(go,250);else{dOn=false;$('dMic').textContent='🎤 Start';}}});
  if(!dRec){dOn=false;$('dMic').textContent='🎤 Start';}};
 go();if(dRec)$('dMic').textContent='⏹ Stop';};
$('dCopy').onclick=()=>{navigator.clipboard&&navigator.clipboard.writeText($('dText').value).then(()=>toast('Copied'));};
$('dClear').onclick=()=>$('dText').value='';
$('rGo').onclick=()=>{const v=pickVoice($('rLang').value);$('rNote').textContent=v?'Voice: '+v.name:'No '+L($('rLang').value)[0]+' voice on this device.';
 if(v){speechSynthesis.cancel();const u=new SpeechSynthesisUtterance($('rText').value);u.voice=v;u.lang=$('rLang').value;u.rate=+$('rRate').value;speechSynthesis.speak(u);}};
$('rStop').onclick=()=>window.speechSynthesis&&speechSynthesis.cancel();
$('tGo').onclick=async()=>{const x=$('tText').value.trim();if(!x)return;store.set('tLang',$('tLang').value);$('tOut').textContent='…';
 const t=await translate(x,tlOf($('tLang').value));$('tOut').textContent=t||'Could not translate. Is there internet?';$('tSay').hidden=$('tCopy').hidden=!t;};
$('tSay').onclick=()=>speak($('tOut').textContent,$('tLang').value,{now:true});
$('tCopy').onclick=()=>{navigator.clipboard&&navigator.clipboard.writeText($('tOut').textContent).then(()=>toast('Copied'));};

// ── the caption screen ───────────────────────────────────────────────────────
const T={code:'',ver:0,msgs:new Map(),timer:0,voice:false,spoken:new Set(),loaded:false,people:[]};
function tvStart(c){T.code=c.toUpperCase();show('tv');$('tvCode').textContent=T.code;$('tvAddr').textContent=location.host+'/#tv='+T.code;
 fillLangs($('tvLang'),store.get('tvLang',$('lang').value));tvPoll();}
$('tvLang').onchange=()=>{store.set('tvLang',$('tvLang').value);tvRender();};
document.addEventListener('pointerdown',()=>{if(!$('tv').hidden&&!T.voice){T.voice=true;$('tvTap').hidden=true;}});
async function tvPoll(){clearTimeout(T.timer);let again=1000;
 const tl=tlOf($('tvLang').value);
 try{const j=await (await fetch('/state?c='+T.code+'&v='+T.ver+'&tl='+enc(tl))).json();
  if(j.ok){if(j.reset)T.msgs.clear();T.people=j.people;
   for(const m of j.msgs){if(!T.loaded)T.spoken.add(m.id);if(m.t)m['t_'+tl]=m.t;T.msgs.set(m.id,Object.assign(T.msgs.get(m.id)||{},m));}
   T.ver=j.v;if(j.more)again=0;else T.loaded=true;tvRender();}
  else $('caps').innerHTML='<div class="cap"><div class="t">No room '+esc(T.code)+' on this board.</div></div>';}catch(e){again=2500;}
 T.timer=setTimeout(tvPoll,again);}
function tvText(m){const tl=tlOf($('tvLang').value);if(!needsTr(m,tl))return m.x;const k='t_'+tl;if(m[k])return m[k];
 if(!m['w_'+tl]){m['w_'+tl]=1;translate(m.x,tl).then(t=>{if(t){m[k]=t;api('/tr',{c:T.code,id:m.id,tl,x:t}).catch(()=>{});tvRender();}else m['w_'+tl]=0;});}return null;}
function tvRender(){const ids=[...T.msgs.keys()].sort((a,b)=>a-b).slice(-4);
 $('caps').innerHTML=ids.map((id,i)=>{const m=T.msgs.get(id),t=tvText(m);
  return '<div class="cap'+(i<ids.length-1?' old':'')+'"><div class="n" style="color:'+(m.w>=0?COLORS[m.w%10]:'')+'">'+esc(m.n)+'</div><div class="t">'+esc(t||m.x)+'</div></div>';}).join('');
 $('tvLive').textContent=T.people.filter(p=>p.live).map(p=>p.n+': '+p.live+'…').join('   ');
 for(const id of ids){if(T.spoken.has(id))continue;const t=tvText(T.msgs.get(id));if(t==null)continue;T.spoken.add(id);if(T.voice)speak(t,$('tvLang').value);}}

// ── start ────────────────────────────────────────────────────────────────────
(function(){const h=new URLSearchParams(location.hash.slice(1));
 if(h.get('tv'))return tvStart(h.get('tv'));
 const c=(h.get('c')||'').toUpperCase(),seat=store.get('seat',{});
 if(c&&seat[c]){api('/join',{c,t:seat[c],n:$('name').value||'Guest',l:$('lang').value}).then(j=>{if(j.ok)enterRoom(j.c,j.t);else{show('home');listRooms();}});return;}
 if(c)$('code').value=c;show('home');listRooms();})();
</script></body></html>)HTML";
