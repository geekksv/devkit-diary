// Tests net.h on a laptop:  c++ -std=c++17 -I../src/scanner net_test.cpp -o net_test && ./net_test
#include "net.h"
#include <vector>
#include <string>

static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

typedef std::vector<uint8_t> Bytes;
static void label(Bytes &b, const std::string &dotted) {
  size_t s = 0;
  while (s < dotted.size()) {
    size_t e = dotted.find('.', s);
    if (e == std::string::npos) e = dotted.size();
    b.push_back(e - s);
    b.insert(b.end(), dotted.begin() + s, dotted.begin() + e);
    s = e + 1;
  }
}
static void u16(Bytes &b, int v) { b.push_back(v >> 8); b.push_back(v & 255); }

int main() {
  // ── addresses
  uint8_t m[6];
  char s[64];
  CHECK(macParse("dc:a6:32:01:02:03", m) && m[0] == 0xdc && m[5] == 3);
  macStr(m, s); CHECK(!strcmp(s, "DC:A6:32:01:02:03"));
  CHECK(!macParse("dc:a6:32:01:02", m));
  CHECK(ipParse("192.168.68.66") == (192u << 24 | 168 << 16 | 68 << 8 | 66));
  CHECK(ipParse("192.168.1.300") == 0 && ipParse("nope") == 0);
  ipStr(ipParse("10.0.0.254"), s); CHECK(!strcmp(s, "10.0.0.254"));

  // ── makers
  auto maker = [&](const char *mac) { uint8_t x[6]; macParse(mac, x); const uint8_t i = ouiMaker(x); return std::string(i == 0xff ? "-" : makerName(i)); };
  CHECK(maker("DC:A6:32:00:00:01") == "Raspberry Pi");
  CHECK(maker("24:0A:C4:00:00:01") == "Espressif");
  CHECK(maker("3C:22:FB:12:34:56") == "Apple");
  CHECK(maker("50:C7:BF:00:00:01") == "TP-Link");
  CHECK(maker("F0:D5:BF:00:00:01") == "Intel");
  CHECK(maker("A4:C1:38:00:00:01") == "-");                 // a maker not in the home list
  CHECK(maker("3E:22:FB:12:34:56") == "-");                 // private (random) address: no maker
  CHECK(macRandom((const uint8_t[]){0x3e, 0, 0, 0, 0, 0}) && !macRandom((const uint8_t[]){0x3c, 0, 0, 0, 0, 0}));
  for (uint32_t i = 1; i < OUI_COUNT; i++) if (OUI_TABLE[i] >> 8 <= OUI_TABLE[i - 1] >> 8) { CHECK(!"table sorted"); break; }

  // ── the table: first scan is the baseline, newcomers after it are new; leaving takes 3 misses
  static Net net;
  memset(&net, 0, sizeof net);
  uint8_t a[6] = {0xdc, 0xa6, 0x32, 1, 1, 1}, b[6] = {0x3e, 0x11, 0x22, 3, 3, 3}, c[6] = {0x24, 0x0a, 0xc4, 9, 9, 9};
  const uint32_t ipA = ipParse("192.168.1.10"), ipB = ipParse("192.168.1.11"), ipC = ipParse("192.168.1.12");
  bool fresh;
  Device *da = seen(net, a, ipA, 1000, &fresh);
  CHECK(da && fresh && !da->isNew && da->first == 1000 && da->online);
  seen(net, b, ipB, 1000);
  uint32_t ans1[] = {ipA, ipB};
  CHECK(endSweep(net, ans1, 2) == 2 && net.baselined);
  Device *dc = seen(net, c, ipC, 2000, &fresh);
  CHECK(fresh && dc->isNew);
  CHECK(seen(net, a, ipA, 2000, &fresh) == da && !fresh && da->last == 2000);
  uint32_t ans2[] = {ipA, ipC};
  endSweep(net, ans2, 2);
  Device *db = byMac(net, b);
  CHECK(db->online && db->misses == 1);
  endSweep(net, ans2, 2);
  CHECK(db->online);
  CHECK(endSweep(net, ans2, 2) == 2 && !db->online && db->rtt == -1);
  // it comes back at a new address: ports and title learned at the old one are dropped
  db->ports = 5; strcpy(db->title, "x");
  seen(net, b, ipParse("192.168.1.40"), 3000);
  CHECK(db->online && db->ip == ipParse("192.168.1.40") && db->ports == 0 && !db->title[0]);
  CHECK(byIp(net, ipParse("192.168.1.40")) == db && byIp(net, ipParse("192.168.1.99")) == nullptr);
  CHECK(!seen(net, (const uint8_t[]){0, 0, 0, 0, 0, 0}, ipA, 1));

  // full: the oldest unnamed absent device makes way; named ones and present ones stay
  static Net full;
  memset(&full, 0, sizeof full);
  for (int i = 0; i < MAX_DEV; i++) {
    uint8_t x[6] = {0x02, 0, 0, 0, (uint8_t)(i >> 8), (uint8_t)i};
    Device *d = seen(full, x, 0x0a000000u + i + 1, 100 + i);
    d->online = i < 10 ? 0 : 1;
    if (i == 0) strcpy(d->nick, "keep me");
  }
  uint8_t extra[6] = {0x02, 9, 9, 9, 9, 9};
  Device *e = seen(full, extra, 0x0a0000ff, 999);
  CHECK(e && full.n == MAX_DEV && byMac(full, extra) == e);
  uint8_t first[6] = {0x02, 0, 0, 0, 0, 0}, second[6] = {0x02, 0, 0, 0, 0, 1};
  CHECK(byMac(full, first) != nullptr && byMac(full, second) == nullptr);

  // names: a better source wins, a worse one doesn't
  Device nd; memset(&nd, 0, sizeof nd);
  setName(nd, "android-3f2a", N_DNS); CHECK(!strcmp(nd.name, "android-3f2a"));
  setName(nd, "Pixel-8", N_MDNS); CHECK(!strcmp(nd.name, "Pixel-8"));
  setName(nd, "PIXEL", N_NETBIOS); CHECK(!strcmp(nd.name, "Pixel-8"));

  // ── what is it
  auto kind = [&](const char *mac, const char *name, const char *model, uint32_t ports) {
    Device d; memset(&d, 0, sizeof d); macParse(mac, d.mac); d.maker = ouiMaker(d.mac);
    strcpy(d.name, name); strcpy(d.model, model); d.ports = ports;
    return std::string(KIND_NAMES[guessKind(d, false, false)]);
  };
  CHECK(kind("3C:22:FB:00:00:01", "Keshavs-MacBook-Air", "", 0) == "computer");
  CHECK(kind("3C:22:FB:00:00:01", "Keshavs-iPhone", "", 0) == "phone");
  CHECK(kind("3E:22:FB:00:00:01", "", "", 0) == "phone");
  CHECK(kind("24:0A:C4:00:00:01", "", "", 0) == "smart");
  CHECK(kind("50:C7:BF:00:00:01", "", "", 0) == "router");
  CHECK(kind("A4:C1:38:00:00:01", "", "[TV] Samsung Q60", 0) == "tv");
  CHECK(kind("A4:C1:38:00:00:01", "", "", 1u << portIndex(9100)) == "printer");
  CHECK(kind("A4:C1:38:00:00:01", "DESKTOP-K2M8QJ", "", 0) == "computer");
  CHECK(kind("DC:A6:32:00:00:01", "", "", 0) == "computer");
  CHECK(kind("A4:C1:38:00:00:01", "", "", 0) == "unknown");
  { Device d; memset(&d, 0, sizeof d); CHECK(guessKind(d, true, false) == K_ROUTER && guessKind(d, false, true) == K_BOARD); }

  // ── packets
  uint8_t q[128];
  int n = buildPtrQuery(ipParse("192.168.1.7"), 0x1234, q, sizeof q);
  { int p = 12; char nm[80]; CHECK(n > 12 && dnsName(q, n, &p, nm, sizeof nm) && !strcmp(nm, "7.1.168.192.in-addr.arpa") && q[p + 1] == 12); }

  // a Bonjour reply (no question echoed, cache-flush class) and a router reply (question echoed, compressed)
  Bytes bon; u16(bon, 0x1234); u16(bon, 0x8400); u16(bon, 0); u16(bon, 1); u16(bon, 0); u16(bon, 0);
  label(bon, "7.1.168.192.in-addr.arpa"); bon.push_back(0); u16(bon, 12); u16(bon, 0x8001); u16(bon, 0); u16(bon, 120);
  Bytes rd; label(rd, "Keshavs-MacBook-Air.local"); rd.push_back(0);
  u16(bon, rd.size()); bon.insert(bon.end(), rd.begin(), rd.end());
  CHECK(parsePtrReply(bon.data(), bon.size(), 0x1234, s, sizeof s) && !strcmp(s, "Keshavs-MacBook-Air"));
  CHECK(!parsePtrReply(bon.data(), bon.size(), 0x9999, s, sizeof s));       // someone else's answer
  CHECK(!parsePtrReply(bon.data(), 30, 0x1234, s, sizeof s));               // cut short
  Bytes rt(q, q + n); rt[2] = 0x81; rt[3] = 0x80; rt[7] = 1;
  u16(rt, 0xC00C); u16(rt, 12); u16(rt, 1); u16(rt, 0); u16(rt, 60);
  Bytes rd2; label(rd2, "android_3f2a.lan"); rd2.push_back(0);
  u16(rt, rd2.size()); rt.insert(rt.end(), rd2.begin(), rd2.end());
  CHECK(parsePtrReply(rt.data(), rt.size(), 0x1234, s, sizeof s) && !strcmp(s, "android-3f2a"));
  Bytes nx(q, q + n); nx[2] = 0x81; nx[3] = 0x83;                            // NXDOMAIN: nothing
  CHECK(!parsePtrReply(nx.data(), nx.size(), 0x1234, s, sizeof s));
  Bytes loop; u16(loop, 0x1234); u16(loop, 0x8400); u16(loop, 0); u16(loop, 1); u16(loop, 0); u16(loop, 0); u16(loop, 0xC00C);
  CHECK(!parsePtrReply(loop.data(), loop.size(), 0x1234, s, sizeof s));      // a pointer to itself

  n = buildNbstat(0x4242, q, sizeof q);
  CHECK(n == 50 && q[13] == 'C' && q[14] == 'K' && q[47] == 0x21);
  Bytes nb; u16(nb, 0x4242); u16(nb, 0x8400); u16(nb, 0); u16(nb, 1); u16(nb, 0); u16(nb, 0);
  nb.insert(nb.end(), q + 12, q + 46); u16(nb, 0x21); u16(nb, 1); u16(nb, 0); u16(nb, 0);
  auto entry = [&](Bytes &x, const char *name, uint8_t suffix, uint16_t flags) {
    char pad[16]; snprintf(pad, sizeof pad, "%-15s", name); x.insert(x.end(), pad, pad + 15); x.push_back(suffix); u16(x, flags); };
  Bytes names; names.push_back(3);
  entry(names, "WORKGROUP", 0x00, 0x8400); entry(names, "DESKTOP-K2M8QJ", 0x20, 0x0400); entry(names, "DESKTOP-K2M8QJ", 0x00, 0x0400);
  u16(nb, names.size()); nb.insert(nb.end(), names.begin(), names.end());
  CHECK(parseNbstat(nb.data(), nb.size(), 0x4242, s, sizeof s) && !strcmp(s, "DESKTOP-K2M8QJ"));
  CHECK(!parseNbstat(nb.data(), 40, 0x4242, s, sizeof s));

  // ── UPnP, web titles
  const char *xml = "<root><device><friendlyName>[TV] Living Room</friendlyName>\n<manufacturer>Samsung Electronics</manufacturer><modelName>QA55Q60</modelName></device></root>";
  char fr[40], mk[40], md[40];
  CHECK(xmlTag(xml, "friendlyName", fr, sizeof fr) && xmlTag(xml, "manufacturer", mk, sizeof mk) && xmlTag(xml, "modelName", md, sizeof md));
  describeModel(mk, fr, md, s, sizeof s); CHECK(!strcmp(s, "[TV] Living Room"));
  describeModel("TP-Link", "", "Archer C6", s, sizeof s); CHECK(!strcmp(s, "TP-Link Archer C6"));
  describeModel("TP-Link", "", "TP-Link Deco", s, sizeof s); CHECK(!strcmp(s, "TP-Link Deco"));
  CHECK(!xmlTag(xml, "serialNumber", s, sizeof s));
  CHECK(ssdpLocation("HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\nLocation: http://192.168.1.1:1900/gatedesc.xml\r\n\r\n", s, sizeof s) &&
        !strcmp(s, "http://192.168.1.1:1900/gatedesc.xml"));
  CHECK(!ssdpLocation("HTTP/1.1 200 OK\r\nSERVER: x\r\n\r\n", s, sizeof s));
  CHECK(htmlTitle("<html><HEAD><Title>\n  Opal   Router\n </title>", s, sizeof s) && !strcmp(s, "Opal Router"));
  CHECK(!htmlTitle("<html><body>no title</body>", s, sizeof s));
  CHECK(!htmlTitle("<titles>x</titles>", s, sizeof s));

  // ── Bonjour browsing: a printer's reply (PTR + SRV + A, compressed) and a Mac's
  { uint8_t bq[1500]; n = buildBrowse(0x4d44, bq, sizeof bq);
    CHECK(n > 12 && n < 1400 && (bq[4] << 8 | bq[5]) == SERVICE_COUNT);
    int p = 12; char nm[80]; CHECK(dnsName(bq, n, &p, nm, sizeof nm) && !strcmp(nm, "_ipp._tcp.local") && bq[p + 1] == 12 && bq[p + 2] == 0x80);
    CHECK(buildBrowse(1, bq, 100) == 0); }
  Bytes br; u16(br, 0); u16(br, 0x8400); u16(br, 0); u16(br, 1); u16(br, 0); u16(br, 2);
  const size_t svcAt = br.size();
  label(br, "_ipp._tcp.local"); br.push_back(0); u16(br, 12); u16(br, 1); u16(br, 0); u16(br, 120);
  Bytes inst; inst.push_back(37); { const char *t = "HP Smart Tank 580-590 series [8E0396]"; inst.insert(inst.end(), t, t + 37); }
  inst.push_back(0xC0); inst.push_back(svcAt);                              // "… ._ipp._tcp.local" by pointer
  u16(br, inst.size()); const size_t instAt = br.size(); br.insert(br.end(), inst.begin(), inst.end());
  br.push_back(0xC0); br.push_back(instAt); u16(br, 33); u16(br, 0x8001); u16(br, 0); u16(br, 120);   // SRV
  Bytes sv; u16(sv, 0); u16(sv, 0); u16(sv, 631); label(sv, "HP7C4D8F8E0396.local"); sv.push_back(0);
  u16(br, sv.size()); const size_t hostAt = br.size() + 6; br.insert(br.end(), sv.begin(), sv.end());
  br.push_back(0xC0); br.push_back(hostAt); u16(br, 1); u16(br, 0x8001); u16(br, 0); u16(br, 120); u16(br, 4);   // A
  br.push_back(192); br.push_back(168); br.push_back(68); br.push_back(52);
  Found fd[4];
  CHECK(parseBrowse(br.data(), br.size(), fd, 4) == 1);
  CHECK(fd[0].ip == ipParse("192.168.68.52") && !strcmp(fd[0].host, "HP7C4D8F8E0396") &&
        !strcmp(fd[0].instance, "HP Smart Tank 580-590 series [8E0396]") && hasService(fd[0].svcs, "_ipp._tcp"));
  { Device pd; memset(&pd, 0, sizeof pd); applyFound(pd, fd[0]);
    CHECK(!strcmp(pd.name, "HP Smart Tank 580-590 series") && pd.nameSrc == N_BONJOUR && !strcmp(pd.model, pd.name));
    CHECK(guessKind(pd, false, false) == K_PRINTER);
    setName(pd, "HP7C4D8F8E0396", N_MDNS); CHECK(!strcmp(pd.name, "HP Smart Tank 580-590 series")); }
  CHECK(parseBrowse(br.data(), 40, fd, 4) == 0);                            // cut short: nothing made up
  { Device w; memset(&w, 0, sizeof w); w.svcs = 1u << serviceIndex("_workstation._tcp"); CHECK(guessKind(w, false, false) == K_COMPUTER);
    w.svcs = 1u << serviceIndex("_googlecast._tcp"); CHECK(guessKind(w, false, false) == K_TV);
    w.svcs = 1u << serviceIndex("_hap._tcp"); CHECK(guessKind(w, false, false) == K_SMART); }
  CHECK(serviceIndex("_IPP._tcp.local") == 0 && serviceIndex("_ippx._tcp.local") == -1);

  // ── wake
  uint8_t mp[102];
  CHECK(buildMagic(a, mp) == 102 && mp[0] == 0xff && mp[5] == 0xff && !memcmp(mp + 6, a, 6) && !memcmp(mp + 96, a, 6));

  // ── latency
  static Ring r; r.clear();
  for (int i = 0; i < 10; i++) r.add(i % 5 == 4 ? -1 : 10 + i);
  Stats st = stats(r, 300);
  CHECK(st.count == 10 && st.lossPct == 20 && st.min == 10 && st.max == 18 && st.avg == 14);
  for (int i = 0; i < 400; i++) r.add(20);
  st = stats(r, 60);
  CHECK(r.count == RING && st.count == 60 && st.min == 20 && st.max == 20 && st.jitter == 0 && st.lossPct == 0);
  CHECK(r.at(RING - 1) == 20);
  r.clear(); r.add(10); r.add(30); r.add(10);
  CHECK(stats(r, 10).jitter == 20);

  // ── CSV
  csvField("Living room, TV", s, sizeof s); CHECK(!strcmp(s, "\"Living room, TV\""));
  csvField("say \"hi\"", s, sizeof s); CHECK(!strcmp(s, "\"say \"\"hi\"\"\""));
  csvField("=HYPERLINK(1)", s, sizeof s); CHECK(!strcmp(s, "\"'=HYPERLINK(1)\""));
  csvField("plain", s, sizeof s); CHECK(!strcmp(s, "plain"));

  printf("%d passed, %d failed\n", passes, fails);
  if (!fails) printf("ALL PASS\n");
  return fails ? 1 : 0;
}
