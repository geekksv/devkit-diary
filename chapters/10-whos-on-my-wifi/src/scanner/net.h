#pragma once
//
// What the scanner knows about the network, and the small protocols it speaks to learn it.
//
// No WiFi, no sockets, no Arduino here: only the device table, the packets (built and read as
// plain bytes), the guesses and the sums. That keeps all of it testable on a laptop
// (tools/net_test.cpp), and scanner.ino does the talking.

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "oui.h"

// ── the devices ─────────────────────────────────────────────────────────────────────────────

enum Kind : uint8_t {
  K_UNKNOWN, K_ROUTER, K_PHONE, K_COMPUTER, K_TV, K_PRINTER, K_SPEAKER, K_CONSOLE, K_CAMERA,
  K_SMART, K_STORAGE, K_TABLET, K_BOARD, K_KINDS
};
static const char *const KIND_NAMES[K_KINDS] = {"unknown", "router", "phone", "computer", "tv", "printer",
                                                "speaker", "console", "camera", "smart", "storage",
                                                "tablet", "board"};

// Where a device's name came from; a better source replaces a worse one, never the other way.
enum NameSrc : uint8_t { N_NONE, N_DNS, N_UPNP, N_NETBIOS, N_MDNS, N_BONJOUR, N_HELPER };
static const char *const NAME_SRC[] = {"", "router", "UPnP", "NetBIOS", "Bonjour", "Bonjour", "helper"};

// The ports checked when you tap "Check ports" — the doors a home device is likely to have open.
struct PortInfo { uint16_t port; const char *label; };
static const PortInfo PORTS[] = {
  {21, "FTP"}, {22, "SSH"}, {23, "Telnet"}, {53, "DNS"}, {80, "Web"}, {139, "NetBIOS"},
  {443, "Web (secure)"}, {445, "File sharing"}, {548, "Apple file sharing"}, {554, "Camera stream"},
  {631, "Printing (IPP)"}, {1883, "MQTT"}, {3389, "Remote Desktop"}, {3689, "iTunes sharing"},
  {5000, "UPnP / NAS"}, {5900, "Screen sharing"}, {7000, "AirPlay"}, {8008, "Chromecast"},
  {8009, "Chromecast"}, {8060, "Roku"}, {8080, "Web (alt)"}, {8443, "Web (alt, secure)"},
  {9100, "Printer"}, {32400, "Plex"}, {62078, "iPhone sync"},
};
static const int PORT_COUNT = sizeof(PORTS) / sizeof(PORTS[0]);
static_assert(PORT_COUNT <= 32, "ports are kept as a 32-bit mask");
inline int portIndex(uint16_t p) {
  for (int i = 0; i < PORT_COUNT; i++) if (PORTS[i].port == p) return i;
  return -1;
}
inline bool hasPort(uint32_t mask, uint16_t p) { const int i = portIndex(p); return i >= 0 && (mask >> i & 1); }

const int MAX_DEV = 96;

struct Device {
  uint8_t mac[6];
  uint32_t ip;               // a.b.c.d as (a<<24)|(b<<16)|(c<<8)|d
  char name[40];             // what the device calls itself
  char nick[28];             // what you call it (set from the phone)
  char model[40];            // from UPnP: "Samsung TV", "Archer C6"
  char title[40];            // its web page's <title>, when it has one
  uint8_t nameSrc;
  uint8_t maker;             // index into OUI_MAKERS, 0xff = not known
  uint8_t kind, kindSet;     // kindSet: chosen on the phone, so no longer guessed
  uint8_t isNew;             // joined after the first scan, not yet acknowledged
  uint8_t online;
  uint8_t misses;            // scans in a row it didn't answer
  int16_t rtt;               // last ping, ms; -1 = didn't answer ping (ARP still saw it)
  uint32_t ports, portsAt;   // open ports (mask over PORTS) and when they were checked (epoch)
  uint32_t first, last;      // epoch seconds: first seen ever, last seen
  uint32_t helperMs;         // millis() of the inbox helper's last check-in from this device; 0 = none
  uint32_t svcs;             // Bonjour services it offers (mask over SERVICES)
};

struct Net {
  Device dev[MAX_DEV];
  int n;
  uint32_t self, gateway;
  bool baselined;            // a first full scan has happened; after it, newcomers are "new"
};

inline bool macZero(const uint8_t *m) { for (int i = 0; i < 6; i++) if (m[i]) return false; return true; }
// The "locally administered" bit: set on the private, per-network addresses phones now use.
inline bool macRandom(const uint8_t *m) { return m[0] & 0x02; }

inline void macStr(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}
inline bool macParse(const char *s, uint8_t *m) {
  unsigned v[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) { if (v[i] > 255) return false; m[i] = v[i]; }
  return true;
}
inline void ipStr(uint32_t ip, char *out) {
  snprintf(out, 16, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255), (unsigned)(ip >> 8 & 255),
           (unsigned)(ip & 255));
}
inline uint32_t ipParse(const char *s) {
  unsigned a, b, c, d;
  if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return 0;
  return a << 24 | b << 16 | c << 8 | d;
}

// Who made it: binary search of the IEEE prefixes. 0xff when not in the table, or when the
// address is a private random one (its prefix means nothing).
inline uint8_t ouiMaker(const uint8_t *m) {
  if (macRandom(m)) return 0xff;
  const uint32_t key = (uint32_t)m[0] << 16 | m[1] << 8 | m[2];
  int lo = 0, hi = (int)OUI_COUNT - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint32_t k = OUI_TABLE[mid] >> 8;
    if (k == key) return OUI_TABLE[mid] & 0xff;
    if (k < key) lo = mid + 1; else hi = mid - 1;
  }
  return 0xff;
}
inline const char *makerName(uint8_t i) {
  return i < sizeof(OUI_MAKERS) / sizeof(OUI_MAKERS[0]) ? OUI_MAKERS[i] : "";
}

inline Device *byMac(Net &net, const uint8_t *m) {
  for (int i = 0; i < net.n; i++) if (!memcmp(net.dev[i].mac, m, 6)) return &net.dev[i];
  return nullptr;
}
inline Device *byIp(Net &net, uint32_t ip) {
  Device *best = nullptr;                 // an address can move between devices; prefer the one online
  for (int i = 0; i < net.n; i++)
    if (net.dev[i].ip == ip && (!best || net.dev[i].online > best->online ||
                                (net.dev[i].online == best->online && net.dev[i].last > best->last)))
      best = &net.dev[i];
  return best;
}

// Room for one more: the oldest device you haven't named, that isn't here now, makes way.
inline Device *makeRoom(Net &net) {
  if (net.n < MAX_DEV) return &net.dev[net.n++];
  int victim = -1;
  for (int i = 0; i < net.n; i++) {
    const Device &d = net.dev[i];
    if (d.online || d.nick[0]) continue;
    if (victim < 0 || d.last < net.dev[victim].last) victim = i;
  }
  if (victim < 0) return nullptr;
  memset(&net.dev[victim], 0, sizeof(Device));
  return &net.dev[victim];
}

// A device answered (ARP): note it. Returns it, and sets *fresh when it was never seen before.
inline Device *seen(Net &net, const uint8_t *mac, uint32_t ip, uint32_t now, bool *fresh = nullptr) {
  if (fresh) *fresh = false;
  if (macZero(mac) || !ip) return nullptr;
  Device *d = byMac(net, mac);
  if (!d) {
    d = makeRoom(net);
    if (!d) return nullptr;
    memset(d, 0, sizeof *d);
    memcpy(d->mac, mac, 6);
    d->maker = ouiMaker(mac);
    d->rtt = -1;
    d->first = now;
    d->isNew = net.baselined;
    if (fresh) *fresh = true;
  }
  if (d->ip != ip) {                      // it moved address: what was learned from the old one is stale
    d->ip = ip;
    d->ports = d->portsAt = 0;
    d->title[0] = 0;
  }
  d->online = 1;
  d->misses = 0;
  d->last = now;
  return d;
}

// After a sweep: everything that didn't answer this time counts a miss; three in a row and it
// has left (phones asleep often skip one).
inline int endSweep(Net &net, const uint32_t *answered, int count) {
  int online = 0;
  for (int i = 0; i < net.n; i++) {
    Device &d = net.dev[i];
    bool hit = false;
    for (int j = 0; j < count && !hit; j++) hit = answered[j] == d.ip;
    if (!hit && d.online && ++d.misses >= 3) { d.online = 0; d.rtt = -1; }
    online += d.online;
  }
  net.baselined = true;
  return online;
}

inline void setName(Device &d, const char *name, uint8_t src) {
  if (!name || !name[0] || src < d.nameSrc) return;
  strncpy(d.name, name, sizeof d.name - 1);
  d.name[sizeof d.name - 1] = 0;
  d.nameSrc = src;
}

inline int strncasecmp_(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    const int x = tolower((unsigned char)a[i]), y = tolower((unsigned char)b[i]);
    if (x != y || !x) return x - y;
  }
  return 0;
}
inline int strcasecmp_(const char *a, const char *b) { return strncasecmp_(a, b, (size_t)-1); }

// Bonjour service browsing: "which printers / TVs / speakers / computers are here?", asked once
// on the Bonjour multicast group. Devices that won't say their name when asked directly still
// announce their services, and the answer carries their name, host name and address together.
static const char *const SERVICES[] = {
  "_ipp._tcp", "_printer._tcp", "_pdl-datastream._tcp", "_scanner._tcp", "_googlecast._tcp",
  "_androidtvremote2._tcp", "_amzn-wplay._tcp", "_airplay._tcp", "_raop._tcp", "_spotify-connect._tcp",
  "_sonos._tcp", "_companion-link._tcp", "_workstation._tcp", "_smb._tcp", "_ssh._tcp", "_hap._tcp",
  "_matter._tcp", "_hue._tcp", "_http._tcp",
};
static const char *const SERVICE_LABELS[] = {
  "Printing", "Printing", "Printing", "Scanning", "Chromecast", "Android TV", "Fire TV", "AirPlay", "AirPlay audio",
  "Spotify Connect", "Sonos", "Apple devices", "Computer", "File sharing", "SSH", "HomeKit", "Matter", "Philips Hue",
  "Web page",
};
static const int SERVICE_COUNT = sizeof(SERVICES) / sizeof(SERVICES[0]);
static_assert(sizeof(SERVICE_LABELS) == sizeof(SERVICES), "a label for every service");
static_assert(SERVICE_COUNT <= 32, "services are kept as a 32-bit mask");
inline int serviceIndex(const char *type) {      // "_ipp._tcp.local" -> its index, or -1
  for (int i = 0; i < SERVICE_COUNT; i++) {
    const size_t n = strlen(SERVICES[i]);
    if (!strncasecmp_(type, SERVICES[i], n) && (type[n] == '.' || !type[n])) return i;
  }
  return -1;
}
inline bool hasService(uint32_t mask, const char *type) { const int i = serviceIndex(type); return i >= 0 && (mask >> i & 1); }

// ── guessing what a device is ───────────────────────────────────────────────────────────────

inline bool has(const char *hay, const char *needle) {   // case-insensitive substring
  if (!hay || !needle) return false;
  const size_t n = strlen(needle);
  for (; *hay; hay++) {
    size_t i = 0;
    while (i < n && hay[i] && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i])) i++;
    if (i == n) return true;
  }
  return false;
}

inline uint8_t guessKind(const Device &d, bool isGateway, bool isSelf) {
  if (isSelf) return K_BOARD;
  if (isGateway) return K_ROUTER;
  const char *maker = d.maker == 0xff ? "" : makerName(d.maker);
  char text[130];
  snprintf(text, sizeof text, "%s %s %s", d.name, d.model, d.title);
  auto any = [&](const char *const *w) { for (; *w; w++) if (has(text, *w)) return true; return false; };

  static const char *const phone[] = {"iphone", "galaxy", "pixel", "android", "oneplus", "redmi", "poco",
                                      "moto", "phone", "nothing-phone", nullptr};
  static const char *const tablet[] = {"ipad", "tab-", "tablet", "kindle", nullptr};
  static const char *const computer[] = {"macbook", "imac", "mac-mini", "macmini", "mac-studio", "desktop-",
                                         "laptop", "-pc", "thinkpad", "ubuntu", "workstation", "windows",
                                         "surface", nullptr};
  static const char *const tv[] = {"tv", "bravia", "roku", "chromecast", "fire-stick", "firetv", "shield",
                                   "apple-tv", "appletv", "webos", "tizen", nullptr};
  static const char *const printer[] = {"printer", "laserjet", "officejet", "deskjet", "pixma", "ecotank",
                                        "envy", nullptr};
  static const char *const speaker[] = {"echo", "alexa", "homepod", "sonos", "nest-mini", "google-home",
                                        "speaker", nullptr};
  static const char *const console[] = {"playstation", "ps4", "ps5", "xbox", "switch", "steam", nullptr};
  static const char *const camera[] = {"camera", "cam-", "ipcam", "doorbell", nullptr};
  static const char *const storage[] = {"nas", "diskstation", "synology", "qnap", nullptr};
  static const char *const router[] = {"router", "archer", "deco", "mesh", "repeater", "extender", "access point",
                                       nullptr};

  auto svc = [&](const char *t) { return hasService(d.svcs, t); };
  if (any(printer) || hasPort(d.ports, 9100) || hasPort(d.ports, 631) || svc("_ipp._tcp") || svc("_printer._tcp") ||
      svc("_pdl-datastream._tcp"))
    return K_PRINTER;
  if (any(console)) return K_CONSOLE;
  if (any(tv) || hasPort(d.ports, 8008) || hasPort(d.ports, 8060) || svc("_googlecast._tcp") ||
      svc("_androidtvremote2._tcp") || svc("_amzn-wplay._tcp"))
    return K_TV;
  if (any(tablet)) return K_TABLET;
  if (any(phone) || hasPort(d.ports, 62078)) return K_PHONE;
  if (any(computer) || hasPort(d.ports, 3389) || hasPort(d.ports, 548)) return K_COMPUTER;
  if (svc("_workstation._tcp") || svc("_smb._tcp") || svc("_ssh._tcp")) return K_COMPUTER;
  if (any(speaker) || svc("_sonos._tcp") || svc("_spotify-connect._tcp")) return K_SPEAKER;
  if (svc("_hap._tcp") || svc("_matter._tcp") || svc("_hue._tcp")) return K_SMART;
  if (svc("_raop._tcp") && !svc("_companion-link._tcp")) return K_SPEAKER;
  if (any(camera) || hasPort(d.ports, 554)) return K_CAMERA;
  if (any(storage) || hasPort(d.ports, 32400)) return K_STORAGE;
  if (any(router)) return K_ROUTER;

  static const char *const smartMakers[] = {"Espressif", "Tuya", "Shelly", "Philips Hue", "LIFX", "Yeelight",
                                            "Belkin", "ecobee", "Google Nest", "Silicon Labs", "Texas Instruments",
                                            "Nordic", "iRobot", "Dyson", "Wyze", "Bilian", nullptr};
  static const char *const tvMakers[] = {"Roku", "Hisense", "TCL", "Vizio", "Skyworth", "Sharp", "Amlogic", nullptr};
  static const char *const pcMakers[] = {"Intel", "Dell", "Lenovo", "HP", "ASUS", "Acer", "MSI", "Gigabyte",
                                         "Liteon", "AzureWave", "Foxconn", "Realtek", "Chicony", "Wistron",
                                         "Compal", "Quanta", "Pegatron", "Microsoft", "Lite-On", nullptr};
  static const char *const phoneMakers[] = {"Xiaomi", "OPPO", "vivo", "realme", "OnePlus", "Motorola", "Honor",
                                            "Transsion", "Nothing", "Huawei", nullptr};
  static const char *const netMakers[] = {"TP-Link", "Netgear", "D-Link", "Ubiquiti", "Cisco", "MikroTik",
                                          "Zyxel", "Tenda", "Mercusys", "eero", "AVM", "Arris", "Technicolor",
                                          "Sagemcom", "ZTE", "FiberHome", "Jio", "Airtel", nullptr};
  auto from = [&](const char *const *w) { for (; *w; w++) if (!strcmp(maker, *w)) return true; return false; };
  if (from(smartMakers)) return K_SMART;
  if (!strcmp(maker, "Sonos") || !strcmp(maker, "Bose") || !strcmp(maker, "Harman") || !strcmp(maker, "JBL"))
    return K_SPEAKER;
  if (!strcmp(maker, "Amazon")) return K_SPEAKER;          // most often an Echo; a Fire TV says so by name
  if (!strcmp(maker, "Nintendo") || !strcmp(maker, "Valve")) return K_CONSOLE;
  if (!strcmp(maker, "Canon") || !strcmp(maker, "Epson") || !strcmp(maker, "Brother") || !strcmp(maker, "Xerox") ||
      !strcmp(maker, "Kyocera"))
    return K_PRINTER;
  if (!strcmp(maker, "Hikvision") || !strcmp(maker, "Dahua") || !strcmp(maker, "EZVIZ") || !strcmp(maker, "Imou") ||
      !strcmp(maker, "Ring") || !strcmp(maker, "Arlo"))
    return K_CAMERA;
  if (!strcmp(maker, "Synology") || !strcmp(maker, "QNAP") || !strcmp(maker, "Western Digital")) return K_STORAGE;
  if (!strcmp(maker, "Raspberry Pi")) return K_COMPUTER;
  if (from(tvMakers)) return K_TV;
  if (from(phoneMakers)) return K_PHONE;
  if (from(pcMakers)) return K_COMPUTER;
  if (from(netMakers)) return K_ROUTER;
  if (macRandom(d.mac)) return K_PHONE;   // private addresses are a phone (or tablet, or watch) habit
  return K_UNKNOWN;
}


// ── packets: DNS-shaped questions (router DNS, Bonjour) and NetBIOS ───────────────────────────

// "Who is 192.168.1.7?" — a reverse (PTR) lookup, for the router's DNS or, sent straight to the
// device on port 5353, for the device itself (Macs, iPhones, Linux with Avahi answer it).
inline int buildPtrQuery(uint32_t ip, uint16_t id, uint8_t *buf, int cap) {
  char name[40];
  snprintf(name, sizeof name, "%u.%u.%u.%u.in-addr.arpa", (unsigned)(ip & 255), (unsigned)(ip >> 8 & 255),
           (unsigned)(ip >> 16 & 255), (unsigned)(ip >> 24));
  if (cap < 12 + (int)strlen(name) + 2 + 4) return 0;
  memset(buf, 0, 12);
  buf[0] = id >> 8; buf[1] = id & 255;
  buf[2] = 0x01;                          // recursion desired (ignored by Bonjour)
  buf[5] = 1;                             // one question
  int p = 12;
  for (const char *s = name; *s;) {
    const char *dot = strchr(s, '.');
    const int len = dot ? (int)(dot - s) : (int)strlen(s);
    buf[p++] = len;
    memcpy(buf + p, s, len);
    p += len;
    s += len + (dot ? 1 : 0);
  }
  buf[p++] = 0;
  buf[p++] = 0; buf[p++] = 12;            // type PTR
  buf[p++] = 0; buf[p++] = 1;             // class IN
  return p;
}

// Reads a (possibly compressed) DNS name at *pos into out ("a.b.c"); moves *pos past it.
inline bool dnsName(const uint8_t *b, int len, int *pos, char *out, int cap) {
  int p = *pos, o = 0, jumps = 0;
  bool jumped = false;
  out[0] = 0;
  while (p < len) {
    const uint8_t l = b[p];
    if (l == 0) { if (!jumped) *pos = p + 1; out[o] = 0; return true; }
    if ((l & 0xC0) == 0xC0) {
      if (p + 1 >= len || ++jumps > 16) return false;
      if (!jumped) *pos = p + 2;
      jumped = true;
      p = (l & 0x3F) << 8 | b[p + 1];
      continue;
    }
    if (p + 1 + l > len) return false;
    if (o && o < cap - 1) out[o++] = '.';
    for (int i = 0; i < l && o < cap - 1; i++) out[o++] = b[p + 1 + i];
    p += 1 + l;
  }
  return false;
}

// The first label of a name, tidied: "Keshavs-MacBook-Air.local." -> "Keshavs-MacBook-Air".
inline void shortName(const char *full, char *out, int cap) {
  int o = 0;
  for (const char *s = full; *s && *s != '.' && o < cap - 1; s++) out[o++] = (*s == '_' ? '-' : *s);
  out[o] = 0;
}

// The PTR answer to a query with this id, as a short name. False when there isn't one.
inline bool parsePtrReply(const uint8_t *b, int len, uint16_t id, char *out, int cap) {
  if (len < 12 || (b[0] << 8 | b[1]) != id || !(b[2] & 0x80) || (b[3] & 0x0F)) return false;
  const int qd = b[4] << 8 | b[5], an = b[6] << 8 | b[7];
  int p = 12;
  char tmp[128];
  for (int i = 0; i < qd; i++) { if (!dnsName(b, len, &p, tmp, sizeof tmp)) return false; p += 4; }
  for (int i = 0; i < an; i++) {
    if (!dnsName(b, len, &p, tmp, sizeof tmp) || p + 10 > len) return false;
    const int type = b[p] << 8 | b[p + 1], rdlen = b[p + 8] << 8 | b[p + 9];
    p += 10;
    if (p + rdlen > len) return false;
    if (type == 12) {
      int q = p;
      if (!dnsName(b, len, &q, tmp, sizeof tmp)) return false;
      shortName(tmp, out, cap);
      return out[0] != 0;
    }
    p += rdlen;
  }
  return false;
}

// One packet asking for every service type (PTR "_ipp._tcp.local" …); answers come back to our port.
inline int buildBrowse(uint16_t id, uint8_t *buf, int cap) {
  memset(buf, 0, 12);
  buf[0] = id >> 8; buf[1] = id & 255;
  int p = 12;
  for (int i = 0; i < SERVICE_COUNT; i++) {
    char name[48];
    snprintf(name, sizeof name, "%s.local", SERVICES[i]);
    if (p + (int)strlen(name) + 6 > cap) return 0;
    for (const char *s = name; *s;) {
      const char *dot = strchr(s, '.');
      const int len = dot ? (int)(dot - s) : (int)strlen(s);
      buf[p++] = len;
      memcpy(buf + p, s, len);
      p += len;
      s += len + (dot ? 1 : 0);
    }
    buf[p++] = 0;
    buf[p++] = 0; buf[p++] = 12;          // PTR
    buf[p++] = 0x80; buf[p++] = 1;        // IN, "answer me directly" (QU)
  }
  buf[4] = SERVICE_COUNT >> 8; buf[5] = SERVICE_COUNT & 255;
  return p;
}

struct Found { uint32_t ip; char host[40]; char instance[40]; uint32_t svcs; };

// Reads one Bonjour reply: PTR (service -> instance), SRV (instance -> host), A (host -> address),
// from every section, and joins them up. Returns how many devices it described.
inline int parseBrowse(const uint8_t *b, int len, Found *out, int cap) {
  if (len < 12 || !(b[2] & 0x80)) return 0;
  const int qd = b[4] << 8 | b[5], rr = (b[6] << 8 | b[7]) + (b[8] << 8 | b[9]) + (b[10] << 8 | b[11]);
  int p = 12;
  char name[128], data[128];
  for (int i = 0; i < qd; i++) { if (!dnsName(b, len, &p, name, sizeof name)) return 0; p += 4; }
  struct Ptr { char svc[40]; char inst[96]; }; static Ptr ptr[16];
  struct Srv { char inst[96]; char host[64]; }; static Srv srv[16];
  struct A { char host[64]; uint32_t ip; }; static A a[16];
  int np = 0, ns = 0, na = 0;
  for (int i = 0; i < rr && p < len; i++) {
    if (!dnsName(b, len, &p, name, sizeof name) || p + 10 > len) break;
    const int type = b[p] << 8 | b[p + 1], rdlen = b[p + 8] << 8 | b[p + 9];
    p += 10;
    if (p + rdlen > len) break;
    int q = p;
    if (type == 12 && np < 16 && serviceIndex(name) >= 0 && dnsName(b, len, &q, data, sizeof data)) {
      snprintf(ptr[np].svc, sizeof ptr[np].svc, "%s", name);
      snprintf(ptr[np].inst, sizeof ptr[np].inst, "%s", data);
      np++;
    } else if (type == 33 && ns < 16 && rdlen > 6) {
      q = p + 6;
      if (dnsName(b, len, &q, data, sizeof data)) {
        snprintf(srv[ns].inst, sizeof srv[ns].inst, "%s", name);
        snprintf(srv[ns].host, sizeof srv[ns].host, "%s", data);
        ns++;
      }
    } else if (type == 1 && rdlen == 4 && na < 16) {
      snprintf(a[na].host, sizeof a[na].host, "%s", name);
      a[na].ip = (uint32_t)b[p] << 24 | b[p + 1] << 16 | b[p + 2] << 8 | b[p + 3];
      na++;
    }
    p += rdlen;
  }
  int got = 0;
  auto add = [&](uint32_t ip, const char *host, const char *inst, const char *svc) {
    Found *f = nullptr;
    for (int k = 0; k < got && !f; k++) if (out[k].ip == ip) f = &out[k];
    if (!f) {
      if (got >= cap) return;
      f = &out[got++];
      memset(f, 0, sizeof *f);
      f->ip = ip;
    }
    if (host && host[0] && !f->host[0]) shortName(host, f->host, sizeof f->host);
    if (inst && inst[0] && !f->instance[0]) {
      // the instance's first label, which may itself contain dots: "Brother DCP-T420W"
      const size_t sl = svc ? strlen(svc) : 0, il = strlen(inst);
      int keep = il > sl + 1 && sl && !strcasecmp_(inst + il - sl, svc) ? (int)(il - sl - 1) : (int)il;
      if (keep > (int)sizeof f->instance - 1) keep = sizeof f->instance - 1;
      memcpy(f->instance, inst, keep);
      f->instance[keep] = 0;
    }
    if (svc) { const int si = serviceIndex(svc); if (si >= 0) f->svcs |= 1u << si; }
  };
  for (int i = 0; i < np; i++) {
    const char *host = nullptr;
    for (int j = 0; j < ns && !host; j++) if (!strcasecmp_(srv[j].inst, ptr[i].inst)) host = srv[j].host;
    if (!host) continue;
    for (int k = 0; k < na; k++) if (!strcasecmp_(a[k].host, host)) add(a[k].ip, host, ptr[i].inst, ptr[i].svc);
  }
  for (int k = 0; k < na; k++) add(a[k].ip, a[k].host, nullptr, nullptr);   // an address alone still names a host
  return got;
}

// What a browse found, put on the device: its Bonjour name ("Keshav's MacBook Air", "HP Smart
// Tank 580-590 series") beats its host name, and a printer's or TV's name is also its model.
inline void applyFound(Device &d, const Found &f) {
  char inst[40];
  snprintf(inst, sizeof inst, "%s", f.instance);
  char *br = strstr(inst, " [");                 // "pi [b8:27:eb:…]", "HP … [8E0396]": the tag is noise
  if (br) *br = 0;
  if (inst[0]) setName(d, inst, N_BONJOUR);
  else setName(d, f.host, N_MDNS);
  d.svcs |= f.svcs;
  const bool product = hasService(f.svcs, "_ipp._tcp") || hasService(f.svcs, "_printer._tcp") ||
                       hasService(f.svcs, "_googlecast._tcp") || hasService(f.svcs, "_sonos._tcp");
  if (product && inst[0] && !d.model[0]) snprintf(d.model, sizeof d.model, "%s", inst);
}

// NetBIOS "node status": Windows PCs (and Samba) answer with their computer name.
inline int buildNbstat(uint16_t id, uint8_t *buf, int cap) {
  if (cap < 50) return 0;
  memset(buf, 0, 50);
  buf[0] = id >> 8; buf[1] = id & 255;
  buf[5] = 1;
  buf[12] = 32;                           // the name "*", first-level encoded: 'C','K' then 'A','A' ×15
  buf[13] = 'C'; buf[14] = 'K';
  for (int i = 15; i < 45; i++) buf[i] = 'A';
  buf[45] = 0;
  buf[46] = 0; buf[47] = 0x21;            // type NBSTAT
  buf[48] = 0; buf[49] = 1;               // class IN
  return 50;
}

inline bool parseNbstat(const uint8_t *b, int len, uint16_t id, char *out, int cap) {
  if (len < 12 || (b[0] << 8 | b[1]) != id || !(b[2] & 0x80)) return false;
  int p = 12;
  char tmp[80];
  if (!dnsName(b, len, &p, tmp, sizeof tmp) || p + 11 > len) return false;
  if ((b[p] << 8 | b[p + 1]) != 0x21) return false;
  p += 10;
  const int count = b[p++];
  for (int i = 0; i < count && p + 18 <= len; i++, p += 18) {
    const uint8_t suffix = b[p + 15];
    const uint16_t flags = b[p + 16] << 8 | b[p + 17];
    if (suffix != 0x00 || (flags & 0x8000)) continue;      // the workstation name, not a group
    int o = 0;
    for (int j = 0; j < 15 && o < cap - 1; j++) out[o++] = b[p + j];
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
    if (o) return true;
  }
  return false;
}

// ── UPnP: TVs, routers and speakers describe themselves in an XML file ────────────────────────

// Copies the text of the first <tag>…</tag> in xml. False if absent.
inline bool xmlTag(const char *xml, const char *tag, char *out, int cap) {
  char open[40];
  snprintf(open, sizeof open, "<%s>", tag);
  const char *a = strstr(xml, open);
  if (!a) return false;
  a += strlen(open);
  const char *e = strchr(a, '<');
  if (!e) return false;
  int o = 0;
  for (const char *s = a; s < e && o < cap - 1; s++) out[o++] = (*s == '\n' || *s == '\r' || *s == '\t') ? ' ' : *s;
  while (o && out[o - 1] == ' ') o--;
  out[o] = 0;
  return o > 0;
}

// The LOCATION: header of an SSDP reply (case varies by device).
inline bool ssdpLocation(const char *reply, char *out, int cap) {
  for (const char *s = reply; *s; s++) {
    if ((s == reply || s[-1] == '\n') && has(s, "location:") && tolower((unsigned char)s[0]) == 'l') {
      const char *v = s + 9;
      while (*v == ' ') v++;
      int o = 0;
      while (*v && *v != '\r' && *v != '\n' && o < cap - 1) out[o++] = *v++;
      out[o] = 0;
      return o > 0;
    }
  }
  return false;
}

// "Samsung 55" QLED" from manufacturer + model, without saying the maker twice.
inline void describeModel(const char *maker, const char *friendly, const char *model, char *out, int cap) {
  if (friendly && friendly[0]) { snprintf(out, cap, "%s", friendly); return; }
  if (maker && maker[0] && model && model[0] && !has(model, maker)) snprintf(out, cap, "%s %s", maker, model);
  else snprintf(out, cap, "%s", model && model[0] ? model : (maker ? maker : ""));
}

// A web page's <title>, tidied.
inline bool htmlTitle(const char *html, char *out, int cap) {
  const char *a = nullptr;
  for (const char *s = html; *s && !a; s++)
    if (*s == '<' && tolower((unsigned char)s[1]) == 't' && tolower((unsigned char)s[2]) == 'i' &&
        tolower((unsigned char)s[3]) == 't' && tolower((unsigned char)s[4]) == 'l' &&
        tolower((unsigned char)s[5]) == 'e' && (s[6] == '>' || s[6] == ' '))
      a = s;
  if (!a) return false;
  a = strchr(a, '>');
  if (!a) return false;
  a++;
  int o = 0;
  bool space = false;
  for (const char *s = a; *s && *s != '<' && o < cap - 1; s++) {
    if (isspace((unsigned char)*s)) { space = o > 0; continue; }
    if (space) { out[o++] = ' '; space = false; if (o >= cap - 1) break; }
    out[o++] = *s;
  }
  out[o] = 0;
  return o > 0;
}

// Wake-on-LAN: six 0xFF then the MAC sixteen times.
inline int buildMagic(const uint8_t *mac, uint8_t *buf) {
  memset(buf, 0xff, 6);
  for (int i = 0; i < 16; i++) memcpy(buf + 6 + i * 6, mac, 6);
  return 102;
}

// ── the latency graph: one ping a second, the last five minutes ─────────────────────────────

const int RING = 300;
struct Ring {
  int16_t v[RING];           // ms, or -1 for a lost ping
  int head, count;
  void clear() { head = count = 0; }
  void add(int ms) { v[head] = ms < 0 ? -1 : (ms > 30000 ? 30000 : ms); head = (head + 1) % RING; if (count < RING) count++; }
  int at(int i) const { return v[(head - count + i + RING * 2) % RING]; }   // 0 = oldest
};
struct Stats { int min, avg, max, jitter, lossPct, count; };
inline Stats stats(const Ring &r, int last) {
  Stats s = {0, 0, 0, 0, 0, 0};
  const int from = r.count > last ? r.count - last : 0;
  long sum = 0, jsum = 0;
  int got = 0, lost = 0, prev = -1, jn = 0;
  for (int i = from; i < r.count; i++) {
    const int x = r.at(i);
    if (x < 0) { lost++; continue; }
    if (!got || x < s.min) s.min = x;
    if (x > s.max) s.max = x;
    sum += x;
    got++;
    if (prev >= 0) { jsum += x > prev ? x - prev : prev - x; jn++; }
    prev = x;
  }
  s.count = got + lost;
  s.avg = got ? (int)((sum + got / 2) / got) : 0;
  s.jitter = jn ? (int)((jsum + jn / 2) / jn) : 0;
  s.lossPct = s.count ? (lost * 100 + s.count / 2) / s.count : 0;
  return s;
}

// ── text out ────────────────────────────────────────────────────────────────────────────────

// One CSV field: quoted when it has a comma, quote or newline; a leading = + - @ is defused so a
// spreadsheet never runs a device's name as a formula.
inline void csvField(const char *s, char *out, int cap) {
  int o = 0;
  const bool risky = s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@';
  const bool quote = risky || strpbrk(s, ",\"\n\r") != nullptr;
  if (quote && o < cap - 1) out[o++] = '"';
  if (risky && o < cap - 1) out[o++] = '\'';
  for (; *s && o < cap - 2; s++) {
    if (*s == '"') { if (o < cap - 3) { out[o++] = '"'; out[o++] = '"'; } continue; }
    out[o++] = (*s == '\n' || *s == '\r') ? ' ' : *s;
  }
  if (quote && o < cap - 1) out[o++] = '"';
  out[o] = 0;
}
