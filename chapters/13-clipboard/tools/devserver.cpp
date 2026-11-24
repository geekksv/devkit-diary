// The board's clipboard, served from a laptop: the same clip.h, the same page, helper and
// installers, behind a few lines of plain HTTP, with the sealed items kept in memory.
//
//   c++ -std=c++17 -I../src/clip devserver.cpp -o devserver && ./devserver 8093
//   then open http://localhost:8093 (browsers treat localhost as secure, so the page works), and
//   point helpers at it: DEVKIT_CLIP_HOSTS=localhost:8093 python3 ../helper/devkit_clip.py --setup
//
// CLIP_INJECT=file.js puts a script at the top of the page, for the end-to-end tests.
#define PROGMEM
#include "clip.h"
#include "clip_page.h"
#include "files.h"
#include <random>
#include <string>
#include <map>
#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>

static std::mt19937 gen(std::random_device{}());
static uint32_t devRand() { return gen(); }
static uint32_t ms() {
  static auto t0 = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() + 1000;
}

struct MemStore : Store {
  std::map<uint32_t, std::string> files;
  int read(uint32_t id, uint8_t *buf, int max) override {
    auto i = files.find(id);
    if (i == files.end()) return -1;
    const int n = (int)i->second.size() < max ? (int)i->second.size() : max;
    memcpy(buf, i->second.data(), n);
    return n;
  }
  void remove(uint32_t id) override { files.erase(id); }
  void removeAll() override { files.clear(); }
};
static MemStore mem;

static std::string decode(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
    else o += s[i];
  }
  return o;
}
static void parse(const std::string &q, std::map<std::string, std::string> &m) {
  size_t a = 0;
  while (a < q.size()) {
    size_t e = q.find('&', a);
    if (e == std::string::npos) e = q.size();
    const std::string kv = q.substr(a, e - a);
    const size_t eq = kv.find('=');
    if (eq != std::string::npos) m[decode(kv.substr(0, eq))] = decode(kv.substr(eq + 1));
    a = e + 1;
  }
}
static void send(int c, const char *type, const std::string &body) {
  std::string h = "HTTP/1.1 200 OK\r\nContent-Type: " + std::string(type) + "\r\nContent-Length: " + std::to_string(body.size()) +
                  "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  h += body;
  size_t off = 0;
  while (off < h.size()) { const ssize_t w = write(c, h.data() + off, h.size() - off); if (w <= 0) break; off += w; }
}

int main(int argc, char **argv) {
  cRand = devRand;
  store = &mem;
  resetAll();
  const int port = argc > 1 ? atoi(argv[1]) : 8093;
  const std::string hosts = "localhost:" + std::to_string(port);
  std::string page = CLIP_PAGE;
  if (const char *inj = getenv("CLIP_INJECT")) {
    std::ifstream f(inj);
    std::stringstream ss;
    ss << f.rdbuf();
    page.insert(page.find("<style>"), "<script>" + ss.str() + "</script>");
  }
  auto withHosts = [&](std::string s) { size_t p; while ((p = s.find("%%HOSTS%%")) != std::string::npos) s.replace(p, 9, hosts); return s; };
  const std::map<std::string, std::pair<const char *, std::string>> files = {
      {"/devkit_clip.py", {"text/x-python; charset=utf-8", CLIP_PY}}, {"/clipcrypto.py", {"text/x-python; charset=utf-8", CRYPTO_PY}},
      {"/i", {"text/plain; charset=utf-8", withHosts(INSTALL_SH)}}, {"/u", {"text/plain; charset=utf-8", UNINSTALL_SH}},
      {"/i.ps1", {"text/plain; charset=utf-8", withHosts(INSTALL_PS1)}}, {"/u.ps1", {"text/plain; charset=utf-8", UNINSTALL_PS1}}};

  const int s = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (sockaddr *)&a, sizeof a) || listen(s, 64)) { perror("bind"); return 1; }
  printf("clipboard on http://localhost:%d\n", port);
  fflush(stdout);
  static char buf[65536], out[REPLY_MAX];
  uint32_t lastTick = 0;
  for (;;) {
    if (ms() - lastTick >= 200) { lastTick = ms(); tick(lastTick, (uint32_t)time(nullptr)); }
    fd_set r;
    FD_ZERO(&r);
    FD_SET(s, &r);
    timeval tv = {0, 50000};
    if (select(s + 1, &r, nullptr, nullptr, &tv) <= 0) continue;
    const int c = accept(s, nullptr, nullptr);
    if (c < 0) continue;
    timeval to = {5, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    std::string req;
    ssize_t n;
    size_t he = std::string::npos, len = 0;
    while ((n = read(c, buf, sizeof buf)) > 0) {
      req.append(buf, n);
      he = req.find("\r\n\r\n");
      if (he == std::string::npos) continue;
      const size_t cl = req.find("Content-Length:");
      len = cl != std::string::npos && cl < he ? strtoul(req.c_str() + cl + 15, nullptr, 10) : 0;
      if (req.size() >= he + 4 + len) break;
    }
    const size_t sp1 = req.find(' '), sp2 = req.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos || he == std::string::npos) { close(c); continue; }
    std::string target = req.substr(sp1 + 1, sp2 - sp1 - 1), path = target, query;
    const size_t qm = target.find('?');
    if (qm != std::string::npos) { path = target.substr(0, qm); query = target.substr(qm + 1); }
    const std::string body = req.substr(he + 4);
    std::map<std::string, std::string> args;
    parse(query, args);
    auto get = [&](const char *k) -> const char * { auto i = args.find(k); return i == args.end() ? "" : i->second.c_str(); };
    const uint32_t tok = (uint32_t)strtoul(get("t"), nullptr, 16);
    if (path == "/") send(c, "text/html; charset=utf-8", page);
    else if (path == "/__report") { printf("%s\n", body.c_str()); fflush(stdout); send(c, "text/plain", "ok"); }
    else if (files.count(path)) send(c, files.at(path).first, files.at(path).second);
    else if (path == "/devkit_clip.py.gz") send(c, "application/gzip", std::string((const char *)CLIP_PY_GZ, CLIP_PY_GZ_LEN));
    else if (path == "/clipcrypto.py.gz") send(c, "application/gzip", std::string((const char *)CRYPTO_PY_GZ, CRYPTO_PY_GZ_LEN));
    else if (path == "/put") {                    // the sealed bytes are the body
      Device *d = deviceByToken(tok);
      const int flags = atoi(get("f")), to = atoi(get("to"));
      const PutResult p = d ? canPut(*d, body.size(), to, flags) : P_NOTO;
      if (!d) send(c, "application/json", "{\"ok\":false,\"err\":\"notin\"}");
      else if (p != P_OK) send(c, "application/json", std::string("{\"ok\":false,\"err\":\"") + PUT_TEXT[p] + "\"}");
      else {
        const uint32_t id = nextItemId();
        mem.files[id] = body;
        d->seen = ms();
        Item *it = addItem(*d, body.size(), flags, to, ms(), (uint32_t)time(nullptr));
        if (!it) { mem.files.erase(id); send(c, "application/json", "{\"ok\":false,\"err\":\"full\"}"); }
        else send(c, "application/json", "{\"ok\":true,\"id\":" + std::to_string(it->id) + "}");
      }
    } else if (path == "/blob") {
      const uint32_t id = (uint32_t)strtoul(get("id"), nullptr, 10);
      if (blobFor(tok, id, ms()) < 0) send(c, "application/json", "{\"ok\":false,\"err\":\"gone\"}");
      else { send(c, "application/octet-stream", mem.files[id]); taken(tok, id); }
    } else {
      parse(body, args);
      Out o = {out, sizeof out, 0};
      out[0] = 0;
      route(path.c_str(), get, ms(), (uint32_t)time(nullptr), o);
      send(c, "application/json", std::string(out, o.n));
    }
    close(c);
  }
}
