#pragma once
//
// The board's side of the conversation: asking the network who is there.
//
//   arp       "who has 192.168.1.7?" for every address — answered by anything that is on the WiFi,
//             even phones and PCs that ignore ping. lwIP keeps only ten answers, so the addresses
//             go out eight at a time and the answers are read back before the next eight.
//   ping      how long a device takes to answer, and the once-a-second latency graph.
//   names     three questions at once per device: Bonjour (Macs, iPhones, Linux), NetBIOS
//             (Windows) and the router's DNS (which often knows the names from DHCP).
//   upnp      TVs, routers and speakers describe themselves in an XML file.
//   ports     which of the common doors are open, by trying to connect to each.
//   wake      the Wake-on-LAN "magic packet".
//
// All of it is blocking, and runs in the scanner's own task, never in loop().

#include <WiFi.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>
#include <ping/ping_sock.h>
#include "net.h"

namespace probe {

// ── lwIP's ARP table is only safe to touch from lwIP's own thread ──────────────────────────────

struct Call { void (*fn)(void *); void *arg; SemaphoreHandle_t done; };
static void trampoline(void *p) {
  Call *c = (Call *)p;
  c->fn(c->arg);
  xSemaphoreGive(c->done);
}
static void inLwip(void (*fn)(void *), void *arg) {
  static SemaphoreHandle_t done = xSemaphoreCreateBinary();
  Call c = {fn, arg, done};
  if (tcpip_callback(trampoline, &c) == ERR_OK) xSemaphoreTake(done, portMAX_DELAY);
}

struct ArpAsk { uint32_t ip[8]; int n; };
static void arpAsk(void *p) {
  ArpAsk *a = (ArpAsk *)p;
  if (!netif_default) return;
  for (int i = 0; i < a->n; i++) {
    ip4_addr_t t;
    t.addr = lwip_htonl(a->ip[i]);
    etharp_request(netif_default, &t);
  }
}
struct ArpSeen { uint32_t ip[ARP_TABLE_SIZE]; uint8_t mac[ARP_TABLE_SIZE][6]; int n; };
static void arpRead(void *p) {
  ArpSeen *s = (ArpSeen *)p;
  s->n = 0;
  for (size_t i = 0; i < ARP_TABLE_SIZE; i++) {
    ip4_addr_t *ip;
    struct netif *nif;
    struct eth_addr *eth;
    if (etharp_get_entry(i, &ip, &nif, &eth)) {
      s->ip[s->n] = lwip_ntohl(ip->addr);
      memcpy(s->mac[s->n], eth->addr, 6);
      s->n++;
    }
  }
}

// Asks these addresses (up to 8) and gives back every answer lwIP holds after waitMs.
inline void arp(const uint32_t *ips, int n, int waitMs, ArpSeen &out) {
  ArpAsk a;
  a.n = n > 8 ? 8 : n;
  memcpy(a.ip, ips, a.n * sizeof(uint32_t));
  inLwip(arpAsk, &a);
  vTaskDelay(pdMS_TO_TICKS(waitMs));
  inLwip(arpRead, &out);
}

// ── ping ────────────────────────────────────────────────────────────────────────────────────

inline void toLwip(uint32_t ip, ip_addr_t &t) {
  IP_ADDR4(&t, ip >> 24, ip >> 16 & 255, ip >> 8 & 255, ip & 255);
}

struct PingWait { SemaphoreHandle_t done; int ms; };
static void pingOk(esp_ping_handle_t h, void *a) {
  uint32_t t = 0;
  esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &t, sizeof t);
  ((PingWait *)a)->ms = (int)t;
}
static void pingEnd(esp_ping_handle_t, void *a) { xSemaphoreGive(((PingWait *)a)->done); }

// One ping; ms, or -1 if it didn't answer.
inline int pingOnce(uint32_t ip, int timeoutMs = 700) {
  esp_ping_config_t c = ESP_PING_DEFAULT_CONFIG();
  toLwip(ip, c.target_addr);
  c.count = 1;
  c.timeout_ms = timeoutMs;
  c.interval_ms = 10;
  PingWait w = {xSemaphoreCreateBinary(), -1};
  esp_ping_callbacks_t cb = {};
  cb.cb_args = &w;
  cb.on_ping_success = pingOk;
  cb.on_ping_end = pingEnd;
  esp_ping_handle_t h;
  if (esp_ping_new_session(&c, &cb, &h) != ESP_OK) { vSemaphoreDelete(w.done); return -1; }
  esp_ping_start(h);
  xSemaphoreTake(w.done, pdMS_TO_TICKS(timeoutMs + 1500));
  esp_ping_stop(h);
  esp_ping_delete_session(h);
  vSemaphoreDelete(w.done);
  return w.ms;
}

// The latency graph: a ping a second, forever, to one address. Each answer (or loss) goes to onSample.
static esp_ping_handle_t watcher = nullptr;
static void (*onSample)(int ms) = nullptr;
static void watchOk(esp_ping_handle_t h, void *) {
  uint32_t t = 0;
  esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &t, sizeof t);
  if (onSample) onSample((int)t);
}
static void watchLost(esp_ping_handle_t, void *) { if (onSample) onSample(-1); }
inline void watch(uint32_t ip, void (*sample)(int)) {
  if (watcher) { esp_ping_stop(watcher); esp_ping_delete_session(watcher); watcher = nullptr; }
  if (!ip) return;
  onSample = sample;
  esp_ping_config_t c = ESP_PING_DEFAULT_CONFIG();
  toLwip(ip, c.target_addr);
  c.count = ESP_PING_COUNT_INFINITE;
  c.interval_ms = 1000;
  c.timeout_ms = 1000;
  esp_ping_callbacks_t cb = {};
  cb.on_ping_success = watchOk;
  cb.on_ping_timeout = watchLost;
  if (esp_ping_new_session(&c, &cb, &watcher) == ESP_OK) esp_ping_start(watcher);
  else watcher = nullptr;
}

// ── UDP questions ───────────────────────────────────────────────────────────────────────────

inline sockaddr_in addr(uint32_t ip, uint16_t port) {
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = lwip_htons(port);
  a.sin_addr.s_addr = lwip_htonl(ip);
  return a;
}

inline int udpSocket() {
  const int s = lwip_socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) return -1;
  sockaddr_in me = addr(0, 0);
  lwip_bind(s, (sockaddr *)&me, sizeof me);
  return s;
}

// Waits up to ms for one datagram. Returns its length (0 on timeout) and who sent it.
inline int receive(int s, uint8_t *buf, int cap, int ms, uint32_t *fromIp, uint16_t *fromPort) {
  fd_set r;
  FD_ZERO(&r);
  FD_SET(s, &r);
  timeval tv = {ms / 1000, (ms % 1000) * 1000};
  if (lwip_select(s + 1, &r, nullptr, nullptr, &tv) <= 0) return 0;
  sockaddr_in from = {};
  socklen_t fl = sizeof from;
  const int n = lwip_recvfrom(s, buf, cap, 0, (sockaddr *)&from, &fl);
  if (n <= 0) return 0;
  *fromIp = lwip_ntohl(from.sin_addr.s_addr);
  *fromPort = lwip_ntohs(from.sin_port);
  return n;
}

struct Named { uint32_t ip; char name[40]; uint8_t src; };

// Asks up to 8 devices their names three ways at once and collects what comes back in ~700 ms.
// Each answer is matched by its query id, so a late reply can't be pinned on the wrong device.
inline int names(const uint32_t *ips, int n, uint32_t dnsServer, Named *out, int cap) {
  const int s = udpSocket();
  if (s < 0) return 0;
  static uint16_t seq = 0;
  const uint16_t base = (uint16_t)(++seq * 32);
  uint8_t buf[512];
  for (int i = 0; i < n && i < 8; i++) {
    int len = buildPtrQuery(ips[i], base + i, buf, sizeof buf);
    sockaddr_in a = addr(ips[i], 5353);
    lwip_sendto(s, buf, len, 0, (sockaddr *)&a, sizeof a);
    if (dnsServer) {
      len = buildPtrQuery(ips[i], base + 8 + i, buf, sizeof buf);
      a = addr(dnsServer, 53);
      lwip_sendto(s, buf, len, 0, (sockaddr *)&a, sizeof a);
    }
    len = buildNbstat(base + 16 + i, buf, sizeof buf);
    a = addr(ips[i], 137);
    lwip_sendto(s, buf, len, 0, (sockaddr *)&a, sizeof a);
  }
  int got = 0;
  const uint32_t until = millis() + 700;
  while (got < cap && (int32_t)(until - millis()) > 0) {
    uint32_t fip;
    uint16_t fport;
    const int len = receive(s, buf, sizeof buf, until - millis(), &fip, &fport);
    if (len <= 0) break;
    const uint16_t id = buf[0] << 8 | buf[1];
    if (id < base || id >= base + 24) continue;
    const int i = (id - base) % 8;
    if (i >= n) continue;
    Named &o = out[got];
    o.ip = ips[i];
    if (fport == 137 && parseNbstat(buf, len, id, o.name, sizeof o.name)) { o.src = N_NETBIOS; got++; }
    else if (fport == 5353 && parsePtrReply(buf, len, id, o.name, sizeof o.name)) { o.src = N_MDNS; got++; }
    else if (fport == 53 && parsePtrReply(buf, len, id, o.name, sizeof o.name)) { o.src = N_DNS; got++; }
  }
  lwip_close(s);
  return got;
}

// Asks the Bonjour group which services are here, and collects ~2.5 s of answers.
inline int browse(Found *out, int cap) {
  const int s = udpSocket();
  if (s < 0) return 0;
  static uint8_t buf[1500];
  const int len = buildBrowse(0x4d44, buf, sizeof buf);
  sockaddr_in a = addr(0xE00000FB, 5353);
  lwip_sendto(s, buf, len, 0, (sockaddr *)&a, sizeof a);
  int got = 0;
  const uint32_t until = millis() + 2500;
  while ((int32_t)(until - millis()) > 0) {
    uint32_t fip;
    uint16_t fport;
    const int n = receive(s, buf, sizeof buf, until - millis(), &fip, &fport);
    if (n <= 0) break;
    static Found one[12];
    const int g = parseBrowse(buf, n, one, 12);
    for (int i = 0; i < g; i++) {               // merge with what earlier replies said about the same address
      Found *f = nullptr;
      for (int k = 0; k < got && !f; k++) if (out[k].ip == one[i].ip) f = &out[k];
      if (!f) { if (got >= cap) continue; f = &out[got++]; *f = one[i]; continue; }
      f->svcs |= one[i].svcs;
      if (!f->host[0]) memcpy(f->host, one[i].host, sizeof f->host);
      if (!f->instance[0]) memcpy(f->instance, one[i].instance, sizeof f->instance);
    }
  }
  lwip_close(s);
  return got;
}

// ── UPnP ───────────────────────────────────────────────────────────────────────────────────

struct Upnp { uint32_t ip; char location[96]; };

// "Who's there?" to the UPnP multicast group; one LOCATION per device that answers in ~2.5 s.
inline int ssdp(Upnp *out, int cap) {
  const int s = udpSocket();
  if (s < 0) return 0;
  static const char ask[] =
      "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
      "ST: upnp:rootdevice\r\n\r\n";
  sockaddr_in a = addr(0xEFFFFFFA, 1900);
  lwip_sendto(s, ask, sizeof ask - 1, 0, (sockaddr *)&a, sizeof a);
  vTaskDelay(pdMS_TO_TICKS(100));
  lwip_sendto(s, ask, sizeof ask - 1, 0, (sockaddr *)&a, sizeof a);
  int got = 0;
  static char buf[1024];
  const uint32_t until = millis() + 2500;
  while (got < cap && (int32_t)(until - millis()) > 0) {
    uint32_t fip;
    uint16_t fport;
    const int len = receive(s, (uint8_t *)buf, sizeof buf - 1, until - millis(), &fip, &fport);
    if (len <= 0) break;
    buf[len] = 0;
    bool dup = false;
    for (int i = 0; i < got && !dup; i++) dup = out[i].ip == fip;
    if (dup || !ssdpLocation(buf, out[got].location, sizeof out[got].location)) continue;
    if (strncmp(out[got].location, "http://", 7)) continue;
    out[got++].ip = fip;
  }
  lwip_close(s);
  return got;
}

// Fetches up to cap-1 bytes of a plain-http page (enough for a description or a <title>).
// A few lines of HTTP by hand rather than HTTPClient, which brings TLS along and would double the
// firmware. Redirects aren't followed; the body is what the device itself says.
inline int fetch(const String &url, char *buf, int cap, int timeoutMs = 1500) {
  if (!url.startsWith("http://")) return 0;
  const int hs = 7, slash = url.indexOf('/', hs);
  const String hostPort = url.substring(hs, slash < 0 ? url.length() : slash);
  const String path = slash < 0 ? "/" : url.substring(slash);
  const int colon = hostPort.indexOf(':');
  const String host = colon < 0 ? hostPort : hostPort.substring(0, colon);
  const uint16_t port = colon < 0 ? 80 : hostPort.substring(colon + 1).toInt();
  const uint32_t ip = ipParse(host.c_str());
  if (!ip || !port) return 0;
  WiFiClient c;
  if (!c.connect(IPAddress(ip >> 24, ip >> 16 & 255, ip >> 8 & 255, ip & 255), port, timeoutMs)) return 0;
  c.print("GET " + path + " HTTP/1.0\r\nHost: " + hostPort + "\r\nUser-Agent: devkit-scan\r\nConnection: close\r\n\r\n");
  int n = 0;
  bool body = false, ok = false;
  const uint32_t until = millis() + timeoutMs;
  String line;
  while ((c.connected() || c.available()) && (int32_t)(until - millis()) > 0 && n < cap - 1) {
    if (!c.available()) { delay(5); continue; }
    if (!body) {                          // status line and headers, one line at a time
      const char ch = c.read();
      if (ch == '\n') {
        if (!line.length() || line == "\r") { body = true; if (!ok) break; }
        else if (line.startsWith("HTTP/")) ok = line.indexOf(" 200") > 0;
        line = "";
      } else if (line.length() < 200) line += ch;
      continue;
    }
    n += c.read((uint8_t *)buf + n, cap - 1 - n);
  }
  c.stop();
  buf[n] = 0;
  return ok ? n : 0;
}

// ── ports ───────────────────────────────────────────────────────────────────────────────────

// Tries every port in PORTS, six at a time, each given ~800 ms to accept. Returns the open mask.
inline uint32_t ports(uint32_t ip) {
  uint32_t open = 0;
  for (int at = 0; at < PORT_COUNT; at += 6) {
    int socks[6], idx[6], n = 0;
    for (int i = at; i < at + 6 && i < PORT_COUNT; i++) {
      const int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
      if (s < 0) break;
      lwip_fcntl(s, F_SETFL, lwip_fcntl(s, F_GETFL, 0) | O_NONBLOCK);
      sockaddr_in a = addr(ip, PORTS[i].port);
      lwip_connect(s, (sockaddr *)&a, sizeof a);
      socks[n] = s;
      idx[n++] = i;
    }
    const uint32_t until = millis() + 800;
    int left = n;
    bool done[6] = {};
    while (left && (int32_t)(until - millis()) > 0) {
      fd_set w;
      FD_ZERO(&w);
      int top = 0;
      for (int k = 0; k < n; k++) if (!done[k]) { FD_SET(socks[k], &w); if (socks[k] > top) top = socks[k]; }
      const int ms = until - millis();
      timeval tv = {ms / 1000, (ms % 1000) * 1000};
      if (lwip_select(top + 1, nullptr, &w, nullptr, &tv) <= 0) break;
      for (int k = 0; k < n; k++) {
        if (done[k] || !FD_ISSET(socks[k], &w)) continue;
        int err = 0;
        socklen_t el = sizeof err;
        lwip_getsockopt(socks[k], SOL_SOCKET, SO_ERROR, &err, &el);
        if (!err) open |= 1u << idx[k];
        done[k] = true;
        left--;
      }
    }
    for (int k = 0; k < n; k++) lwip_close(socks[k]);
  }
  return open;
}

// ── wake ────────────────────────────────────────────────────────────────────────────────────

inline bool wake(const uint8_t *mac, uint32_t subnetBroadcast) {
  const int s = udpSocket();
  if (s < 0) return false;
  int yes = 1;
  lwip_setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof yes);
  uint8_t pkt[102];
  buildMagic(mac, pkt);
  bool ok = false;
  for (uint16_t port : {9, 7}) {
    sockaddr_in a = addr(0xFFFFFFFF, port);
    ok |= lwip_sendto(s, pkt, sizeof pkt, 0, (sockaddr *)&a, sizeof a) == (int)sizeof pkt;
    a = addr(subnetBroadcast, port);
    ok |= lwip_sendto(s, pkt, sizeof pkt, 0, (sockaddr *)&a, sizeof a) == (int)sizeof pkt;
  }
  lwip_close(s);
  return ok;
}

}  // namespace probe
