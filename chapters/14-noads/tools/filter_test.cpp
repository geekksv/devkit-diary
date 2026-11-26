// The ad blocker's logic on a laptop:
//   c++ -std=c++17 -O2 -I../src/noads filter_test.cpp -o t && ./t [blocklist.txt ...]
// With list files given (for example HaGeZi's light and multi lists), it also builds each one the
// way the board does and checks every name in it.
#include "builder.h"
#include <chrono>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ── a laptop "flash": files under a temporary folder ────────────────────────
struct DiskFS {
  std::string root;
  std::string at(const char *p) { return root + p; }
  bool append(const char *p, const uint8_t *d, size_t n) {
    FILE *f = fopen(at(p).c_str(), "ab");
    if (!f) return false;
    const bool ok = fwrite(d, 1, n, f) == n;
    fclose(f);
    return ok;
  }
  int read(const char *p, uint32_t off, uint8_t *b, size_t n) {
    FILE *f = fopen(at(p).c_str(), "rb");
    if (!f) return -1;
    fseek(f, off, SEEK_SET);
    const int got = (int)fread(b, 1, n, f);
    fclose(f);
    return got;
  }
  template <class F> bool scan(const char *p, F fn) {
    FILE *f = fopen(at(p).c_str(), "rb");
    if (!f) return false;
    uint8_t b[1536];
    size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) fn(b, n);
    fclose(f);
    return true;
  }
  void remove(const char *p) { ::remove(at(p).c_str()); }
  long size(const char *p) { struct stat s; return stat(at(p).c_str(), &s) ? -1 : (long)s.st_size; }
};

struct FileReader {
  DiskFS *fs;
  const char *path;
  int operator()(uint32_t off, uint8_t *b, size_t n) const { return fs->read(path, off, b, n); }
};

// Builds a list from text through the board's Builder, returns the loaded Blocklist.
static long buildFrom(DiskFS &fs, const std::string &text, Blocklist<FileReader> &bl, size_t chunk = 1460) {
  static Builder<DiskFS> *b = nullptr;
  delete b;
  b = new Builder<DiskFS>(fs);
  b->begin();
  for (size_t i = 0; i < text.size(); i += chunk)
    b->feed((const uint8_t *)text.data() + i, std::min(chunk, text.size() - i));
  const long n = b->finish(1700000000);
  if (n < 0) return n;
  fs.remove("/list.bin");
  rename(fs.at("/list.new").c_str(), fs.at("/list.bin").c_str());
  bl.read = FileReader{&fs, "/list.bin"};
  bl.load((uint32_t)fs.size("/list.bin"));
  return n;
}

// A DNS query for name/type, as a phone would send it (with an EDNS record at the end).
static std::vector<uint8_t> query(const char *name, uint16_t type, uint16_t id = 0x1234) {
  std::vector<uint8_t> q = {(uint8_t)(id >> 8), (uint8_t)id, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 1};
  for (const char *p = name; *p;) {
    const char *dot = strchr(p, '.');
    const int l = dot ? (int)(dot - p) : (int)strlen(p);
    q.push_back((uint8_t)l);
    q.insert(q.end(), p, p + l);
    p += l + (dot ? 1 : 0);
  }
  q.push_back(0);
  q.push_back(type >> 8); q.push_back((uint8_t)type);
  q.push_back(0); q.push_back(1);
  const uint8_t opt[] = {0, 0, 41, 0x10, 0, 0, 0, 0, 0, 0, 0};
  q.insert(q.end(), opt, opt + sizeof opt);
  return q;
}

int main(int argc, char **argv) {
  // ── names ──
  char nm[300];
  CHECK(normalizeName("Ads.Example.COM.", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(!normalizeName("", nm, sizeof nm) && !normalizeName("a b.com", nm, sizeof nm) && !normalizeName(".x.com", nm, sizeof nm));
  CHECK(fingerprint("ads.example.com") == fingerprint("ADS.example.com"));
  CHECK(fingerprint("ads.example.com") != fingerprint("ads.example.co"));
  CHECK(fingerprint("ads.example.com") < (1ULL << 48));
  CHECK(fingerprint("x.example.com", 1) == fingerprint("x"));

  // ── your lists ──
  NameList allow = {}, block = {};
  CHECK(allow.add("Shop.Example.com") && allow.add("shop.example.com") && allow.n == 1);
  CHECK(!allow.add("com") && !allow.add("bad name.com"));
  CHECK(allow.has("shop.example.com") && allow.has("img.shop.example.com"));
  CHECK(!allow.has("example.com") && !allow.has("xshop.example.com"));
  CHECK(allow.remove("SHOP.example.com") && allow.n == 0 && !allow.remove("nope.com"));
  for (int i = 0; i < USER_NAMES; i++) { snprintf(nm, sizeof nm, "s%d.example.com", i); CHECK(block.add(nm)); }
  CHECK(!block.add("one-more.example.com"));
  block = {};

  // ── deciding ──
  std::set<uint64_t> big = {fingerprint("doubleclick.net"), fingerprint("ads.example.com")};
  auto has = [&](uint64_t f) { return big.count(f) > 0; };
  CHECK(decide("doubleclick.net", allow, block, has, false) == BLOCK_LIST);
  CHECK(decide("stats.g.doubleclick.net", allow, block, has, false) == BLOCK_LIST);
  CHECK(decide("ads.example.com", allow, block, has, false) == BLOCK_LIST);
  CHECK(decide("x.ads.example.com", allow, block, has, false) == BLOCK_LIST);
  CHECK(decide("example.com", allow, block, has, false) == ALLOW);
  CHECK(decide("notdoubleclick.net", allow, block, has, false) == ALLOW);
  CHECK(decide("google.com", allow, block, has, false) == ALLOW);
  big.insert(fingerprint("com"));
  CHECK(decide("google.com", allow, block, has, false) == ALLOW);       // never a bare "com"
  allow.add("ads.example.com");
  CHECK(decide("x.ads.example.com", allow, block, has, false) == ALLOW_YOURS);
  block.add("tiktok.com");
  CHECK(decide("www.tiktok.com", allow, block, has, false) == BLOCK_YOURS);
  CHECK(decide("doubleclick.net", allow, block, has, true) == ALLOW);   // paused
  CHECK(decide("www.tiktok.com", allow, block, has, true) == BLOCK_YOURS);   // yours still applies
  CHECK(decide("use-application-dns.net", allow, block, has, false) == BLOCK_SPECIAL);

  // ── packets ──
  Question q;
  auto pk = query("Ads.Example.com", T_A, 0xBEEF);
  CHECK(parseQuery(pk.data(), pk.size(), q) && !strcmp(q.name, "ads.example.com") && q.type == T_A && q.id == 0xBEEF);
  uint8_t out[DNS_MAX];
  int n = blockedAnswer(pk.data(), q, out);
  CHECK(n == q.end + 16 && rd16(out) == 0xBEEF && (rd16(out + 2) & 0x8000) && (rd16(out + 2) & 0xF) == 0);
  CHECK(rd16(out + 4) == 1 && rd16(out + 6) == 1 && rd16(out + 10) == 0);
  CHECK(out[n - 4] == 0 && out[n - 1] == 0 && rd16(out + n - 6) == 4);   // 0.0.0.0
  CHECK(answerTtl(out, n) == 60);
  pk = query("ads.example.com", T_AAAA);
  parseQuery(pk.data(), pk.size(), q);
  n = blockedAnswer(pk.data(), q, out);
  CHECK(n == q.end + 28 && rd16(out + n - 18) == 16);                      // ::
  pk = query("ads.example.com", T_HTTPS);
  parseQuery(pk.data(), pk.size(), q);
  n = blockedAnswer(pk.data(), q, out);
  CHECK(n == q.end && rd16(out + 6) == 0 && (rd16(out + 2) & 0xF) == 0);   // empty answer
  pk = query("use-application-dns.net", T_A);
  parseQuery(pk.data(), pk.size(), q);
  n = blockedAnswer(pk.data(), q, out, true);
  CHECK((rd16(out + 2) & 0xF) == 3 && rd16(out + 6) == 0);                 // NXDOMAIN
  // Not questions: a response, two questions, a pointer in the name, too short.
  pk = query("x.com", T_A);
  pk[2] |= 0x80;
  CHECK(!parseQuery(pk.data(), pk.size(), q));
  pk = query("x.com", T_A);
  pk[5] = 2;
  CHECK(!parseQuery(pk.data(), pk.size(), q));
  pk = query("x.com", T_A);
  pk[12] = 0xC0;
  CHECK(!parseQuery(pk.data(), pk.size(), q));
  CHECK(!parseQuery(pk.data(), 10, q));
  // Random bytes never crash the parser, and whatever parses gives a sane name.
  std::mt19937 rng(9);
  int parsed = 0;
  for (int i = 0; i < 200000; i++) {
    std::vector<uint8_t> r(rng() % 80);
    for (auto &x : r) x = (uint8_t)rng();
    if (r.size() > 12) { r[2] &= 0x07; r[4] = 0; r[5] = 1; }
    if (parseQuery(r.data(), r.size(), q)) {
      parsed++;
      CHECK(strlen(q.name) <= NAME_MAX_LEN && q.end <= (int)r.size());
      blockedAnswer(r.data(), q, out);
    }
  }
  printf("random packets: %d of 200000 parsed as questions, none crashed\n", parsed);

  // ── the cache ──
  Cache *cache = new Cache();
  pk = query("example.com", T_A, 7);
  parseQuery(pk.data(), pk.size(), q);
  // an upstream answer: one A record, TTL 300
  std::vector<uint8_t> ans(pk.begin(), pk.begin() + q.end);
  ans[2] = 0x81; ans[3] = 0x80; ans[7] = 1; ans[11] = 0;
  const uint8_t rr[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 1, 0x2C, 0, 4, 93, 184, 216, 34};
  ans.insert(ans.end(), rr, rr + sizeof rr);
  CHECK(answerTtl(ans.data(), ans.size()) == 300);
  cache->put(q, ans.data(), ans.size(), 1000);
  auto pk2 = query("example.com", T_A, 99);
  Question q2;
  parseQuery(pk2.data(), pk2.size(), q2);
  n = cache->get(q2, 2000, out);
  CHECK(n == (int)ans.size() && rd16(out) == 99 && !memcmp(out + 2, ans.data() + 2, n - 2));
  CHECK(cache->get(q2, 1000 + 300000, out) == 0);                     // expired
  pk2 = query("example.com", T_AAAA);
  parseQuery(pk2.data(), pk2.size(), q2);
  CHECK(cache->get(q2, 2000, out) == 0);                              // another type
  ans[3] = 0x82;                                                      // SERVFAIL: not kept
  parseQuery(pk.data(), pk.size(), q);
  cache->clear();
  cache->put(q, ans.data(), ans.size(), 1000);
  CHECK(cache->get(q, 1500, out) == 0);
  // full: the least recently used goes
  ans[3] = 0x80;
  for (int i = 0; i < CACHE + 10; i++) {
    snprintf(nm, sizeof nm, "n%d.com", i);
    auto p3 = query(nm, T_A);
    Question q3;
    parseQuery(p3.data(), p3.size(), q3);
    cache->put(q3, ans.data(), ans.size(), 5000 + i);
  }
  int kept = 0;
  for (int i = 0; i < CACHE + 10; i++) {
    snprintf(nm, sizeof nm, "n%d.com", i);
    auto p3 = query(nm, T_A);
    Question q3;
    parseQuery(p3.data(), p3.size(), q3);
    kept += cache->get(q3, 6000, out) > 0;
  }
  CHECK(kept == CACHE);
  delete cache;

  // ── statistics ──
  Stats *st = new Stats();
  memset(st, 0, sizeof *st);
  for (int i = 0; i < 500; i++) record(*st, i % 3 ? "doubleclick.net" : "ads.example.com", 0x0A04A8C0, BLOCK_LIST, false, 1000 + i);
  for (int i = 0; i < 300; i++) { snprintf(nm, sizeof nm, "rare%d.com", i); record(*st, nm, 0x0B04A8C0, BLOCK_LIST, false, 2000); }
  record(*st, "google.com", 0x0B04A8C0, ALLOW, true, 3000);
  record(*st, "github.com", 0x0B04A8C0, ALLOW, false, 3000);
  CHECK(st->total == 802 && st->blocked == 800 && st->cached == 1 && st->forwarded == 1);
  static char jb[16384];
  Out o = {jb, sizeof jb, 0};
  o.add("{");
  statsJson(o, *st, 4000);
  o.add("}");
  const std::string js(jb, o.n);
  CHECK(js.find("[\"doubleclick.net\",") < js.find("[\"ads.example.com\","));   // heavy hitters stay, in order
  CHECK(js.find("\"192.168.4.10\",500,500") != std::string::npos);
  CHECK(js.find("\"google.com\",\"192.168.4.11\",9") != std::string::npos);
  CHECK(o.n < (int)sizeof jb - 1);
  // the per-minute counts move on with time
  record(*st, "x.com", 1, ALLOW, false, 60000 * 5 + 10);
  o = {jb, sizeof jb, 0};
  statsJson(o, *st, 60000 * 5 + 20);
  CHECK(std::string(jb, o.n).find("[0,0],[1,0]]") != std::string::npos);
  delete st;

  // ── list lines ──
  CHECK(parseListLine("ads.example.com", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(parseListLine("0.0.0.0 Ads.Example.com # comment", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(parseListLine("127.0.0.1\tads.example.com\r", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(parseListLine("||ads.example.com^", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(parseListLine("*.ads.example.com", nm, sizeof nm) && !strcmp(nm, "ads.example.com"));
  CHECK(!parseListLine("# a comment", nm, sizeof nm) && !parseListLine("   ", nm, sizeof nm));
  CHECK(!parseListLine("0.0.0.0 localhost", nm, sizeof nm) && !parseListLine("127.0.0.1 localhost.localdomain", nm, sizeof nm));
  CHECK(!parseListLine("||ads.example.com^$third-party", nm, sizeof nm));
  CHECK(!parseListLine("192.168.1.1 router.lan", nm, sizeof nm));
  CHECK(!parseListLine("ads.example.com/path", nm, sizeof nm));
  CHECK(!parseListLine("[Adblock Plus 2.0]", nm, sizeof nm) && !parseListLine("! comment", nm, sizeof nm));

  // ── building and finding, small ──
  char dir[] = "/tmp/noadsXXXXXX";
  DiskFS fs{mkdtemp(dir)};
  Blocklist<FileReader> bl;
  std::string text = "# test list\nads.example.com\n0.0.0.0 tracker.example.net\nADS.example.com\n\n||pixel.example.org^\n";
  CHECK(buildFrom(fs, text, bl) == 3 && bl.ok && bl.h.count == 3 && bl.h.built == 1700000000);
  CHECK(bl.has(fingerprint("ads.example.com")) && bl.has(fingerprint("tracker.example.net")) && bl.has(fingerprint("pixel.example.org")));
  CHECK(!bl.has(fingerprint("example.com")) && !bl.has(0) && !bl.has((1ULL << 48) - 1));
  auto blHas = [&](uint64_t f) { return bl.has(f); };
  NameList none = {};
  CHECK(decide("cdn.tracker.example.net", none, none, blHas, false) == BLOCK_LIST);
  // a list cut short (power cut while writing) is refused
  const long full = fs.size("/list.bin");
  {
    std::ifstream in(fs.at("/list.bin"), std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(in)), {});
    FILE *cf = fopen(fs.at("/cut.bin").c_str(), "wb");
    fwrite(data.data(), 1, data.size() - 3, cf);
    fclose(cf);
  }
  Blocklist<FileReader> cut;
  cut.read = FileReader{&fs, "/cut.bin"};
  CHECK(!cut.load((uint32_t)full - 3) && !cut.has(fingerprint("ads.example.com")));
  // nothing usable: no list
  CHECK(buildFrom(fs, "# only comments\n\n", bl) == -1);
  // every bucket file is cleaned up
  char bp[16];
  bool clean = true;
  for (int b = 0; b < FILE_BUCKETS; b++) { Builder<DiskFS>::bucketPath(b, bp); clean &= fs.size(bp) < 0; }
  CHECK(clean);
  // chunk boundaries in the middle of lines don't matter
  std::string many;
  for (int i = 0; i < 5000; i++) many += "host" + std::to_string(i) + ".ads.example\n";
  for (size_t ch : {1, 7, 1460, 100000}) {
    CHECK(buildFrom(fs, many, bl, ch) == 5000);
    bool all = true;
    for (int i = 0; i < 5000; i += 97) all &= bl.has(fingerprint(("host" + std::to_string(i) + ".ads.example").c_str()));
    CHECK(all);
  }

  // ── the real lists, if given ──
  for (int a = 1; a < argc; a++) {
    std::ifstream in(argv[a], std::ios::binary);
    std::string list((std::istreambuf_iterator<char>(in)), {});
    std::vector<std::string> names;
    {
      size_t p = 0;
      while (p < list.size()) {
        size_t e = list.find('\n', p);
        if (e == std::string::npos) e = list.size();
        if (parseListLine(list.substr(p, e - p).c_str(), nm, sizeof nm)) names.push_back(nm);
        p = e + 1;
      }
    }
    std::set<std::string> uniq(names.begin(), names.end());
    const auto t0 = std::chrono::steady_clock::now();
    const long got = buildFrom(fs, list, bl);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("%s: %zu names, %ld kept (%ld KB in flash), built in %.2f s\n", argv[a], uniq.size(), got,
           fs.size("/list.bin") / 1024, secs);
    CHECK(got >= (long)uniq.size() - 2 && got <= (long)uniq.size());   // two names sharing a fingerprint: practically never
    size_t found = 0;
    for (auto &nmx : uniq) found += bl.has(fingerprint(nmx.c_str()));
    CHECK(found == uniq.size());
    // Real sites that must never be blocked by accident: 2 million made-up names, counted.
    std::mt19937 r2(42);
    int falsePos = 0;
    const char *tld[] = {"com", "net", "org", "in", "io"};
    for (int i = 0; i < 2000000; i++) {
      snprintf(nm, sizeof nm, "site%u-%u.%s", (unsigned)r2(), (unsigned)r2(), tld[i % 5]);
      falsePos += bl.has(fingerprint(nm));
    }
    printf("  made-up names wrongly blocked: %d of 2,000,000\n", falsePos);
    CHECK(falsePos <= 1);
    // Common sites stay reachable.
    for (const char *ok : {"google.com", "www.google.com", "youtube.com", "www.youtube.com", "github.com",
                           "wikipedia.org", "en.wikipedia.org", "amazon.in", "flipkart.com", "whatsapp.net",
                           "web.whatsapp.com", "instagram.com", "netflix.com", "apple.com", "icloud.com",
                           "microsoft.com", "login.microsoftonline.com", "zoom.us", "paytm.com", "hdfcbank.com"}) {
      const Verdict v = decide(ok, none, none, blHas, false);
      if (v != ALLOW) printf("  blocked a site people need: %s\n", ok);
      CHECK(v == ALLOW);
    }
    for (const char *ad : {"doubleclick.net", "googleadservices.com", "pagead2.googlesyndication.com", "adservice.google.com"}) {
      const Verdict v = decide(ad, none, none, blHas, false);
      printf("  %-32s %s\n", ad, v == BLOCK_LIST ? "blocked" : "allowed");
    }
    // how fast is a lookup from "flash"
    const auto t1 = std::chrono::steady_clock::now();
    int hits = 0;
    for (int i = 0; i < 20000; i++) hits += decide(names[(i * 7919) % names.size()].c_str(), none, none, blHas, false) != ALLOW;
    printf("  20,000 lookups: %.0f ms (all %d blocked)\n",
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count(), hits);
    CHECK(hits == 20000);
  }

  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
