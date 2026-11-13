// A remote for the whole computer — one board, nothing attached.
//
// The board does two things at once, on the one antenna it has:
//
//   - to your LAPTOP it is a Bluetooth keyboard. You pair it once, the way you would pair any
//     keyboard, and from then on it can press keys — arrows to move slides, the media keys to
//     play and pause and change the volume, and whole strings of text.
//
//   - to your PHONE it is a WiFi network serving a web page. That page is the remote's face:
//     the buttons you tap. Nothing to install — join the board's network and the page opens.
//
// So a tap on your phone becomes a keystroke on your laptop, routed through a chip bridging
// its two radios. The phone never talks to the laptop; only the board does, and only as a
// keyboard the laptop already trusts.
//
// This is the first sketch in the book whose output is not the page it serves. The page is
// the control surface; the result happens on another screen entirely. It is also the only one
// that runs WiFi and Bluetooth together — the core has software coexistence compiled in
// (CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y), which time-slices the radio between the two. Expect
// the page to feel a touch less instant than the others because of it.
//
// Board: ESP32 DevKit V1, nothing attached. Pair "devkit-remote" on the laptop over
// Bluetooth; join "devkit-remote" on the phone over WiFi, then open http://192.168.4.1
//
// No external libraries: BLEHIDDevice and the ASCII keymap below both ship with the ESP32
// Arduino core.

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLEUtils.h>
#include <BLESecurity.h>

// The core bundles an ASCII -> HID-usage table so we can type arbitrary text. It is guarded
// by US_KEYBOARD and is otherwise never compiled in. keymap[c] gives {usage, modifier}.
#define US_KEYBOARD 1
#include "HIDKeyboardTypes.h"

#include "ui.h"

const char *AP_NAME = "devkit-remote";   // the WiFi the phone joins
const char *AP_PASS = "";                // open; set 8+ characters to lock the control page
const char *BT_NAME = "devkit-remote";   // the name the laptop sees when pairing
const int   MAX_STATIONS = 4;            // phones on the control page at once; one is usual

// Capacitive touch, so the board also works as a bare clicker with no phone at all: hold it
// like a remote and your thumb finds the two header pins on the edge. GPIO 32 and 33 sit next
// to each other on the DevKit V1 and are both safe to use (not strapping pins, not wired to
// flash). This is the SECONDARY input and the less reliable one — see calibrate() below.
const int TOUCH_NEXT = T9;   // GPIO 32
const int TOUCH_PREV = T8;   // GPIO 33
const bool TOUCH_ENABLED = true;

// ── HID usages ───────────────────────────────────────────────────────────────
// Keyboard page (0x07) usage IDs. These are key positions, not characters.
enum {
  K_B = 0x05, K_F = 0x09, K_T = 0x17, K_W = 0x1A,
  K_ENTER = 0x28, K_ESC = 0x29, K_TAB = 0x2B,
  K_F5 = 0x3E, K_F11 = 0x44,
  K_RIGHT = 0x4F, K_LEFT = 0x50,
};
// Modifier bits (from HIDKeyboardTypes.h): CTRL 0x01, SHIFT 0x02, ALT 0x04, GUI 0x08.
// GUI is Command on a Mac and the Windows key on a PC.

// Consumer page (0x0C) usage IDs — the media keys. These work system-wide on every OS,
// whatever application is focused, which the keyboard shortcuts above do not.
enum {
  CC_PLAYPAUSE = 0x00CD, CC_NEXT = 0x00B5, CC_PREV = 0x00B6,
  CC_MUTE = 0x00E2, CC_VOLUP = 0x00E9, CC_VOLDOWN = 0x00EA,
};

// The report map: a keyboard (report ID 1) and a consumer control (report ID 2) in one
// device. Standard boilerplate — the only choices here are the two report IDs, which must
// match the inputReport() calls in setup().
static const uint8_t HID_REPORT_MAP[] = {
  // Keyboard, report ID 1
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x01,        //   Report ID (1)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,        //   Usage Minimum (Left Control)
  0x29, 0xE7,        //   Usage Maximum (Right GUI)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data, Variable, Absolute) ; the modifier byte
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Constant)                 ; one reserved byte
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,        //   Usage Minimum (0)
  0x29, 0x65,        //   Usage Maximum (101)
  0x81, 0x00,        //   Input (Data, Array)              ; six key slots
  0xC0,              // End Collection

  // Consumer control, report ID 2
  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x02,        //   Report ID (2)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
  0x19, 0x00,        //   Usage Minimum (0)
  0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x00,        //   Input (Data, Array)              ; one 16-bit usage
  0xC0,              // End Collection
};

// ── globals ──────────────────────────────────────────────────────────────────
WebServer web(80);
DNSServer dns;

BLEHIDDevice *hid = nullptr;
BLECharacteristic *kbIn = nullptr;   // report ID 1
BLECharacteristic *ccIn = nullptr;   // report ID 2
volatile bool linked = false;        // a BLE host is connected
uint32_t started = 0;

// Touch baselines, learned at boot. A touch on the classic ESP32 reads LOWER than rest, so a
// press is a reading that has fallen below a fraction of the baseline.
int baseNext = 0, basePrev = 0;

// ── sending keys ─────────────────────────────────────────────────────────────
// A HID report carries no report ID in its payload here: inputReport(id) installs a Report
// Reference descriptor that tells the host the ID, so the value is just the data. (The core's
// own gamepad example prepends the ID into the payload; that is the sloppy path and it shifts
// every field by a byte. A keyboard host will not forgive that, so we follow the spec.)

static void tapKey(uint8_t modifier, uint8_t usage) {
  if (!linked) return;
  uint8_t press[8] = { modifier, 0, usage, 0, 0, 0, 0, 0 };
  kbIn->setValue(press, 8);
  kbIn->notify();
  delay(10);
  uint8_t release[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  kbIn->setValue(release, 8);
  kbIn->notify();
  delay(10);
}

static void tapConsumer(uint16_t usage) {
  if (!linked) return;
  uint8_t press[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
  ccIn->setValue(press, 2);
  ccIn->notify();
  delay(10);
  uint8_t release[2] = { 0, 0 };
  ccIn->setValue(release, 2);
  ccIn->notify();
  delay(10);
}

// Type a string one key at a time, using the bundled ASCII table. Characters outside it — and
// anything the US layout cannot reach without a dead key — are skipped rather than mistyped.
static void typeText(const String &s) {
  for (unsigned i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (c >= KEYMAP_SIZE) continue;
    KEYMAP k = keymap[c];
    if (k.usage == 0) continue;
    tapKey(k.modifier, k.usage);
  }
}

// GUI on a Mac, Control everywhere else — the one difference that matters for the browser
// shortcuts. The page sends which OS it is set to.
static uint8_t cmdKey(bool mac) { return mac ? 0x08 /*GUI*/ : 0x01 /*CTRL*/; }

// ── BLE callbacks ────────────────────────────────────────────────────────────
class LinkCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { linked = true; }
  void onDisconnect(BLEServer *) override {
    linked = false;
    BLEDevice::startAdvertising();   // so the laptop can find us again
  }
};

// "Just Works" pairing with bonding, exactly like a shop-bought keyboard: the laptop pairs it
// without asking for a PIN, and remembers it.
class JustWorks : public BLESecurityCallbacks {
  bool onSecurityRequest() override { return true; }
  bool onConfirmPIN(uint32_t) override { return true; }
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}
#endif
};

static void startBLE() {
  BLEDevice::init(BT_NAME);

  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);            // no screen, no keypad — just works
  security->setAuthenticationMode(true, false, true);  // bond, no MITM, secure connection

  BLEDevice::setSecurityCallbacks(new JustWorks());

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new LinkCallbacks());

  hid = new BLEHIDDevice(server);
  hid->manufacturer()->setValue("devkit-diary");
  hid->pnp(0x02, 0xE502, 0xA111, 0x0100);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t *)HID_REPORT_MAP, sizeof(HID_REPORT_MAP));

  kbIn = hid->inputReport(1);   // report ID 1 — keyboard
  ccIn = hid->inputReport(2);   // report ID 2 — consumer control
  hid->setBatteryLevel(100);
  hid->startServices();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);   // HID Keyboard, so the OS files it under keyboards
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// ── touch (the no-phone clicker) ─────────────────────────────────────────────
static void calibrate() {
  if (!TOUCH_ENABLED) return;
  // Average a handful of untouched reads. Do this with nothing near the pins — the baseline
  // is whatever "not touched" happens to be on this board, in this room, at this temperature,
  // and it drifts with all three. That drift is exactly why a bought remote has real buttons.
  long n = 0, p = 0;
  for (int i = 0; i < 16; i++) { n += touchRead(TOUCH_NEXT); p += touchRead(TOUCH_PREV); delay(8); }
  baseNext = n / 16;
  basePrev = p / 16;
}

static void pollTouch() {
  if (!TOUCH_ENABLED || !linked) return;
  static uint32_t lock = 0;
  if (millis() < lock) return;                 // debounce: one press per 320 ms
  // A press is a reading fallen well below baseline. 60% is a wide margin that tolerates some
  // drift without firing on noise; too tight and a warm board stops responding.
  if (touchRead(TOUCH_NEXT) < baseNext * 0.6) { tapKey(0, K_RIGHT); lock = millis() + 320; }
  else if (touchRead(TOUCH_PREV) < basePrev * 0.6) { tapKey(0, K_LEFT); lock = millis() + 320; }
}

// ── endpoints ────────────────────────────────────────────────────────────────
// Named actions, so the firmware owns the mapping and the page only sends intent. Anything
// needing the OS-specific command key reads ?os=mac.
static void handleKey() {
  if (!linked) { web.send(200, "application/json", "{\"ok\":false,\"err\":\"noble\"}"); return; }
  const String k = web.arg("k");
  const bool mac = web.arg("os") == "mac";

  if      (k == "next")      tapKey(0, K_RIGHT);
  else if (k == "prev")      tapKey(0, K_LEFT);
  else if (k == "black")     tapKey(0, K_B);          // B blanks a slideshow
  else if (k == "start")     tapKey(0, K_F5);         // F5 starts one
  else if (k == "esc")       tapKey(0, K_ESC);
  else if (k == "playpause") tapConsumer(CC_PLAYPAUSE);
  else if (k == "tracknext") tapConsumer(CC_NEXT);
  else if (k == "trackprev") tapConsumer(CC_PREV);
  else if (k == "volup")     tapConsumer(CC_VOLUP);
  else if (k == "voldown")   tapConsumer(CC_VOLDOWN);
  else if (k == "mute")      tapConsumer(CC_MUTE);
  else if (k == "newtab")    tapKey(cmdKey(mac), K_T);
  else if (k == "closetab")  tapKey(cmdKey(mac), K_W);
  else if (k == "fullscreen") {
    if (mac) tapKey(0x01 | 0x08, K_F);   // Ctrl+Cmd+F on a Mac
    else     tapKey(0, K_F11);           // F11 on a PC
  } else {
    web.send(200, "application/json", "{\"ok\":false,\"err\":\"what\"}");
    return;
  }
  web.send(200, "application/json", "{\"ok\":true}");
}

// Whatever you type on the phone, typed onto the laptop.
static void handleType() {
  if (!linked) { web.send(200, "application/json", "{\"ok\":false,\"err\":\"noble\"}"); return; }
  typeText(web.arg("x"));
  web.send(200, "application/json", "{\"ok\":true}");
}

// Open a site: a fresh tab, then the address, then Enter. This assumes a browser window is the
// focused app — there is no HID key for "launch Chrome", so the macro leans on the browser
// already being in front. New-tab puts the cursor in the address bar on every major browser;
// the pause gives it time to land before the typing starts.
static void handleOpen() {
  if (!linked) { web.send(200, "application/json", "{\"ok\":false,\"err\":\"noble\"}"); return; }
  String url = web.arg("u");
  url.trim();
  if (!url.length()) { web.send(200, "application/json", "{\"ok\":false,\"err\":\"empty\"}"); return; }
  const bool mac = web.arg("os") == "mac";
  tapKey(cmdKey(mac), K_T);   // new tab -> focus the address bar
  delay(300);
  typeText(url);
  tapKey(0, K_ENTER);
  web.send(200, "application/json", "{\"ok\":true}");
}

static void handleState() {
  String o = "{\"ble\":" + String(linked ? "true" : "false")
           + ",\"stations\":" + String(WiFi.softAPgetStationNum())
           + ",\"heap\":" + String(ESP.getFreeHeap())
           + ",\"up\":" + String((millis() - started) / 1000) + "}";
  web.send(200, "application/json", o);
}

static const char BODY[] PROGMEM = R"HTML(
<style>
.state{display:flex;align-items:center;gap:9px;margin:0 2px 14px;font-size:13px;color:var(--mute)}
.grp{margin-bottom:14px}
.grp .label{margin-left:2px}
.pad{display:grid;gap:10px}
.pad.p3{grid-template-columns:repeat(3,1fr)}
.pad.p2{grid-template-columns:repeat(2,1fr)}
.pad button{padding:18px 8px;font-size:15px;border-radius:16px;line-height:1.15}
.pad button .k{display:block;font-size:11px;font-weight:700;color:rgba(20,22,29,.62);margin-top:3px}
.pad button.ghost .k{color:var(--faint)}
.big-nav{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.big-nav button{padding:26px 8px;font-size:17px}
.seg{display:inline-flex;background:var(--card2);border:1px solid var(--line);border-radius:12px;padding:3px;gap:3px}
.seg button{width:auto;padding:7px 16px;font-size:13px;border-radius:9px;background:transparent;color:var(--mute);box-shadow:none}
.seg button.on{background:linear-gradient(135deg,var(--amber),var(--pink));color:#14161d}
.quick{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
.quick button{width:auto;padding:9px 14px;font-size:13px;border-radius:999px;background:var(--card2);color:var(--ink);border:1px solid var(--line);box-shadow:none}
.hint{color:var(--faint);font-size:11.5px;margin:9px 2px 0;line-height:1.55}
#toast{position:fixed;left:50%;bottom:calc(20px + env(safe-area-inset-bottom));transform:translateX(-50%) translateY(14px);
  background:var(--card2);border:1px solid var(--line);color:var(--ink);padding:10px 16px;border-radius:999px;
  font-size:13px;font-weight:650;opacity:0;pointer-events:none;transition:.25s;box-shadow:var(--shadow);z-index:9}
#toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
</style>

<header><span class="dot"></span><div>
  <h1>Remote</h1><p class="sub">your phone, driving the laptop it is paired to</p>
</div></header>

<div class="state">
  <span class="pill" id="link"><span class="live"></span><span id="linktext">checking…</span></span>
  <span class="seg" style="margin-left:auto" id="os">
    <button data-os="pc" class="on">PC</button><button data-os="mac">Mac</button>
  </span>
</div>

<div class="card">
  <p class="label">slides</p>
  <div class="big-nav">
    <button class="ghost" data-k="prev">◀ Back</button>
    <button data-k="next">Next ▶</button>
  </div>
  <div class="pad p3" style="margin-top:10px">
    <button class="ghost" data-k="start">Start<span class="k">F5</span></button>
    <button class="ghost" data-k="black">Black<span class="k">B</span></button>
    <button class="ghost" data-k="esc">Exit<span class="k">Esc</span></button>
  </div>
</div>

<div class="card">
  <p class="label">media — works in anything, even unfocused</p>
  <div class="pad p3">
    <button class="ghost" data-k="trackprev">⏮</button>
    <button class="mint" data-k="playpause">⏯ Play</button>
    <button class="ghost" data-k="tracknext">⏭</button>
  </div>
  <div class="pad p3" style="margin-top:10px">
    <button class="ghost" data-k="voldown">Vol −</button>
    <button class="ghost" data-k="mute">Mute</button>
    <button class="ghost" data-k="volup">Vol +</button>
  </div>
</div>

<div class="card">
  <p class="label">open a site in the browser</p>
  <form id="openf" autocomplete="off">
    <div class="row">
      <input id="url" placeholder="youtube.com" autocomplete="off" autocapitalize="off" spellcheck="false">
      <button class="narrow" id="go">Go</button>
    </div>
  </form>
  <div class="quick" id="quick">
    <button data-u="youtube.com">YouTube</button>
    <button data-u="github.com/geekksv">GitHub</button>
    <button data-u="geekksv.pages.dev">My site</button>
    <button data-u="google.com">Google</button>
  </div>
  <p class="hint">Opens a new tab and types the address. It needs a browser window in front —
    there is no key for “launch the browser”, so this leans on it already being open.</p>
  <div class="pad p3" style="margin-top:12px">
    <button class="ghost" data-k="newtab">New tab</button>
    <button class="ghost" data-k="closetab">Close tab</button>
    <button class="ghost" data-k="fullscreen">Fullscreen</button>
  </div>
</div>

<div class="card">
  <p class="label">type on the laptop</p>
  <form id="typef" autocomplete="off">
    <input id="txt" placeholder="type here, it appears over there" autocomplete="off">
    <div class="row" style="margin-top:10px">
      <button id="send">Type it</button>
    </div>
  </form>
  <p class="hint">Your phone becomes the laptop’s keyboard. Handy when the laptop is across the
    room, or its own keyboard is awkward to reach.</p>
</div>

<p class="foot">one ESP32, nothing attached · paired over Bluetooth, controlled over WiFi</p>
<div id="toast"></div>

<script>
const $=i=>document.getElementById(i);
let os='pc', connected=false;
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(r=>r.json());

let toastT=null;
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');
  clearTimeout(toastT);toastT=setTimeout(()=>t.classList.remove('show'),1400);}

function setLink(on){
  connected=on;
  const p=$('link');
  p.className='pill '+(on?'on':'off');
  $('linktext').textContent=on?'laptop connected':'no laptop paired';
}

async function key(k){
  if(!connected){toast('Pair a laptop first');return;}
  try{const d=await post('/key','k='+encodeURIComponent(k)+'&os='+os);
    if(!d.ok&&d.err==='noble'){setLink(false);toast('Laptop dropped off');}}
  catch(e){toast('The board did not answer');}
}

// One delegated handler for every button carrying data-k.
document.querySelectorAll('[data-k]').forEach(b=>b.addEventListener('click',()=>key(b.dataset.k)));

// OS switch — flips the command key the browser shortcuts use.
$('os').addEventListener('click',e=>{
  const b=e.target.closest('[data-os]'); if(!b)return;
  os=b.dataset.os;
  [...$('os').children].forEach(c=>c.classList.toggle('on',c===b));
});

async function open(u){
  if(!u.trim())return;
  if(!connected){toast('Pair a laptop first');return;}
  try{const d=await post('/open','u='+encodeURIComponent(u)+'&os='+os);
    if(d.ok)toast('Opening '+u); else if(d.err==='noble'){setLink(false);toast('Laptop dropped off');}}
  catch(e){toast('The board did not answer');}
}
$('openf').onsubmit=e=>{e.preventDefault();open($('url').value);$('url').value='';};
$('quick').addEventListener('click',e=>{const b=e.target.closest('[data-u]');if(b)open(b.dataset.u);});

$('typef').onsubmit=async e=>{
  e.preventDefault();
  const v=$('txt').value; if(!v)return;
  if(!connected){toast('Pair a laptop first');return;}
  $('txt').value='';
  try{const d=await post('/type','x='+encodeURIComponent(v));
    if(d.ok)toast('Typed'); else if(d.err==='noble'){setLink(false);toast('Laptop dropped off');}}
  catch(e){toast('The board did not answer');$('txt').value=v;}
};

async function state(){
  try{const d=await (await fetch('/state')).json();setLink(d.ble);}catch(e){}
  setTimeout(state,1500);
}
state();
</script>
)HTML";

static void handlePage() { uiSend(web, "Remote", BODY); }

void setup() {
  Serial.begin(115200);
  delay(300);
  started = millis();

  // Bluetooth first, so its controller claims its memory before WiFi starts sharing the radio.
  startBLE();
  calibrate();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, strlen(AP_PASS) ? AP_PASS : NULL, 1, 0, MAX_STATIONS);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  web.on("/", handlePage);
  web.on("/state", handleState);
  web.on("/key", HTTP_POST, handleKey);
  web.on("/type", HTTP_POST, handleType);
  web.on("/open", HTTP_POST, handleOpen);
  web.onNotFound(handlePage);   // answer every captive-portal probe with the remote
  web.begin();

  Serial.printf("\npair \"%s\" on the laptop (Bluetooth)\njoin \"%s\" on the phone, open http://%s\nfree heap: %u bytes\n",
                BT_NAME, AP_NAME, ip.toString().c_str(), ESP.getFreeHeap());
}

void loop() {
  dns.processNextRequest();
  web.handleClient();
  pollTouch();
}
