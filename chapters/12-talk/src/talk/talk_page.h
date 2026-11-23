#pragma once
// The page, served the same over http and https. Phones listen, translate and speak here; the
// board (talk.h) only keeps the room. Views: home, room, pass (one phone, two people), tools,
// and the caption screen (#tv=CODE). Icons are drawn inline (an SVG sprite at the top), so the
// page needs nothing from the internet but the translators.

static const char TALK_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#f6f7f9" media="(prefers-color-scheme: light)">
<meta name="theme-color" content="#0f1115" media="(prefers-color-scheme: dark)">
<title>Talk</title>
<link rel="icon" href="data:,">
<style>
:root{--bg:#f6f7f9;--surface:#fff;--surface2:#f0f2f5;--line:#e2e5ea;--text:#151922;--muted:#667085;--acc:#3557d4;--acc-ink:#fff;--acc-soft:#e9eefc;
 --danger:#c93b3b;--warn:#b7791f;--warn-soft:#fdf6e7;--mine:#3557d4;--mine-ink:#fff;--mine-sub:rgba(255,255,255,.78);
 --shadow:0 1px 2px rgba(16,24,40,.05),0 1px 3px rgba(16,24,40,.07);--pop:0 12px 32px rgba(16,24,40,.16);--r:12px;
 --p0:#3557d4;--p1:#0f8a5f;--p2:#b35c00;--p3:#c0392b;--p4:#7c4dcc;--p5:#0e7c8c;--p6:#a3446e;--p7:#5a7d1a;--p8:#8a5a2b;--p9:#4b5563;}
@media (prefers-color-scheme:dark){:root{--bg:#0f1115;--surface:#171a21;--surface2:#1f232c;--line:#2b303b;--text:#e7e9ee;--muted:#959cab;--acc:#6f8cff;--acc-ink:#0f1115;
 --acc-soft:#1d2540;--danger:#ef6b6b;--warn:#e0a84a;--warn-soft:#2a2416;--mine:#2f4bb8;--mine-ink:#fff;--mine-sub:rgba(255,255,255,.72);
 --shadow:none;--pop:0 12px 32px rgba(0,0,0,.5);
 --p0:#8aa2ff;--p1:#4fd1a0;--p2:#f0a85a;--p3:#ff8a80;--p4:#b79bff;--p5:#5fd3e0;--p6:#f28cb8;--p7:#a6d36a;--p8:#d9a776;--p9:#aab2c0;}}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,"Noto Sans","Noto Sans Devanagari","Noto Sans Tamil",sans-serif;-webkit-font-smoothing:antialiased}
body.big{font-size:19px}
[hidden]{display:none!important}
.ic{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;flex:none;vertical-align:middle}
.ic.s{width:16px;height:16px}.ic.l{width:26px;height:26px}
.wrap{max-width:560px;margin:0 auto;padding:20px 16px 32px}
h1{font-size:22px;line-height:1.2;margin:0;letter-spacing:-.01em}h2{font-size:15px;margin:0 0 10px;font-weight:600}
p{margin:6px 0}.muted{color:var(--muted)}.small{font-size:13px}
.brand{display:flex;gap:12px;align-items:center;margin:4px 0 18px}
.mark{width:40px;height:40px;border-radius:11px;background:var(--acc);color:var(--acc-ink);display:grid;place-items:center}
.card{background:var(--surface);border:1px solid var(--line);border-radius:var(--r);padding:16px;margin:12px 0;box-shadow:var(--shadow)}
.field{display:block;margin:0 0 12px}.field>span{display:block;font-size:13px;font-weight:500;color:var(--muted);margin-bottom:6px}
.row{display:flex;gap:8px;align-items:center}.row>*{min-width:0}.grow{flex:1}
select,input,textarea{font:inherit;color:var(--text);background:var(--surface);border:1px solid var(--line);border-radius:10px;padding:10px 12px;width:100%;outline:none;transition:border-color .15s,box-shadow .15s}
select:focus,input:focus,textarea:focus{border-color:var(--acc);box-shadow:0 0 0 3px var(--acc-soft)}
textarea{min-height:120px;resize:vertical}
input[type=range]{padding:0;border:0;box-shadow:none;accent-color:var(--acc)}
.btn{font:inherit;font-weight:600;display:inline-flex;align-items:center;justify-content:center;gap:8px;border-radius:10px;padding:10px 16px;cursor:pointer;
 border:1px solid transparent;background:var(--acc);color:var(--acc-ink);touch-action:manipulation;transition:filter .15s,transform .05s;white-space:nowrap}
.btn:active{transform:scale(.98)}.btn:hover{filter:brightness(1.05)}
.btn.sec{background:var(--surface);color:var(--text);border-color:var(--line)}
.btn.ghost{background:transparent;color:var(--text);border-color:transparent}
.btn.block{width:100%}.btn.sm{padding:6px 12px;font-size:13px}
.btn:disabled{opacity:.45}
.iconbtn{width:40px;height:40px;padding:0;border-radius:10px;background:transparent;color:var(--text);border:1px solid transparent;display:inline-grid;place-items:center;cursor:pointer}
.iconbtn:hover{background:var(--surface2)}
:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
a{color:var(--acc);text-decoration:none}
.or{display:flex;align-items:center;gap:10px;color:var(--muted);font-size:13px;margin:14px 0 10px}.or:before,.or:after{content:"";flex:1;height:1px;background:var(--line)}
.code-in{text-transform:uppercase;letter-spacing:.2em;font-weight:600}
.notice{display:flex;gap:12px;align-items:flex-start;border-radius:var(--r);padding:14px;margin:12px 0;background:var(--warn-soft);border:1px solid color-mix(in srgb,var(--warn) 35%,transparent)}
.notice .ic{color:var(--warn);margin-top:2px}
.err{color:var(--danger);font-size:13px;min-height:1em;margin:8px 0 0}
.list .item{display:flex;gap:12px;align-items:center;padding:10px 0;border-top:1px solid var(--line)}.list .item:first-child{border:0;padding-top:0}
.badge{font-weight:700;letter-spacing:.14em;background:var(--surface2);border-radius:8px;padding:4px 8px;font-size:13px}
.tag{display:inline-block;background:var(--surface2);color:var(--muted);border-radius:6px;padding:1px 7px;font-size:12px;margin:3px 4px 0 0}
.tiles{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.tile{text-align:left;background:var(--surface);border:1px solid var(--line);border-radius:var(--r);padding:14px;cursor:pointer;color:var(--text);font:inherit;box-shadow:var(--shadow)}
.tile:hover{border-color:var(--acc)}.tile .ic{color:var(--acc);margin-bottom:8px}.tile b{display:block;font-size:14px}.tile span{font-size:13px;color:var(--muted)}
.foot{font-size:12px;color:var(--muted);margin-top:18px}

/* the room */
#room{display:flex;flex-direction:column;height:100%;max-width:760px;margin:0 auto;width:100%;overflow:hidden;background:var(--bg)}
#room>*{min-width:0}
.top{display:flex;gap:6px;align-items:center;padding:8px 8px 8px 6px;background:var(--surface);border-bottom:1px solid var(--line)}
.title{flex:1;min-width:0}.title .code{font-weight:700;letter-spacing:.14em;font-size:16px}.title .sub{font-size:12px;color:var(--muted);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.people{display:flex;gap:6px;overflow-x:auto;padding:8px 12px;background:var(--surface);border-bottom:1px solid var(--line);scrollbar-width:none}
.chip{flex:none;display:flex;gap:7px;align-items:center;border:1px solid var(--line);border-radius:999px;padding:3px 10px 3px 3px;font-size:13px;background:var(--surface)}
.av{width:24px;height:24px;border-radius:50%;display:grid;place-items:center;font-size:11px;font-weight:700;color:#fff;flex:none}
.chip.away{opacity:.5}.chip .lng{color:var(--muted)}
.rec{width:8px;height:8px;border-radius:50%;background:var(--danger);display:inline-block;animation:blink 1.2s infinite}
@keyframes blink{50%{opacity:.3}}
.msgs{flex:1;overflow-y:auto;padding:16px 12px;display:flex;flex-direction:column;gap:10px}
.empty{margin:auto;text-align:center;color:var(--muted);max-width:280px;font-size:14px}.empty .ic{width:36px;height:36px;color:var(--acc);margin-bottom:6px}
.msg{max-width:86%;background:var(--surface);border:1px solid var(--line);border-radius:14px 14px 14px 4px;padding:8px 12px 10px;align-self:flex-start;box-shadow:var(--shadow)}
.msg.mine{align-self:flex-end;background:var(--mine);border-color:var(--mine);color:var(--mine-ink);border-radius:14px 14px 4px 14px}
.msg .who{font-size:12px;display:flex;gap:10px;justify-content:space-between;align-items:center;color:var(--muted)}
.msg .who b{font-weight:600;display:inline-flex;gap:4px;align-items:center}
.msg.mine .who,.msg.mine .who b{color:var(--mine-sub)!important}
.msg .main{font-size:1.07em;margin-top:2px;word-wrap:break-word}
.msg .orig{font-size:.86em;color:var(--muted);margin-top:4px;padding-top:4px;border-top:1px dashed var(--line);word-wrap:break-word}
.msg.mine .orig{color:var(--mine-sub);border-color:rgba(255,255,255,.25)}
.say{background:none;border:0;padding:2px;margin:-2px;color:inherit;cursor:pointer;display:inline-grid;border-radius:6px}.say:hover{background:var(--surface2)}
.msg.mine .say:hover{background:rgba(255,255,255,.15)}
.live{padding:0 14px}.live div{color:var(--muted);font-size:13px;padding:3px 0;display:flex;gap:8px;align-items:center}
.phrases{display:flex;gap:6px;overflow-x:auto;padding:8px 12px 0;scrollbar-width:none}
.phrases button{flex:none;background:var(--surface);border:1px solid var(--line);color:var(--text);font:inherit;font-size:13px;padding:6px 12px;border-radius:999px;cursor:pointer}
.phrases button:hover{border-color:var(--acc);color:var(--acc)}
.bar{display:flex;gap:8px;align-items:center;padding:10px 10px calc(10px + env(safe-area-inset-bottom));background:var(--surface);border-top:1px solid var(--line)}
.compose{flex:1;min-width:0;display:flex;align-items:center;background:var(--surface2);border:1px solid var(--line);border-radius:999px;padding:0 4px 0 14px}
.compose:focus-within{border-color:var(--acc);box-shadow:0 0 0 3px var(--acc-soft)}
.compose input{border:0;background:transparent;padding:10px 0;box-shadow:none!important;min-width:0}
.compose .iconbtn{width:34px;height:34px;border-radius:50%;color:var(--acc)}
.mic{width:48px;height:48px;border-radius:50%;padding:0;flex:none;border:0;background:var(--acc);color:var(--acc-ink);display:grid;place-items:center;cursor:pointer;box-shadow:var(--shadow)}
.mic.on{background:var(--danger);color:#fff;animation:pulse 1.4s infinite}
@keyframes pulse{0%{box-shadow:0 0 0 0 color-mix(in srgb,var(--danger) 45%,transparent)}100%{box-shadow:0 0 0 14px transparent}}
.menu{position:fixed;right:10px;top:54px;z-index:5;background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:6px;min-width:250px;box-shadow:var(--pop)}
.menu button{display:flex;gap:12px;align-items:center;width:100%;text-align:left;background:none;border:0;color:var(--text);font:inherit;padding:10px 12px;border-radius:8px;cursor:pointer}
.menu button:hover{background:var(--surface2)}.menu button .ic{color:var(--muted)}
.menu hr{border:0;border-top:1px solid var(--line);margin:6px 4px}
.menu .danger,.menu .danger .ic{color:var(--danger)}
.toast{position:fixed;left:50%;bottom:96px;transform:translateX(-50%);background:var(--text);color:var(--bg);border-radius:10px;padding:10px 14px;z-index:9;max-width:calc(100% - 32px);text-align:center;font-size:14px;box-shadow:var(--pop)}

/* pass: one phone, two people facing each other */
#pass{display:flex;flex-direction:column;height:100%;background:var(--bg)}
.half{flex:1;display:flex;flex-direction:column;padding:14px 16px;gap:10px;min-height:0}
.half.flip{transform:rotate(180deg);border-bottom:1px solid var(--line);background:var(--surface)}
.half .who{display:flex;justify-content:space-between;align-items:center;font-size:12px;font-weight:600;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.half .lines{flex:1;overflow-y:auto;display:flex;flex-direction:column;justify-content:flex-end;gap:8px}
.half .lines div{font-size:1.25em;line-height:1.35}.half .lines .hint{font-size:14px;color:var(--muted);text-align:center;margin:auto 0}.half .lines .mine{color:var(--muted);font-size:1em}
.half .ctl{display:flex;gap:10px;align-items:center}
.half .mic{width:64px;height:64px}

/* tools */
.seg{display:flex;background:var(--surface2);border-radius:10px;padding:4px;gap:4px;margin:14px 0}
.seg button{flex:1;display:flex;gap:6px;align-items:center;justify-content:center;border:0;background:transparent;color:var(--muted);font:inherit;font-weight:500;font-size:14px;padding:8px 6px;border-radius:8px;cursor:pointer}
.seg button.sel{background:var(--surface);color:var(--text);box-shadow:var(--shadow)}
.out{font-size:1.2em;margin:12px 0 4px;min-height:1.2em;word-wrap:break-word}

/* caption screen */
#tv{--p0:#8aa2ff;--p1:#4fd1a0;--p2:#f0a85a;--p3:#ff8a80;--p4:#b79bff;--p5:#5fd3e0;--p6:#f28cb8;--p7:#a6d36a;--p8:#d9a776;--p9:#aab2c0;min-height:100%;padding:3vh 4vw calc(3vh + 52px);display:flex;flex-direction:column;background:#0b0d11;color:#eef0f4}
#tv .head{display:flex;justify-content:space-between;align-items:center;gap:12px;color:#8a92a3;font-size:max(13px,2vh)}
#tv .head b{color:#eef0f4;letter-spacing:.14em}
#tv select{width:auto;background:#161a22;color:#eef0f4;border-color:#2b303b}
#tv .caps{flex:1;display:flex;flex-direction:column;justify-content:flex-end;gap:2.6vh;padding:3vh 0}
#tv .cap .n{font-size:max(13px,2.3vh);font-weight:600;letter-spacing:.02em}
#tv .cap .t{font-size:max(22px,5vh);line-height:1.25;font-weight:600}
#tv .cap.old{opacity:.45}#tv .cap.old .t{font-size:max(17px,3.4vh)}
#tv .lv{font-size:max(15px,3vh);color:#8a92a3;min-height:4vh}
#tvTap{position:fixed;left:0;right:0;bottom:0;background:#1d2540;color:#dfe5ff;display:flex;gap:10px;align-items:center;justify-content:center;padding:14px;font-weight:500}
</style></head><body>

<svg width="0" height="0" style="position:absolute" aria-hidden="true">
<symbol id="i-mic" viewBox="0 0 24 24"><rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3M8.5 21h7"/></symbol>
<symbol id="i-stop" viewBox="0 0 24 24"><rect x="6.5" y="6.5" width="11" height="11" rx="2"/></symbol>
<symbol id="i-vol" viewBox="0 0 24 24"><path d="M11 5 6 9H3v6h3l5 4z"/><path d="M15.5 8.5a5 5 0 0 1 0 7M18.5 5.5a9 9 0 0 1 0 13"/></symbol>
<symbol id="i-mute" viewBox="0 0 24 24"><path d="M11 5 6 9H3v6h3l5 4z"/><path d="m16 9.5 5 5m0-5-5 5"/></symbol>
<symbol id="i-more" viewBox="0 0 24 24"><circle cx="12" cy="5.5" r=".8"/><circle cx="12" cy="12" r=".8"/><circle cx="12" cy="18.5" r=".8"/></symbol>
<symbol id="i-back" viewBox="0 0 24 24"><path d="m15 18-6-6 6-6"/></symbol>
<symbol id="i-zap" viewBox="0 0 24 24"><path d="M13 2 4 14h7l-1 8 9-12h-7z"/></symbol>
<symbol id="i-send" viewBox="0 0 24 24"><path d="M21 3 10 14M21 3l-7 18-4-7-7-4z"/></symbol>
<symbol id="i-captions" viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 11h3M13 11h4M7 15h6M15 15h2"/></symbol>
<symbol id="i-text" viewBox="0 0 24 24"><path d="M4 7V5h10v2M9 5v14M7 19h4M14 13v-1h7v1M17.5 12v7M16 19h3"/></symbol>
<symbol id="i-tv" viewBox="0 0 24 24"><rect x="2.5" y="4" width="19" height="13" rx="2"/><path d="M8 21h8M12 17v4"/></symbol>
<symbol id="i-lang" viewBox="0 0 24 24"><path d="M4 5h9M8.5 3v2M11 5c-1 4-3.5 7-7 9M6.5 9c1 2.2 2.8 4 5 5"/><path d="m13 21 4-9 4 9M14.4 18h5.2"/></symbol>
<symbol id="i-down" viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></symbol>
<symbol id="i-trash" viewBox="0 0 24 24"><path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13M10 11v5M14 11v5"/></symbol>
<symbol id="i-out" viewBox="0 0 24 24"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4M16 17l5-5-5-5M21 12H9"/></symbol>
<symbol id="i-phone" viewBox="0 0 24 24"><rect x="6.5" y="2.5" width="11" height="19" rx="2.5"/><path d="M6.5 12h11M11 18.5h2"/></symbol>
<symbol id="i-tool" viewBox="0 0 24 24"><path d="M14.7 6.3a4 4 0 0 0-5.3 5.2L3.5 17.4a1.4 1.4 0 0 0 0 2l1.1 1.1a1.4 1.4 0 0 0 2 0l5.9-5.9a4 4 0 0 0 5.2-5.3l-2.5 2.5-2.3-.5-.5-2.3z"/></symbol>
<symbol id="i-copy" viewBox="0 0 24 24"><rect x="9" y="9" width="12" height="12" rx="2"/><path d="M5 15H4.5A1.5 1.5 0 0 1 3 13.5v-9A1.5 1.5 0 0 1 4.5 3h9A1.5 1.5 0 0 1 15 4.5V5"/></symbol>
<symbol id="i-x" viewBox="0 0 24 24"><path d="M6 6l12 12M18 6 6 18"/></symbol>
<symbol id="i-lock" viewBox="0 0 24 24"><rect x="4.5" y="10.5" width="15" height="10" rx="2"/><path d="M8 10.5V7a4 4 0 0 1 8 0v3.5"/></symbol>
<symbol id="i-info" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8h.01"/></symbol>
<symbol id="i-chat" viewBox="0 0 24 24"><path d="M21 12a8 8 0 0 1-11.6 7.1L4 20.5l1.4-4.9A8 8 0 1 1 21 12z"/></symbol>
<symbol id="i-play" viewBox="0 0 24 24"><path d="M7 5v14l12-7z"/></symbol>
</svg>

<!-- home -->
<div class="wrap" id="home">
<div class="brand"><div class="mark"><svg class="ic"><use href="#i-lang"/></svg></div>
 <div><h1>Talk</h1><div class="muted small">Everyone speaks and reads in their own language</div></div></div>
<div class="notice" id="insecure" hidden><svg class="ic"><use href="#i-lock"/></svg>
 <div><b>The microphone needs the secure page</b>
 <p class="small" style="margin:4px 0 8px">Typing and reading work here. To speak, open <a id="secLink" href="#"></a>.
 The first time, the phone says the page is "not private", because the board made its own certificate.
 Android: <i>Advanced</i>, then <i>Proceed</i>. iPhone: <i>Show details</i>, then <i>visit this website</i>.</p></div></div>
<div class="notice" id="nosr" hidden><svg class="ic"><use href="#i-info"/></svg>
 <div><b>This browser can't listen</b><p class="small" style="margin:4px 0 0">Chrome, Edge or Safari can. You can still type, read and hear.</p></div></div>
<div class="card">
 <label class="field"><span>Your name</span><input id="name" maxlength="24" placeholder="e.g. Asha" autocomplete="name"></label>
 <label class="field"><span>Language you speak</span><select id="lang"></select></label>
 <button class="btn block" id="start">Start a room</button>
 <div class="or">or join with a code</div>
 <div class="row"><input id="code" class="code-in grow" maxlength="4" placeholder="ABCD" autocapitalize="characters"><button class="btn sec" id="join">Join</button></div>
 <p class="err" id="homeErr"></p>
</div>
<div class="card" id="openRooms" hidden><h2>Rooms on this board</h2><div class="list" id="roomList"></div></div>
<div class="tiles">
 <button class="tile" id="goPass"><svg class="ic"><use href="#i-phone"/></svg><b>One phone, two people</b><span>Pass it across the table</span></button>
 <button class="tile" id="goTools"><svg class="ic"><use href="#i-tool"/></svg><b>Tools</b><span>Dictate, read aloud, translate</span></button>
</div>
<p class="foot">Speech recognition usually sends your voice to Google or Apple, and translation sends the words to Google (or MyMemory).
The board keeps a room's messages in memory only, and forgets them when it restarts.</p>
</div>

<!-- the room -->
<div id="room" hidden>
<div class="top">
 <button class="iconbtn" id="back" aria-label="Back"><svg class="ic"><use href="#i-back"/></svg></button>
 <div class="title"><div class="code" id="rCode"></div><div class="sub" id="rInfo"></div></div>
 <button class="iconbtn" id="spk" aria-label="Read messages aloud"></button>
 <button class="iconbtn" id="more" aria-label="More"><svg class="ic"><use href="#i-more"/></svg></button>
</div>
<div class="people" id="people"></div>
<div class="msgs" id="msgs"></div>
<div class="live" id="live"></div>
<div class="phrases" id="phrases" hidden></div>
<div class="bar">
 <button class="iconbtn" id="phr" aria-label="Quick phrases" title="Quick phrases"><svg class="ic"><use href="#i-zap"/></svg></button>
 <div class="compose"><input id="typed" placeholder="Type a message" enterkeyhint="send" autocomplete="off">
  <button class="iconbtn" id="sendBtn" aria-label="Send" hidden><svg class="ic"><use href="#i-send"/></svg></button></div>
 <button class="mic" id="mic" aria-label="Speak"><svg class="ic"><use href="#i-mic"/></svg></button>
</div>
</div>
<div class="menu" id="menu" hidden>
 <button id="mKeep"></button>
 <button id="mTv"><svg class="ic"><use href="#i-tv"/></svg>Show captions on a TV</button>
 <button id="mBig"><svg class="ic"><use href="#i-text"/></svg>Bigger text</button>
 <button id="mLang"><svg class="ic"><use href="#i-lang"/></svg>Change my language</button>
 <button id="mSave"><svg class="ic"><use href="#i-down"/></svg>Save the conversation</button>
 <hr>
 <button id="mClear" class="danger" hidden><svg class="ic"><use href="#i-trash"/></svg>Clear for everyone</button>
 <button id="mLeave" class="danger"><svg class="ic"><use href="#i-out"/></svg>Leave the room</button>
</div>

<!-- pass -->
<div id="pass" hidden>
<div class="half flip" id="hB">
 <div class="who"><span>Guest</span></div>
 <div class="lines" id="lB"></div>
 <div class="ctl"><select class="grow" id="langB"></select><button class="mic" id="micB" aria-label="Speak"><svg class="ic l"><use href="#i-mic"/></svg></button></div>
</div>
<div class="half" id="hA">
 <div class="who"><span>You</span><button class="iconbtn" id="passEnd" aria-label="End"><svg class="ic"><use href="#i-x"/></svg></button></div>
 <div class="lines" id="lA"></div>
 <div class="ctl"><select class="grow" id="langA"></select><button class="mic" id="micA" aria-label="Speak"><svg class="ic l"><use href="#i-mic"/></svg></button></div>
</div>
</div>

<!-- tools -->
<div class="wrap" id="tools" hidden>
<div class="row"><button class="iconbtn" id="toolsBack" aria-label="Back"><svg class="ic"><use href="#i-back"/></svg></button><h1>Tools</h1></div>
<div class="seg tabs">
 <button data-t="dict" class="sel"><svg class="ic s"><use href="#i-mic"/></svg>Dictate</button>
 <button data-t="read"><svg class="ic s"><use href="#i-vol"/></svg>Read aloud</button>
 <button data-t="tran"><svg class="ic s"><use href="#i-lang"/></svg>Translate</button></div>
<div class="card" data-p="dict">
 <label class="field"><span>Language</span><div class="row"><select id="dLang" class="grow"></select><button class="btn" id="dMic"></button></div></label>
 <p class="muted small" id="dInter"></p>
 <textarea id="dText" placeholder="What you say appears here"></textarea>
 <div class="row" style="margin-top:10px"><button class="btn sec" id="dCopy"><svg class="ic s"><use href="#i-copy"/></svg>Copy</button><button class="btn ghost" id="dClear">Clear</button></div>
</div>
<div class="card" data-p="read" hidden>
 <textarea id="rText" placeholder="Paste or type text to hear it"></textarea>
 <label class="field" style="margin-top:12px"><span>Voice language</span><select id="rLang"></select></label>
 <label class="field"><span>Speed</span><input id="rRate" type="range" min="0.5" max="1.6" step="0.1" value="1"></label>
 <div class="row"><button class="btn" id="rGo"><svg class="ic s"><use href="#i-play"/></svg>Read</button><button class="btn sec" id="rStop"><svg class="ic s"><use href="#i-stop"/></svg>Stop</button></div>
 <p class="muted small" id="rNote"></p>
</div>
<div class="card" data-p="tran" hidden>
 <textarea id="tText" placeholder="Type in any language"></textarea>
 <label class="field" style="margin-top:12px"><span>Translate to</span><div class="row"><select id="tLang" class="grow"></select><button class="btn" id="tGo">Translate</button></div></label>
 <p class="out" id="tOut"></p>
 <div class="row"><button class="btn sec" id="tSay" hidden><svg class="ic s"><use href="#i-vol"/></svg>Say it</button><button class="btn sec" id="tCopy" hidden><svg class="ic s"><use href="#i-copy"/></svg>Copy</button></div>
</div>
</div>

<!-- caption screen -->
<div id="tv" hidden>
<div class="head"><div><b id="tvCode"></b> &middot; <span id="tvAddr"></span></div><select id="tvLang"></select></div>
<div class="caps" id="caps"></div>
<div class="lv" id="tvLive"></div>
<div id="tvTap"><svg class="ic"><use href="#i-vol"/></svg>Tap anywhere to hear the captions read aloud</div>
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
// Avatars keep one strong colour; names use --p0..9, lighter in dark mode so they stay readable.
const AV=['#3557d4','#0f8a5f','#b35c00','#c0392b','#7c4dcc','#0e7c8c','#a3446e','#5a7d1a','#8a5a2b','#4b5563'];
const COLORS=AV.map((_,i)=>'var(--p'+i+')');
const ic=(n,c)=>'<svg class="ic'+(c?' '+c:'')+'" aria-hidden="true"><use href="#i-'+n+'"/></svg>';

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
// Every request to the board goes through one queue, so a phone holds one connection to it, not
// six. A secure connection costs the board ~37 KB, and it can only keep about three.
let chain=Promise.resolve();
function queued(fn){const p=chain.then(fn,fn);chain=p.catch(()=>{});return p;}
async function boardFetch(url,opt){const c=new AbortController();const t=setTimeout(()=>c.abort(),10000);
 try{const r=await fetch(url,Object.assign({signal:c.signal},opt||{}));return await r.json();}finally{clearTimeout(t)}}
function api(path,data){return queued(()=>boardFetch(path,{method:'POST',body:new URLSearchParams(data||{})}));}
function getJ(url){return queued(()=>boardFetch(url));}
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
// How soon to ask again: quickly while someone is talking, slowly in a quiet room, rarely when
// the screen is off.
function pace(active){return document.hidden?5000:active?800:2000;}

// ── home ─────────────────────────────────────────────────────────────────────
fillLangs($('lang'),store.get('lang','en-IN'));$('name').value=store.get('name','');
$('lang').onchange=()=>store.set('lang',$('lang').value);$('name').onchange=()=>store.set('name',$('name').value.trim());
if(!isSecureContext){$('insecure').hidden=false;$('secLink').href=secureUrl();$('secLink').textContent='https://'+(location.hostname||'talk.local');}
if(!SR)$('nosr').hidden=false;
function needName(){const n=$('name').value.trim();if(!n){$('name').focus();$('homeErr').textContent='Type your name first.';return null;}$('homeErr').textContent='';store.set('name',n);return n;}
$('start').onclick=async()=>{const n=needName();if(!n)return;const j=await api('/create',{n,l:$('lang').value});
 if(!j.ok){$('homeErr').textContent=j.err==='full'?'The board already has its rooms open. Join one of them.':'Could not start: '+j.err;return;}enterRoom(j.c,j.t);};
async function joinCode(c){const n=needName();if(!n)return;c=c.toUpperCase();const seat=store.get('seat',{});
 const j=await api('/join',{c,n,l:$('lang').value,t:seat[c]||''});
 if(!j.ok){$('homeErr').textContent={noroom:'No room with that code.',roomfull:'That room is full.'}[j.err]||j.err;return;}enterRoom(j.c,j.t);}
$('join').onclick=()=>{const c=$('code').value.trim();if(c.length===4)joinCode(c);else $('homeErr').textContent='The code has four letters.';};
$('code').onkeydown=e=>{if(e.key==='Enter')$('join').click();};
async function listRooms(){if($('home').hidden||document.hidden)return;try{const j=await getJ('/rooms');
 $('openRooms').hidden=!j.rooms.length;
 $('roomList').innerHTML=j.rooms.map(r=>'<div class="item"><span class="badge">'+r.c+'</span><div class="grow"><div><b>'+esc(r.host)+'</b> <span class="muted small">&middot; '+r.n+' here</span></div>'+
  [...new Set(r.langs.map(l=>L(l)[0].split(' ')[0]))].map(n=>'<span class="tag">'+esc(n)+'</span>').join('')+'</div><button class="btn sec sm" data-c="'+r.c+'">Join</button></div>').join('');
 $('roomList').querySelectorAll('button').forEach(b=>b.onclick=()=>joinCode(b.dataset.c));}catch(e){}}
setInterval(listRooms,5000);

// ── the room ─────────────────────────────────────────────────────────────────
const R={code:'',tok:'',me:-1,host:-1,ver:0,msgs:new Map(),people:[],timer:0,polling:false,loaded:false,openAt:0,speakOn:true,
 mic:null,mode:'off',sent:new Set(),spoken:new Set(),trying:new Set()};
function myLang(){const p=R.people.find(p=>p.i===R.me);return p?p.l:$('lang').value;}
function enterRoom(c,t){Object.assign(R,{code:c,tok:t,me:-1,ver:0,warned:false,msgs:new Map(),people:[],loaded:false,sent:new Set(),spoken:new Set(),trying:new Set()});
 const seat=store.get('seat',{});seat[c]=t;store.set('seat',seat);
 R.speakOn=store.get('speak',true);spkUi();
 location.hash='c='+c;show('room');$('rCode').textContent=c;$('msgs').innerHTML='';poll();}
async function poll(){clearTimeout(R.timer);if(!R.code)return;if(R.polling){R.soon=true;return;}R.polling=true;let again=1000;
 try{const j=await getJ('/state?c='+R.code+'&t='+R.tok+'&v='+R.ver+'&tl='+enc(tlOf(myLang())));
  if(!j.ok){if(j.err==='noroom'){toast('That room has closed.');leaveRoom(true);}R.polling=false;return;}
  const active=R.mode!=='off'||Date.now()-(R.changed||0)<15000||R.people.some(p=>p.on||p.live);
  if(j.same){again=pace(active);throw 0;}
  R.changed=Date.now();
  if(j.reset){R.msgs.clear();$('msgs').innerHTML='';}
  R.me=j.me;R.host=j.host;R.people=j.people;R.openAt=Date.now()-j.now*1000;
  if(R.me<0&&!R.warned){R.warned=true;toast('You are watching this room. Join from the start screen to talk.');}
  for(const m of j.msgs){const old=R.msgs.get(m.id);if(old&&old.t&&!m.t)m.t=old.t;R.msgs.set(m.id,m);if(!R.loaded)R.spoken.add(m.id);}
  R.ver=j.v;if(j.more)again=0;else{R.loaded=true;again=pace(true);}
  renderRoom();
 }catch(e){if(e!==0)again=2500;}
 R.polling=false;if(R.soon){R.soon=false;again=0;}   // something was sent meanwhile: show it now
 if(R.code&&!$('room').hidden)R.timer=setTimeout(poll,again);}
function needsTr(m,tl){return base(m.l)!==base(tl);}
function renderRoom(){
 const tl=tlOf(myLang());
 const here=R.people.filter(p=>!p.away).length;
 $('rInfo').textContent=L(myLang())[0]+' \u00b7 '+here+(here===1?' person':' people')+(R.me===R.host?' \u00b7 you host':'');$('mClear').hidden=R.me!==R.host;
 $('people').innerHTML=R.people.map(p=>'<div class="chip'+(p.away?' away':'')+'"><span class="av" style="background:'+AV[p.i%10]+'">'+esc(([...p.n][0]||'?').toUpperCase())+'</span>'+
  esc(p.n)+(p.i===R.me?' (you)':'')+' <span class="lng">'+esc(L(p.l)[1])+'</span>'+(p.on?' <span class="rec" title="Listening"></span>':'')+'</div>').join('');
 const box=$('msgs'),stick=box.scrollTop+box.clientHeight>=box.scrollHeight-60;
 const ids=[...R.msgs.keys()].sort((a,b)=>a-b);
 for(const id of ids){const m=R.msgs.get(id);let el=document.getElementById('m'+id);
  if(!el){el=document.createElement('div');el.id='m'+id;el.className='msg'+(m.w===R.me&&m.w>=0?' mine':'')+(m.k===2?' phrase':'');box.appendChild(el);}
  const tr=needsTr(m,tl);if(tr&&!m.t)fetchTr(m,tl);
  const main=tr?(m.t||'\u2026'):m.x,sub=tr?m.x:'';
  const time=new Date(R.openAt+m.at*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});
  const html='<div class="who"><b style="color:'+(m.w>=0?COLORS[m.w%10]:'var(--muted)')+'">'+esc(m.n)+(m.k===2?ic('zap','s'):'')+'</b><span class="row" style="gap:6px">'+time+
   '<button class="say" aria-label="Read aloud">'+ic('vol','s')+'</button></span></div><div class="main">'+esc(main)+'</div>'+(sub?'<div class="orig">'+esc(L(m.l)[1])+': '+esc(sub)+'</div>':'');
  if(el.dataset.h!==html){el.innerHTML=html;el.dataset.h=html;el.querySelector('.say').onclick=()=>sayMsg(m,true);}
  maybeSpeak(m);}
 for(const el of [...box.children])if(el.id!=='empty'&&!R.msgs.has(+el.id.slice(1)))el.remove();
 let em=$('empty');if(!R.msgs.size&&!em){box.insertAdjacentHTML('beforeend','<div class="empty" id="empty">'+ic('chat')+'<div>Tap the mic and speak, or type. Everyone reads it in their own language.</div></div>');}
 else if(R.msgs.size&&em)em.remove();
 if(stick)box.scrollTop=box.scrollHeight;
 $('live').innerHTML=R.people.filter(p=>p.live&&p.i!==R.me).map(p=>'<div><span class="rec"></span><span><b>'+esc(p.n)+'</b> '+esc(p.live)+'\u2026</span></div>').join('');}
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
function sendTyped(){send($('typed').value,1);$('typed').value='';$('sendBtn').hidden=true;}
$('typed').onkeydown=e=>{if(e.key==='Enter')sendTyped();};
$('typed').oninput=()=>{$('sendBtn').hidden=!$('typed').value.trim();};
$('sendBtn').onclick=sendTyped;
function spkUi(){$('spk').innerHTML=ic(R.speakOn?'vol':'mute');$('spk').title=R.speakOn?'Reading new messages aloud':'Not reading aloud';}
$('spk').onclick=()=>{R.speakOn=!R.speakOn;store.set('speak',R.speakOn);spkUi();if(!R.speakOn&&window.speechSynthesis)speechSynthesis.cancel();
 toast(R.speakOn?'New messages will be read aloud':'Messages will not be read aloud');};
// The mic: tap for one sentence; "keep listening" for captions.
// The grey "speaking now" line: only the newest words are sent, at most every 700 ms, so a fast
// talker never queues up a pile of updates.
let livePending=null,liveBusy=false;
function sendLive(t,on){livePending={t,on};if(!liveBusy)flushLive();}
async function flushLive(){if(!livePending||!R.code)return;liveBusy=true;const x=livePending;livePending=null;
 await api('/live',{c:R.code,t:R.tok,x:x.t,on:x.on?1:0}).catch(()=>{});await sleep(700);liveBusy=false;if(livePending)flushLive();}
function micStart(mode){if(R.mic)return;R.mode=mode;
 R.mic=listen(myLang(),mode==='keep',{final:t=>{send(t,0);sendLive('',mode==='keep');},interim:t=>sendLive(t,true),
  end:fatal=>{R.mic=null;if(R.mode==='keep'&&!fatal){setTimeout(()=>{if(R.mode==='keep')micStart('keep');},250);return;}
   R.mode='off';micUi();sendLive('',false);}});
 if(!R.mic)R.mode='off';micUi();}
function micStop(){R.mode='off';if(R.mic){try{R.mic.stop()}catch(e){}}micUi();}
function micUi(){$('mic').classList.toggle('on',R.mode!=='off');$('mic').innerHTML=ic(R.mode!=='off'?'stop':'mic');
 $('mKeep').innerHTML=R.mode==='keep'?ic('stop')+'Stop listening':ic('captions')+'Keep listening (captions)';}
micUi();
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
async function passPoll(){clearTimeout(P.timer);if(!P.code)return;let again=pace(!!P.mic);
 try{const j=await getJ('/state?c='+P.code+'&t='+P.a+'&v='+P.ver+'&tl='+enc(tlOf($('langA').value)));
  if(j.ok&&!j.same){if(j.reset)P.msgs.clear();P.openAt=Date.now()-j.now*1000;const tl=tlOf($('langA').value);
   for(const m of j.msgs){if(!P.loaded)P.spoken.add(m.id);if(m.t)m['t_'+tl]=m.t;P.msgs.set(m.id,Object.assign(P.msgs.get(m.id)||{},m));}
   P.ver=j.v;if(j.more)again=0;else{P.loaded=true;again=pace(true);}passRender();}}catch(e){again=2500;}
 if(P.code)P.timer=setTimeout(passPoll,again);}
function passLine(m,side){const lang=$(side==='A'?'langA':'langB').value,tl=tlOf(lang);const mine=(side==='A')===(m.w===0);
 if(!needsTr(m,tl))return {t:m.x,mine};const k='t_'+tl;if(m[k])return {t:m[k],mine};
 if(!m['w_'+tl]){m['w_'+tl]=1;translate(m.x,tl).then(t=>{if(t){m[k]=t;api('/tr',{c:P.code,t:P.a,id:m.id,tl,x:t}).catch(()=>{});passRender();}else m['w_'+tl]=0;});}
 return {t:'\u2026',mine};}
function passRender(){const ids=[...P.msgs.keys()].sort((a,b)=>a-b).slice(-6);
 for(const side of ['A','B']){$('l'+side).innerHTML=ids.length?ids.map(id=>{const r=passLine(P.msgs.get(id),side);return '<div class="'+(r.mine?'mine':'theirs')+'">'+esc(r.t)+'</div>';}).join(''):
  '<div class="hint">Tap the mic and speak. The other side reads it in their own language.</div>';}
 for(const id of ids){const m=P.msgs.get(id);if(P.spoken.has(id))continue;
  const to=m.w===0?'B':'A',lang=$(to==='A'?'langA':'langB').value,r=passLine(m,to);
  if(r.t==='\u2026')continue;P.spoken.add(id);if(!P.mic)speak(r.t,lang);}}
function passMic(side){if(P.mic){try{P.mic.stop()}catch(e){}return;}
 const lang=$(side==='A'?'langA':'langB').value,tok=side==='A'?P.a:P.b;P.who=side;
 P.mic=listen(lang,false,{final:t=>api('/say',{c:P.code,t:tok,x:t,l:lang,k:0}).then(passPoll),
  end:()=>{P.mic=null;for(const x of ['A','B']){$('mic'+x).classList.remove('on');$('mic'+x).innerHTML=ic('mic','l');}}});
 if(P.mic){$('mic'+side).classList.add('on');$('mic'+side).innerHTML=ic('stop','l');}}
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
  interim:t=>$('dInter').textContent=t,end:f=>{dRec=null;if(dOn&&!f)setTimeout(go,250);else{dOn=false;dUi();}}});
  if(!dRec){dOn=false;dUi();}};
 go();dUi();};
function dUi(){$('dMic').innerHTML=dOn?ic('stop','s')+'Stop':ic('mic','s')+'Start';$('dMic').style.background=dOn?'var(--danger)':'';}
dUi();
$('dCopy').onclick=()=>{navigator.clipboard&&navigator.clipboard.writeText($('dText').value).then(()=>toast('Copied'));};
$('dClear').onclick=()=>$('dText').value='';
$('rGo').onclick=()=>{const v=pickVoice($('rLang').value);$('rNote').textContent=v?'Voice: '+v.name:'No '+L($('rLang').value)[0]+' voice on this device.';
 if(v){speechSynthesis.cancel();const u=new SpeechSynthesisUtterance($('rText').value);u.voice=v;u.lang=$('rLang').value;u.rate=+$('rRate').value;speechSynthesis.speak(u);}};
$('rStop').onclick=()=>window.speechSynthesis&&speechSynthesis.cancel();
$('tGo').onclick=async()=>{const x=$('tText').value.trim();if(!x)return;store.set('tLang',$('tLang').value);$('tOut').textContent='\u2026';
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
 try{const j=await getJ('/state?c='+T.code+'&v='+T.ver+'&tl='+enc(tl));
  if(j.ok&&j.same){}
  else if(j.ok){if(j.reset)T.msgs.clear();T.people=j.people;
   for(const m of j.msgs){if(!T.loaded)T.spoken.add(m.id);if(m.t)m['t_'+tl]=m.t;T.msgs.set(m.id,Object.assign(T.msgs.get(m.id)||{},m));}
   T.ver=j.v;if(j.more)again=0;else T.loaded=true;tvRender();}
  else $('caps').innerHTML='<div class="cap"><div class="t">No room '+esc(T.code)+' on this board.</div></div>';}catch(e){again=2500;}
 T.timer=setTimeout(tvPoll,again);}
function tvText(m){const tl=tlOf($('tvLang').value);if(!needsTr(m,tl))return m.x;const k='t_'+tl;if(m[k])return m[k];
 if(!m['w_'+tl]){m['w_'+tl]=1;translate(m.x,tl).then(t=>{if(t){m[k]=t;api('/tr',{c:T.code,id:m.id,tl,x:t}).catch(()=>{});tvRender();}else m['w_'+tl]=0;});}return null;}
function tvRender(){const ids=[...T.msgs.keys()].sort((a,b)=>a-b).slice(-4);
 $('caps').innerHTML=ids.map((id,i)=>{const m=T.msgs.get(id),t=tvText(m);
  return '<div class="cap'+(i<ids.length-1?' old':'')+'"><div class="n" style="color:'+(m.w>=0?COLORS[m.w%10]:'')+'">'+esc(m.n)+'</div><div class="t">'+esc(t||m.x)+'</div></div>';}).join('');
 $('tvLive').textContent=T.people.filter(p=>p.live).map(p=>p.n+': '+p.live+'\u2026').join('   ');
 for(const id of ids){if(T.spoken.has(id))continue;const t=tvText(T.msgs.get(id));if(t==null)continue;T.spoken.add(id);if(T.voice)speak(t,$('tvLang').value);}}

document.addEventListener('visibilitychange',()=>{if(document.hidden)return;
 if(R.code&&!$('room').hidden)poll();else if(P.code)passPoll();else if(T.code)tvPoll();});

// ── start ────────────────────────────────────────────────────────────────────
(function(){const h=new URLSearchParams(location.hash.slice(1));
 if(h.get('tv'))return tvStart(h.get('tv'));
 const c=(h.get('c')||'').toUpperCase(),seat=store.get('seat',{});
 if(c&&seat[c]){api('/join',{c,t:seat[c],n:$('name').value||'Guest',l:$('lang').value}).then(j=>{if(j.ok)enterRoom(j.c,j.t);else{show('home');listRooms();}});return;}
 if(c)$('code').value=c;show('home');listRooms();})();
</script></body></html>)HTML";
