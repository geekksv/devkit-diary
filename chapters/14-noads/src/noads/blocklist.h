#pragma once
// The big blocklist in flash: the file format, finding a fingerprint in it, and turning a
// downloaded list into that file. Nothing from Arduino here either: storage comes in through small
// functions, so tools/filter_test.cpp runs all of it on a laptop against ordinary files.
//
// The file, /list.bin:
//   `count` fingerprints of 6 bytes, big-endian, sorted, no repeats,
//   then a 16-byte footer: "NOAD", version 1, 6 (bytes a fingerprint), 2 spare, count (u32), built (u32, unix).
// The footer is at the end because it's written last: flash files can be added to cheaply, but
// changing their start means copying the whole file, and two copies of the list don't fit.
//
// Finding one: an index in memory holds the first fingerprint of every block of 512, so a lookup
// is a binary search in memory, then one 3 KB read from flash and a binary search in that.
//
// Building one, from a list of 100,000-200,000 names, on a board with ~200 KB of memory: each name's
// fingerprint goes into a bucket by its top bits; then each bucket (~1,500 names, ~12 KB as
// numbers) is sorted in memory and appended (builder.h). The buckets come out in order, so the whole file is sorted.

#include "filter.h"
#include <algorithm>
#include <vector>

static const uint32_t LIST_MAGIC = 0x44414F4E;   // "NOAD"
static const int FP_BYTES = 6, HEADER = 16, BLOCK = 512, BUCKETS = 128;

inline void putFp(uint8_t *p, uint64_t fp) {
  for (int i = FP_BYTES - 1; i >= 0; i--) { p[i] = (uint8_t)fp; fp >>= 8; }
}
inline uint64_t getFp(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 0; i < FP_BYTES; i++) v = v << 8 | p[i];
  return v;
}

struct ListHeader {
  uint32_t count, built;
};

inline void writeHeader(uint8_t *h, uint32_t count, uint32_t built) {
  memset(h, 0, HEADER);
  memcpy(h, &LIST_MAGIC, 4);
  h[4] = 1;
  h[5] = FP_BYTES;
  memcpy(h + 8, &count, 4);
  memcpy(h + 12, &built, 4);
}
inline bool readHeader(const uint8_t *h, ListHeader &out) {
  uint32_t m;
  memcpy(&m, h, 4);
  if (m != LIST_MAGIC || h[4] != 1 || h[5] != FP_BYTES) return false;
  memcpy(&out.count, h + 8, 4);
  memcpy(&out.built, h + 12, 4);
  return true;
}

// The loaded list: the index, and a way to read bytes from the file.
// read(offset, buf, n) returns how many bytes it read.
template <class R> struct Blocklist {
  R read;
  ListHeader h = {0, 0};
  std::vector<uint64_t> index;   // the first fingerprint of each block
  bool ok = false;

  // Checks the header and the file's length, and builds the index. False leaves it empty.
  bool load(uint32_t fileSize) {
    ok = false;
    index.clear();
    uint8_t hb[HEADER];
    if (fileSize < HEADER || read(fileSize - HEADER, hb, HEADER) != HEADER || !readHeader(hb, h)) return false;
    if ((uint64_t)HEADER + (uint64_t)h.count * FP_BYTES != fileSize) return false;
    const uint32_t blocks = (h.count + BLOCK - 1) / BLOCK;
    index.reserve(blocks);
    uint8_t fb[FP_BYTES];
    for (uint32_t b = 0; b < blocks; b++) {
      if (read((uint32_t)b * BLOCK * FP_BYTES, fb, FP_BYTES) != FP_BYTES) { index.clear(); return false; }
      index.push_back(getFp(fb));
    }
    ok = true;
    return true;
  }

  bool has(uint64_t fp) {
    if (!ok || index.empty() || fp < index[0]) return false;
    // the last block whose first fingerprint is <= fp
    const size_t b = std::upper_bound(index.begin(), index.end(), fp) - index.begin() - 1;
    const uint32_t first = (uint32_t)b * BLOCK;
    const uint32_t n = h.count - first < (uint32_t)BLOCK ? h.count - first : BLOCK;
    static uint8_t buf[BLOCK * FP_BYTES];
    if ((uint32_t)read(first * FP_BYTES, buf, n * FP_BYTES) != n * FP_BYTES) return false;
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
      const uint32_t mid = (lo + hi) / 2;
      const uint64_t v = getFp(buf + mid * FP_BYTES);
      if (v == fp) return true;
      if (v < fp) lo = mid + 1;
      else hi = mid;
    }
    return false;
  }
};

// ── turning a downloaded list into the file ─────────────────────────────────

// One line of a list, in any of the usual shapes, to a domain name. False for comments, blank
// lines, and lines that aren't a single domain:
//   "ads.example.com"                       plain
//   "0.0.0.0 ads.example.com" / "127.0.0.1 ..."   hosts file
//   "||ads.example.com^"                    adblock style (domain rules only)
//   "*.ads.example.com"                     wildcard
inline bool parseListLine(const char *line, char *out, int cap) {
  while (*line == ' ' || *line == '\t') line++;
  if (!*line || *line == '#' || *line == '!' || *line == '[') return false;
  char tok[2][NAME_MAX_LEN + 2];
  int nt = 0;
  for (const char *p = line; *p && nt < 2;) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#') break;
    int k = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '#' && *p != '\r' && *p != '\n') {
      if (k < NAME_MAX_LEN + 1) tok[nt][k++] = *p;
      p++;
    }
    tok[nt][k] = 0;
    if (k) nt++;
    while (*p == '\r' || *p == '\n') p++;
  }
  if (!nt) return false;
  const char *d = tok[0];
  if (nt == 2) {
    if (strcmp(tok[0], "0.0.0.0") && strcmp(tok[0], "127.0.0.1") && strcmp(tok[0], "::") && strcmp(tok[0], "::1"))
      return false;
    d = tok[1];
  }
  char tmp[NAME_MAX_LEN + 2];
  int n = 0;
  if (!strncmp(d, "||", 2)) d += 2;
  if (!strncmp(d, "*.", 2)) d += 2;
  for (; *d && *d != '^' && n < (int)sizeof tmp - 1; d++) tmp[n++] = *d;
  if (*d && *d != '^') return false;
  if (*d == '^' && d[1]) return false;                 // "^$third-party" and other options: not a plain domain
  tmp[n] = 0;
  if (!normalizeName(tmp, out, cap) || !strchr(out, '.')) return false;
  if (!strcmp(out, "localhost") || !strcmp(out, "localhost.localdomain") || !strcmp(out, "0.0.0.0")) return false;
  for (const char *c = out; *c; c++)
    if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '.' || *c == '_')) return false;
  return true;
}

inline int bucketOf(uint64_t fp) { return (int)(fp >> (FP_BYTES * 8 - 7)); }   // the top 7 bits: 0..127

// Sorts n fingerprints in place and drops repeats. Returns how many are left.
// A plain heapsort that only ever moves whole 8-byte numbers: the board keeps this array in memory
// that faults on byte access, and std::sort may copy with memmove, which works byte by byte.
inline uint32_t sortUnique(uint64_t *v, uint32_t n) {
  auto sift = [&](uint32_t root, uint32_t end) {
    const uint64_t x = v[root];
    while (2 * root + 1 < end) {
      uint32_t child = 2 * root + 1;
      if (child + 1 < end && v[child + 1] > v[child]) child++;
      if (v[child] <= x) break;
      v[root] = v[child];
      root = child;
    }
    v[root] = x;
  };
  for (uint32_t i = n / 2; i-- > 0;) sift(i, n);
  for (uint32_t end = n; end > 1; end--) {
    const uint64_t t = v[0];
    v[0] = v[end - 1];
    v[end - 1] = t;
    sift(0, end - 1);
  }
  uint32_t k = 0;
  for (uint32_t i = 0; i < n; i++)
    if (!k || v[i] != v[k - 1]) v[k++] = v[i];
  return k;
}
