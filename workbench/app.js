// ESP32 Flasher — put a devkit-diary project on an ESP32 from the browser.
//
// The firmware is compiled by GitHub Actions and published next to this page
// (firmware/<device>/<project>.bin, one merged image written at 0x0). This page only does what
// a browser can do over USB: identify the chip, write the image, and read the serial line.
// Writing is Espressif's esptool-js; the port is the browser's Web Serial API.

import { ESPLoader, Transport } from "https://cdn.jsdelivr.net/npm/esptool-js@0.7.0/bundle.js";

const $ = (id) => document.getElementById(id);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const state = {
  meta: null,         // projects.json
  manifest: null,     // firmware/manifest.json
  port: null,         // the SerialPort the user picked
  chip: null,         // { name, desc, mac, flash } once detected
  busy: false,        // connecting or flashing — the port belongs to esptool
  mon: null,          // { reader, closed } while the monitor is open
};

// USB vendor ids of the adapters ESP32 boards ship with.
const ADAPTERS = { 0x10c4: "Silicon Labs CP210x", 0x1a86: "WCH CH340/CH9102", 0x0403: "FTDI", 0x303a: "Espressif USB" };

// ── the log ──────────────────────────────────────────────────────────────────

const log = $("log");
const MAX_LINES = 3000;
let partial = null;          // the line still being written to, so text arriving in pieces joins up

function stamp() {
  const d = new Date();
  return d.toTimeString().slice(0, 8) + "." + String(d.getMilliseconds()).padStart(3, "0");
}

function newLine(kind) {
  const line = document.createElement("div");
  if (kind) line.className = kind;
  if ($("stamps").checked) {
    const t = document.createElement("span");
    t.className = "t";
    t.textContent = stamp() + "  ";
    line.appendChild(t);
  }
  line.appendChild(document.createTextNode(""));
  log.appendChild(line);
  while (log.childElementCount > MAX_LINES) log.firstElementChild.remove();
  return line;
}

// Text from the board: may end mid-line; carriage returns are dropped.
function monitorText(text) {
  const parts = text.replace(/\r/g, "").split("\n");
  parts.forEach((piece, i) => {
    if (!partial) partial = newLine();
    if (piece) partial.lastChild.textContent += piece;
    if (i < parts.length - 1) partial = null;
  });
  scroll();
}

// A line from this page or from esptool, set apart from the board's own output.
function sysLine(text, kind = "sys") {
  partial = null;
  newLine(kind).lastChild.textContent = text;
  scroll();
}

function scroll() { if ($("autoscroll").checked) log.scrollTop = log.scrollHeight; }

const terminal = {   // esptool-js writes its progress messages here
  clean() {},
  writeLine(data) { sysLine("esptool  " + data); },
  write(data) { sysLine("esptool  " + data); },
};

// ── projects and devices ─────────────────────────────────────────────────────

async function loadData() {
  const bust = "?t=" + Date.now();
  state.meta = await (await fetch("projects.json" + bust)).json();
  try {
    state.manifest = await (await fetch("firmware/manifest.json" + bust)).json();
  } catch (e) {
    state.manifest = null;
  }
  const dev = $("device");
  for (const d of state.meta.devices) {
    const o = document.createElement("option");
    o.value = d.id;
    o.textContent = d.name;
    dev.appendChild(o);
  }
  if (state.manifest) {
    const when = new Date(state.manifest.built);
    $("built").textContent = "Firmware built " + when.toLocaleString() + " from commit " +
      state.manifest.commit + " · " + state.manifest.core;
  } else {
    $("built").textContent = "No firmware has been published yet.";
  }
  fillProjects();
}

const device = () => state.meta.devices.find((d) => d.id === $("device").value);
const project = () => state.meta.projects.find((p) => p.id === $("project").value);
const firmware = (devId, projId) => state.manifest?.firmware?.[devId]?.[projId];

function fillProjects() {
  const sel = $("project");
  const keep = sel.value;
  sel.innerHTML = "";
  for (const p of state.meta.projects) {
    const fw = firmware(device().id, p.id);
    const o = document.createElement("option");
    o.value = p.id;
    o.textContent = p.title + (fw?.ok ? "" : " — not available for this device");
    o.disabled = !fw?.ok;
    sel.appendChild(o);
  }
  const first = [...sel.options].find((o) => !o.disabled);
  sel.value = keep && !sel.querySelector(`option[value="${keep}"]`)?.disabled ? keep : first?.value || "";
  showProject();
}

function showProject() {
  const p = project();
  const about = $("about");
  about.innerHTML = "";
  $("next").hidden = true;
  if (!p) { refresh(); return; }
  const fw = firmware(device().id, p.id);
  const h = document.createElement("h2");
  h.textContent = p.title;
  const d = document.createElement("p");
  d.style.margin = "0";
  d.textContent = p.description;
  const meta = document.createElement("div");
  meta.className = "meta";
  const pill = (text, cls) => { const s = document.createElement("span"); s.className = "pill " + (cls || ""); s.textContent = text; meta.appendChild(s); };
  pill(p.wifi_setup ? "joins your WiFi" : "WiFi " + p.wifi, "mint");
  if (fw?.ok) pill((fw.size / 1024).toFixed(0) + " KB");
  if (p.partition !== "default") pill("partition: " + p.partition.replace("_", " "));
  if (!device().tested) pill("untested on " + device().name, "warn");
  const link = document.createElement("a");
  link.href = "https://github.com/geekksv/devkit-diary/tree/main/" + p.chapter;
  link.target = "_blank";
  link.rel = "noopener";
  link.textContent = "read the chapter →";
  link.className = "pill";
  meta.appendChild(link);
  about.append(h, d, meta);
  showWifiCard();
  refresh();
}

// ── buttons follow the state ─────────────────────────────────────────────────

function refresh() {
  const supported = "serial" in navigator;
  $("pickPort").disabled = !supported || state.busy;
  $("detect").disabled = !state.port || state.busy;
  $("flash").disabled = !state.port || state.busy || !project() || !firmware(device().id, project().id)?.ok;
  $("monToggle").disabled = !state.port || state.busy;
  $("monToggle").textContent = state.mon ? "Close" : "Open";
  $("resetBoard").disabled = !state.port || state.busy;
  $("wifiScan").disabled = !state.port || state.busy;
  $("wifiGo").disabled = !state.port || state.busy;
  $("device").disabled = state.busy;
  $("project").disabled = state.busy;
  const ms = $("mstate");
  ms.textContent = state.mon ? "open · " + $("monBaud").value : "closed";
  ms.classList.toggle("on", !!state.mon);
}

// ── the port ─────────────────────────────────────────────────────────────────

async function pickPort() {
  try {
    const port = await navigator.serial.requestPort();
    await stopMonitor();
    state.port = port;
    state.chip = null;
    const info = port.getInfo();
    const vendor = ADAPTERS[info.usbVendorId] || (info.usbVendorId ? "USB " + info.usbVendorId.toString(16) : "serial port");
    $("portInfo").textContent = vendor + (info.usbProductId ? " · " + info.usbProductId.toString(16).padStart(4, "0") : "");
    $("chipInfo").textContent = "";
    sysLine("port selected: " + vendor);
    refresh();
    await detect();
  } catch (e) {
    if (e.name !== "NotFoundError") sysLine("could not select a port: " + e.message, "err");
  }
}

// Connects esptool, runs `job(loader)`, and always lets go of the port afterwards.
async function withLoader(job) {
  await stopMonitor();
  state.busy = true;
  refresh();
  const transport = new Transport(state.port, false);
  try {
    const loader = new ESPLoader({ transport, baudrate: Number($("upload").value), terminal });
    const desc = await loader.main();
    state.chip = {
      name: loader.chip.CHIP_NAME,
      desc,
      mac: await loader.chip.readMac(loader),
      flash: await loader.detectFlashSize().catch(() => "?"),
    };
    showChip();
    return await job(loader);
  } finally {
    try { await transport.disconnect(); } catch (e) { /* already closed */ }
    state.busy = false;
    refresh();
  }
}

function showChip() {
  const c = state.chip;
  const want = device();
  const el = $("chipInfo");
  if (!c) { el.textContent = ""; return; }
  const match = c.name === want.chip;
  el.className = "chipinfo" + (match ? "" : " warn");
  el.textContent = c.desc + " · " + c.flash + " flash · MAC " + c.mac +
    (match ? "" : "\nThis is an " + c.name + ". These projects are built for the " + want.name + " only.");
}

async function detect() {
  try {
    await withLoader(async () => {
      // Select the device the board turned out to be, if it is one we build for.
      const found = state.meta.devices.find((d) => d.chip === state.chip.name);
      if (found && found.id !== $("device").value) { $("device").value = found.id; fillProjects(); }
      showChip();
    });
    await startMonitor(true);
  } catch (e) {
    sysLine("could not talk to the board: " + e.message, "err");
    sysLine("hold BOOT on the board and press Detect again, or check the cable", "err");
  }
}

// ── flashing ─────────────────────────────────────────────────────────────────

function stage(name) {
  const order = ["connect", "erase", "write", "verify", "reset"];
  const at = order.indexOf(name);
  for (const s of $("stages").children) {
    const i = order.indexOf(s.dataset.s);
    s.classList.toggle("on", i === at);
    s.classList.toggle("done", i < at || name === "done");
  }
}

function progress(pct, msg, cls) {
  $("bar").style.width = pct + "%";
  $("bar").parentElement.className = "bar" + (cls ? " " + cls : "");
  $("pmsg").textContent = msg;
  $("pmsg").className = "pmsg" + (cls === "bad" ? " bad" : "");
}

async function flash() {
  const p = project(), d = device();
  const fw = firmware(d.id, p.id);
  const erase = $("erase").checked;
  $("progress").hidden = false;
  $("next").hidden = true;
  $("stages").querySelector('[data-s="erase"]').classList.toggle("skip", !erase);
  stage("connect");
  progress(0, "Downloading " + p.title + "…");
  try {
    const res = await fetch(fw.file + "?c=" + state.manifest.commit);
    if (!res.ok) throw new Error("firmware download failed (" + res.status + ")");
    const data = new Uint8Array(await res.arrayBuffer());
    if (data.length !== fw.size) throw new Error("firmware download was incomplete");
    sysLine("flashing " + p.id + " for " + d.name + " — " + data.length + " bytes");
    // Everything except the board's saved settings, unless the user asked for a clean slate.
    const parts = erase ? [{ data, address: 0 }] : keepSettings(data);
    progress(0, "Connecting to the board…");

    await withLoader(async (loader) => {
      if (state.chip.name !== d.chip) {
        throw new Error("this board is an " + state.chip.name + " — the projects here are built and tested " +
          "for the " + d.name + " only");
      }
      stage(erase ? "erase" : "write");
      progress(0, erase ? "Erasing the whole flash — this takes a few seconds…" : "Writing…");
      let wrote = false;
      await loader.writeFlash({
        fileArray: parts,
        flashMode: "keep", flashFreq: "keep", flashSize: "keep",
        eraseAll: erase,
        compress: true,
        calculateMD5Hash: md5hex,
        // Progress across all the parts, not per part: the bar should not restart halfway.
        reportProgress: (i, written, total) => {
          if (!wrote) { wrote = true; stage("write"); }
          const before = parts.slice(0, i).reduce((a, x) => a + x.data.length, 0);
          const all = parts.reduce((a, x) => a + x.data.length, 0);
          const pct = Math.round(((before + Math.min(written, total)) / all) * 100);
          progress(pct, "Writing… " + pct + "%");
          if (i === parts.length - 1 && written >= total) { stage("verify"); progress(100, "Verifying what was written…"); }
        },
      });
      stage("reset");
      progress(100, "Resetting the board…");
    });

    await startMonitor(true);       // the reset happens here, so the boot log is captured
    stage("done");
    progress(100, "Done — " + p.title + " is running.", "ok");
    $("next").textContent = p.after + ".";
    $("next").hidden = false;
    if (p.wifi_setup) {
      $("wifiDone").hidden = true;
      wifiStatus("Next: tell the board your WiFi. Press Scan, pick your network, type its password.");
      $("wifiCard").scrollIntoView({ behavior: "smooth", block: "nearest" });
    }
  } catch (e) {
    progress(100, "Failed: " + e.message, "bad");
    sysLine("flash failed: " + e.message, "err");
    if (/sync|connect|timed? ?out/i.test(e.message)) {
      sysLine("hold BOOT on the board, press Flash, and let go once writing starts", "err");
    }
  }
}

// ── keeping the board's saved settings ───────────────────────────────────────
// A merged image covers the whole flash from 0x0, including the NVS partition where sketches
// keep their WiFi password and settings — blank (0xFF) in the image. Written as one piece it
// would wipe them on every flash. So the image's own partition table (always at 0x8000) is read,
// and the image is written around every NVS partition, the way the Arduino IDE writes only the
// bootloader, the table and the app. "Erase flash first" still wipes everything.

const TABLE = 0x8000;

function partitions(img) {
  const out = [];
  for (let at = TABLE; at + 32 <= img.length && at < TABLE + 0xC00; at += 32) {
    if (img[at] !== 0xAA || img[at + 1] !== 0x50) break;     // the end of the table (or its MD5 row)
    const u32 = (o) => (img[o] | img[o + 1] << 8 | img[o + 2] << 16 | img[o + 3] << 24) >>> 0;
    let label = "";
    for (let i = 12; i < 28 && img[at + i]; i++) label += String.fromCharCode(img[at + i]);
    out.push({ type: img[at + 2], subtype: img[at + 3], offset: u32(at + 4), size: u32(at + 8), label });
  }
  return out;
}

function keepSettings(img) {
  const nvs = partitions(img).filter((p) => p.type === 0x01 && p.subtype === 0x02)   // data / nvs
    .sort((a, b) => a.offset - b.offset);
  if (!nvs.length) return [{ data: img, address: 0 }];
  const parts = [];
  let from = 0;
  for (const n of nvs) {
    if (n.offset > from) parts.push({ data: img.slice(from, Math.min(n.offset, img.length)), address: from });
    from = Math.max(from, n.offset + n.size);
    sysLine("keeping saved settings: " + n.label + " at 0x" + n.offset.toString(16) + ", " + (n.size / 1024) + " KB, not rewritten");
  }
  if (from < img.length) parts.push({ data: img.slice(from), address: from });
  return parts.filter((p) => p.data.length);
}

// ── the serial monitor ───────────────────────────────────────────────────────

// Pulls the board's EN pin low through RTS and lets it go, with DTR released so GPIO0 stays
// high: a normal boot. The same auto-reset circuit the Arduino IDE uses.
async function resetBoard(port) {
  await port.setSignals({ dataTerminalReady: false, requestToSend: true });
  await sleep(120);
  await port.setSignals({ dataTerminalReady: false, requestToSend: false });
}

async function startMonitor(reset) {
  if (state.mon || !state.port || state.busy) return;
  const port = state.port;
  const baud = Number($("monBaud").value);
  try {
    await port.open({ baudRate: baud });
  } catch (e) {
    sysLine("could not open the port for the monitor: " + e.message, "err");
    return;
  }
  // Raw bytes, not text: Improv packets (WiFi setup) travel on the same line as the log and
  // are taken out before the rest is decoded.
  const reader = port.readable.getReader();
  const decoder = new TextDecoder();
  const improvIn = new ImprovReader();
  let skipNewline = false;
  state.mon = { reader, closed: Promise.resolve() };
  refresh();
  sysLine("monitor open at " + baud + " baud");
  if (reset) await resetBoard(port).catch(() => {});
  state.mon.closed = (async () => {
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        if (!value) continue;
        const text = [];
        for (const b of value) {
          const r = improvIn.feed(b);
          for (const t of r.text) {
            if (skipNewline && t === 10) { skipNewline = false; continue; }
            skipNewline = false;
            text.push(t);
          }
          if (r.packet) { improvPacket(r.packet); skipNewline = true; }
        }
        if (text.length) monitorText(decoder.decode(new Uint8Array(text), { stream: true }));
      }
    } catch (e) {
      sysLine("monitor stopped: " + e.message, "err");
    } finally {
      try { reader.releaseLock(); } catch (e) { /* gone */ }
    }
  })();
}

async function stopMonitor() {
  const m = state.mon;
  if (!m) return;
  state.mon = null;
  try { await m.reader.cancel(); } catch (e) { /* already cancelled */ }
  await m.closed;
  improvFail("the monitor closed");
  try { await state.port.close(); } catch (e) { /* already closed */ }
  sysLine("monitor closed");
  refresh();
}

// ── WiFi setup over the cable: Improv Serial (improv-wifi.com) ───────────────
// Packet: "IMPROV" · version 1 · type · length · data · checksum (low byte of the sum of all
// bytes before it). The board's side is improv.h in any sketch that needs a network.

const IMPROV = { STATE: 1, ERROR: 2, RPC: 3, RESULT: 4 };
const RPC = { WIFI: 1, STATE: 2, INFO: 3, SCAN: 4 };
const HEAD = [73, 77, 80, 82, 79, 86];           // "IMPROV"

class ImprovReader {
  constructor() { this.buf = []; }
  // Takes one byte. Returns { text: bytes that are ordinary log, packet: a whole valid packet }.
  feed(b) {
    const buf = this.buf;
    buf.push(b);
    const n = buf.length;
    const giveUp = () => {
      this.buf = [];
      if (b === HEAD[0] && n > 1) { this.buf = [b]; return { text: buf.slice(0, -1) }; }
      return { text: buf };
    };
    if (n <= 6) return b === HEAD[n - 1] ? { text: [] } : giveUp();
    if (n === 7 && b !== 1) return giveUp();
    if (n < 9) return { text: [] };
    if (buf[8] > 200) return giveUp();
    if (n < 10 + buf[8]) return { text: [] };
    let sum = 0;
    for (let i = 0; i < n - 1; i++) sum = (sum + buf[i]) & 255;
    this.buf = [];
    if (sum !== buf[n - 1]) return { text: buf };
    return { text: [], packet: { type: buf[7], data: buf.slice(9, 9 + buf[8]) } };
  }
}

function improvBuild(type, data) {
  const out = [...HEAD, 1, type, data.length, ...data];
  let sum = 0;
  for (const b of out) sum = (sum + b) & 255;
  out.push(sum);
  return new Uint8Array(out);
}

// An RPC result's strings: data = command, length, then length-prefixed strings.
function improvStrings(data) {
  const out = [];
  const dec = new TextDecoder();
  for (let i = 2; i < 2 + data[1] && i < data.length;) {
    const l = data[i];
    out.push(dec.decode(new Uint8Array(data.slice(i + 1, i + 1 + l))));
    i += 1 + l;
  }
  return out;
}

let improvWait = null;     // { onPacket(p) -> true when done, reject }

function improvPacket(p) {
  if (improvWait && improvWait.onPacket(p)) improvWait = null;
}
function improvFail(why) {
  if (improvWait) { improvWait.reject(new Error(why)); improvWait = null; }
}

// Sends one RPC and resolves with whatever `onPacket` decides, or rejects after `ms`.
async function improvRpc(cmd, payload, onPacket, ms) {
  if (!state.mon) await startMonitor(false);
  if (!state.mon) throw new Error("the board's port is not open");
  const data = [cmd, payload.length, ...payload];
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { improvWait = null; reject(new Error("the board did not answer")); }, ms);
    improvWait = {
      onPacket: (p) => {
        const r = onPacket(p);
        if (r === undefined) return false;
        clearTimeout(timer);
        if (r instanceof Error) reject(r); else resolve(r);
        return true;
      },
      reject: (e) => { clearTimeout(timer); reject(e); },
    };
    const w = state.port.writable.getWriter();
    w.write(improvBuild(IMPROV.RPC, data)).finally(() => w.releaseLock());
  });
}

const improvInfo = () => improvRpc(RPC.INFO, [], (p) =>
  p.type === IMPROV.RESULT && p.data[0] === RPC.INFO ? improvStrings(p.data) : undefined, 2500);

function improvScan() {
  const nets = [];
  return improvRpc(RPC.SCAN, [], (p) => {
    if (p.type !== IMPROV.RESULT || p.data[0] !== RPC.SCAN) return undefined;
    const s = improvStrings(p.data);
    if (!s.length) return nets;                       // the empty result ends the list
    nets.push({ ssid: s[0], rssi: Number(s[1]), secure: s[2] === "YES" });
    return undefined;
  }, 20000);
}

function improvConnect(ssid, pass) {
  const enc = new TextEncoder();
  const s = [...enc.encode(ssid)], p = [...enc.encode(pass)];
  return improvRpc(RPC.WIFI, [s.length, ...s, p.length, ...p], (pk) => {
    if (pk.type === IMPROV.STATE && pk.data[0] === 3) { wifiStatus("Joining " + ssid + "…"); return undefined; }
    if (pk.type === IMPROV.ERROR && pk.data[0] === 3) return new Error("it could not join — check the name and password");
    if (pk.type === IMPROV.ERROR && pk.data[0]) return new Error("the board refused the request (" + pk.data[0] + ")");
    if (pk.type === IMPROV.RESULT && pk.data[0] === RPC.WIFI) return improvStrings(pk.data)[0] || "";
    return undefined;
  }, 45000);
}

// ── the WiFi card ────────────────────────────────────────────────────────────

function wifiStatus(msg, cls) {
  $("wifiMsg").textContent = msg;
  $("wifiMsg").className = "pmsg" + (cls ? " " + cls : "");
}

function showWifiCard() {
  const p = project();
  $("wifiCard").hidden = !(p && p.wifi_setup);
}

// Finds the board's Improv side. A board that is joining a saved network is busy for up to
// twenty seconds before it listens, so this keeps asking for a while.
async function wifiReady() {
  for (let i = 0; i < 12; i++) {
    try { const info = await improvInfo(); return info; } catch (e) { await sleep(500); }
  }
  throw new Error("the board did not answer — is the project flashed and running?");
}

async function wifiScan() {
  $("wifiScan").disabled = true;
  try {
    wifiStatus("Asking the board which networks it can see…");
    await wifiReady();
    const nets = await improvScan();
    const list = $("ssids");
    list.innerHTML = "";
    const seen = new Set();
    for (const n of nets.sort((a, b) => b.rssi - a.rssi)) {
      if (seen.has(n.ssid)) continue;
      seen.add(n.ssid);
      const o = document.createElement("option");
      o.value = n.ssid;
      o.label = (n.rssi > -60 ? "strong" : n.rssi > -72 ? "good" : "weak") + (n.secure ? "" : " · open");
      list.appendChild(o);
    }
    if (nets.length && !$("ssid").value) $("ssid").value = nets[0].ssid;
    wifiStatus(nets.length ? "Pick your network and type its password." : "No networks found — type the name.");
  } catch (e) {
    wifiStatus(e.message, "bad");
  } finally {
    $("wifiScan").disabled = false;
  }
}

async function wifiConnect() {
  const ssid = $("ssid").value.trim(), pass = $("wifiPass").value;
  if (!ssid) { wifiStatus("Type or pick a network first.", "bad"); return; }
  $("wifiGo").disabled = true;
  try {
    wifiStatus("Talking to the board…");
    await wifiReady();
    wifiStatus("Sending the network to the board…");
    const url = await improvConnect(ssid, pass);
    const p = project();
    wifiStatus("Connected. The board restarts into " + (p ? p.title : "the project") + " in a moment.", "ok");
    $("wifiDone").hidden = false;
    $("wifiUrl").textContent = url;
    $("wifiUrl").href = url;
    $("wifiLocal").textContent = p && p.host ? "http://" + p.host + ".local" : "";
    $("wifiPass").value = "";
  } catch (e) {
    wifiStatus("Not connected: " + e.message + ".", "bad");
  } finally {
    $("wifiGo").disabled = false;
  }
}

// ── MD5, so esptool can check the flash against the file ─────────────────────
// esptool-js asks the caller for it (WebCrypto has no MD5). RFC 1321.

function md5hex(bytes) {
  const K = new Uint32Array(64), S = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
  for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 2 ** 32) >>> 0;
  const n = bytes.length, total = ((n + 8) >>> 6) * 64 + 64;
  const buf = new Uint8Array(total);
  buf.set(bytes);
  buf[n] = 0x80;
  const bits = n * 8;
  const dv = new DataView(buf.buffer);
  dv.setUint32(total - 8, bits >>> 0, true);
  dv.setUint32(total - 4, Math.floor(bits / 2 ** 32), true);
  let a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  const M = new Uint32Array(16);
  for (let off = 0; off < total; off += 64) {
    for (let j = 0; j < 16; j++) M[j] = dv.getUint32(off + j * 4, true);
    let A = a0, B = b0, C = c0, D = d0;
    for (let i = 0; i < 64; i++) {
      let F, g;
      if (i < 16) { F = (B & C) | (~B & D); g = i; }
      else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
      else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
      else { F = C ^ (B | ~D); g = (7 * i) % 16; }
      F = (F + A + K[i] + M[g]) >>> 0;
      A = D; D = C; C = B;
      const s = S[(i >> 4) * 4 + (i % 4)];
      B = (B + ((F << s) | (F >>> (32 - s)))) >>> 0;
    }
    a0 = (a0 + A) >>> 0; b0 = (b0 + B) >>> 0; c0 = (c0 + C) >>> 0; d0 = (d0 + D) >>> 0;
  }
  const out = new DataView(new ArrayBuffer(16));
  [a0, b0, c0, d0].forEach((v, i) => out.setUint32(i * 4, v, true));
  return [...new Uint8Array(out.buffer)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// ── wiring ───────────────────────────────────────────────────────────────────

function wire() {
  if (!("serial" in navigator)) $("nosupport").hidden = false;
  $("pickPort").onclick = pickPort;
  $("detect").onclick = detect;
  $("flash").onclick = flash;
  $("device").onchange = () => { fillProjects(); showChip(); };
  $("project").onchange = showProject;
  $("monToggle").onclick = () => (state.mon ? stopMonitor() : startMonitor(false));
  $("monBaud").onchange = async () => { if (state.mon) { await stopMonitor(); await startMonitor(false); } refresh(); };
  $("resetBoard").onclick = async () => {
    if (state.mon) { sysLine("resetting the board"); await resetBoard(state.port).catch((e) => sysLine(e.message, "err")); }
    else await startMonitor(true);
  };
  $("clearLog").onclick = () => { log.innerHTML = ""; partial = null; };
  $("wifiScan").onclick = wifiScan;
  $("wifiGo").onclick = wifiConnect;
  $("wifiShow").onchange = () => { $("wifiPass").type = $("wifiShow").checked ? "text" : "password"; };
  $("copyLog").onclick = () => navigator.clipboard?.writeText(log.innerText).then(() => sysLine("log copied"));
  // A board unplugged mid-session: forget it rather than leave buttons that cannot work.
  navigator.serial?.addEventListener("disconnect", (e) => {
    if (e.target !== state.port) return;
    state.mon = null;
    state.port = null;
    state.chip = null;
    $("portInfo").textContent = "unplugged";
    $("chipInfo").textContent = "";
    sysLine("board unplugged", "err");
    refresh();
  });
}

wire();
loadData().catch((e) => sysLine("could not load the project list: " + e.message, "err"));
refresh();
export { md5hex, ImprovReader, improvBuild, improvStrings, partitions, keepSettings };   // for tests
