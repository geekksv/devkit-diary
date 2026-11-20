#pragma once
//
// One look, shared by every page these sketches serve.
//
// It has to sit beside the .ino, because the Arduino IDE only compiles what is in the sketch
// folder. So every chapter that serves a page carries its own copy of this file, and they are
// kept byte-identical rather than drifting.
//
// The pages are built to be opened on a phone: large touch targets, safe-area padding for
// notches, and no layout that needs a mouse.

#include <WebServer.h>

static const char UI_CSS[] PROGMEM = R"CSS(
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
:root{
  --bg:#0e1016; --card:#161a24; --card2:#1b2130; --line:#252b3a;
  --ink:#eef1f7; --mute:#99a2b6; --faint:#5e6679;
  --amber:#ffb86c; --mint:#7ee7c7; --violet:#a78bfa; --pink:#ff8fab; --red:#ff6b7a;
  --r:20px; --shadow:0 10px 30px rgba(0,0,0,.35);
  color-scheme:dark;
}
html,body{margin:0;padding:0}
body{
  min-height:100dvh;background:var(--bg);color:var(--ink);
  font:15px/1.6 ui-rounded,-apple-system,"Segoe UI Variable Display","Segoe UI",Inter,system-ui,sans-serif;
  background-image:
    radial-gradient(900px 500px at 12% -8%, rgba(255,184,108,.10), transparent 60%),
    radial-gradient(800px 500px at 92% 4%, rgba(167,139,250,.10), transparent 60%);
  padding:max(18px,env(safe-area-inset-top)) 18px calc(28px + env(safe-area-inset-bottom));
}
.wrap{max-width:720px;margin:0 auto}

header{display:flex;align-items:center;gap:12px;margin:6px 0 22px}
.dot{width:12px;height:12px;border-radius:50%;background:linear-gradient(135deg,var(--amber),var(--pink));
  box-shadow:0 0 0 4px rgba(255,184,108,.12);flex:none}
h1{font-size:19px;font-weight:700;letter-spacing:-.02em;margin:0}
.sub{color:var(--faint);font-size:12.5px;margin:2px 0 0}

.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
  padding:18px;margin-bottom:14px;box-shadow:var(--shadow)}
.card.tight{padding:14px 16px}
.grid{display:grid;gap:14px}
.g2{grid-template-columns:repeat(auto-fit,minmax(150px,1fr))}
.g3{grid-template-columns:repeat(auto-fit,minmax(104px,1fr))}

.label{color:var(--mute);font-size:11px;letter-spacing:.09em;text-transform:uppercase;margin:0 0 8px}
.big{font-size:32px;font-weight:800;letter-spacing:-.03em;line-height:1.1;
  font-variant-numeric:tabular-nums}
.unit{font-size:13px;color:var(--faint);font-weight:600;margin-left:3px}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  font-variant-numeric:tabular-nums}

.pill{display:inline-flex;align-items:center;gap:7px;padding:7px 14px;border-radius:999px;
  font-size:12.5px;font-weight:700;background:var(--card2);border:1px solid var(--line)}
.pill.on{background:rgba(126,231,199,.14);border-color:rgba(126,231,199,.3);color:var(--mint)}
.pill.off{background:rgba(255,107,122,.13);border-color:rgba(255,107,122,.3);color:var(--red)}
.pill.warn{background:rgba(255,184,108,.14);border-color:rgba(255,184,108,.32);color:var(--amber)}
.live{width:7px;height:7px;border-radius:50%;background:currentColor;animation:pulse 1.8s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.35;transform:scale(.8)}}

.bar{height:9px;border-radius:999px;background:var(--card2);overflow:hidden}
.bar>i{display:block;height:100%;border-radius:999px;
  background:linear-gradient(90deg,var(--amber),var(--pink));transition:width .45s cubic-bezier(.2,.8,.2,1)}
.bar.mint>i{background:linear-gradient(90deg,var(--mint),#5ec8f0)}

table{width:100%;border-collapse:collapse;font-size:13.5px}
th{text-align:left;color:var(--mute);font-size:10.5px;letter-spacing:.09em;text-transform:uppercase;
  font-weight:700;padding:0 10px 10px 0;border-bottom:1px solid var(--line)}
td{padding:11px 10px 11px 0;border-bottom:1px solid rgba(255,255,255,.035);vertical-align:middle}
tr:last-child td{border-bottom:0}

input,button,select,textarea{font:inherit;border-radius:14px;border:1px solid var(--line);
  background:var(--card2);color:var(--ink);padding:13px 15px;outline:0;width:100%}
input:focus,textarea:focus,select:focus{border-color:rgba(255,184,108,.55);
  box-shadow:0 0 0 4px rgba(255,184,108,.10)}
button{cursor:pointer;font-weight:750;border:0;color:#14161d;
  background:linear-gradient(135deg,var(--amber),var(--pink));transition:transform .12s,filter .2s}
button:active{transform:scale(.97)}
button:disabled{cursor:default;background:var(--card2);color:var(--faint);border:1px solid var(--line);box-shadow:none}
button.ghost{background:var(--card2);color:var(--ink);border:1px solid var(--line)}
button.mint{background:linear-gradient(135deg,var(--mint),#5ec8f0)}
.row{display:flex;gap:10px}.row>*{flex:1}.row>.narrow{flex:0 0 118px}

.empty{text-align:center;color:var(--faint);padding:30px 14px;font-size:13.5px}
.foot{text-align:center;color:var(--faint);font-size:11.5px;margin-top:20px;line-height:1.7}
a{color:var(--amber)}
.fade{animation:fade .35s ease both}
@keyframes fade{from{opacity:0;transform:translateY(7px)}to{opacity:1;transform:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
)CSS";

// Streams a page rather than building one String: a 6 KB page assembled in RAM on a board
// with 520 KB total is avoidable, and fragmentation here shows up as a reboot an hour later.
inline void uiSend(WebServer &web, const char *title, const char *bodyHtml) {
  web.setContentLength(CONTENT_LENGTH_UNKNOWN);
  web.send(200, "text/html; charset=utf-8", "");
  web.sendContent(F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
                    "<meta name=\"theme-color\" content=\"#0e1016\"><title>"));
  web.sendContent(title);
  web.sendContent(F("</title><style>"));
  web.sendContent_P(UI_CSS);
  web.sendContent(F("</style></head><body><div class=\"wrap\">"));
  web.sendContent_P(bodyHtml);
  web.sendContent(F("</div></body></html>"));
  web.sendContent("");
}

// Escapes text going into JSON. A stray quote or control character silently breaks the
// whole response, and the page just stops updating with nothing in the log.
inline String uiJson(const char *s) {
  String o;
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      default:   if ((uint8_t)*p < 0x20) o += ' '; else o += *p;
    }
  }
  return o;
}

// Escapes text going into HTML, for anything a person typed.
inline String uiHtml(const char *s) {
  String o;
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;";  break;
      case '>': o += "&gt;";  break;
      case '"': o += "&quot;"; break;
      default:  o += *p;
    }
  }
  return o;
}
