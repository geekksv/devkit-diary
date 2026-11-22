// The board's game, served from a laptop: the same tambola.h and the same page, behind a few
// lines of plain HTTP. For trying the game without flashing, and for the end-to-end tests.
//
//   c++ -std=c++17 -I../src/tambola devserver.cpp -o devserver && ./devserver 8090
//   then open http://localhost:8090 on this computer (or this computer's address on a phone)
//
// TAMBOLA_INJECT=file.js adds a script to the page (the tests use it to press the page's buttons).
#define PROGMEM
#include "tambola.h"
#include "tambola_page.h"
#include <random>
#include <string>
#include <map>
#include <chrono>
#include <fstream>
#include <sstream>
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
  tRand = devRand;
  const int port = argc > 1 ? atoi(argv[1]) : 8090;
  std::string page = TAMBOLA_BODY;
  if (const char *inj = getenv("TAMBOLA_INJECT")) {
    std::ifstream f(inj);
    std::stringstream ss;
    ss << f.rdbuf();
    const size_t at = page.rfind("</body>");
    page.insert(at, "<script>" + ss.str() + "</script>");
  }
  const int s = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (sockaddr *)&a, sizeof a) || listen(s, 64)) { perror("bind"); return 1; }
  printf("tambola on http://localhost:%d\n", port);
  fflush(stdout);
  static char buf[65536], out[8192];
  for (;;) {
    tick(ms());
    fd_set r;
    FD_ZERO(&r);
    FD_SET(s, &r);
    timeval tv = {0, 50000};
    if (select(s + 1, &r, nullptr, nullptr, &tv) <= 0) continue;
    const int c = accept(s, nullptr, nullptr);
    if (c < 0) continue;
    timeval to = {2, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    std::string req;
    ssize_t n;
    while ((n = read(c, buf, sizeof buf)) > 0) {
      req.append(buf, n);
      const size_t he = req.find("\r\n\r\n");
      if (he == std::string::npos) continue;
      size_t len = 0;
      const size_t cl = req.find("Content-Length:");
      if (cl != std::string::npos && cl < he) len = strtoul(req.c_str() + cl + 15, nullptr, 10);
      if (req.size() >= he + 4 + len) break;
    }
    const size_t sp1 = req.find(' '), sp2 = req.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) { close(c); continue; }
    std::string target = req.substr(sp1 + 1, sp2 - sp1 - 1), path = target, query;
    const size_t qm = target.find('?');
    if (qm != std::string::npos) { path = target.substr(0, qm); query = target.substr(qm + 1); }
    std::map<std::string, std::string> args;
    parse(query, args);
    const size_t he = req.find("\r\n\r\n");
    if (he != std::string::npos) parse(req.substr(he + 4), args);
    if (path == "/") send(c, "text/html; charset=utf-8", page);
    else {
      Out o = {out, sizeof out, 0};
      auto get = [&](const char *k) -> const char * { auto i = args.find(k); return i == args.end() ? "" : i->second.c_str(); };
      route(path.c_str(), get, ms(), o);
      send(c, "application/json", std::string(out, o.n));
    }
    close(c);
  }
}
