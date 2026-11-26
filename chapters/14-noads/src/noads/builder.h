#pragma once
// Turns a downloaded blocklist (text, any of the usual formats) into a sorted list file, in little
// memory and with few flash writes. The same code runs on the board (LittleFS) and on a laptop
// (ordinary files) through FS:
//
//   bool append(const char *path, const uint8_t *data, size_t n)    adds to the end of a file
//   bool scan(const char *path, F fn)       calls fn(data, n) for the whole file, in pieces
//   void remove(const char *path)
//
// Feed it the download with feed() as the bytes arrive, then call finish(). The finished list is
// /list.new; the caller swaps it in for /list.bin (noads.ino does that while no lookup is running).
//
// How: while downloading, each name's fingerprint goes into one of 8 files by its top 3 bits,
// 256 at a time (flash is slow at many small writes, fine at fewer big ones). Then each file is
// read 16 times, each time keeping one sixteenth of it (the next 4 bits): ~1,500 fingerprints,
// ~12 KB, sorted in memory and written out. 128 pieces in order make the whole list sorted.

#include "blocklist.h"
// One 16 KB area, reserved once (never taken from the heap, so it can't fail after memory has been
// broken into pieces), used twice: while downloading it holds the fingerprints waiting to be
// written; afterwards, the piece being sorted. 2,048 fingerprints a piece covers lists of up to
// ~240,000 names.
static const uint32_t SORT_MAX = 2048;
static uint64_t workArea[SORT_MAX];

static const int FILE_BUCKETS = 8, SUB_BUCKETS = BUCKETS / FILE_BUCKETS;

template <class FS> struct Builder {
  FS &fs;
  static const int BUF_PER_FILE = 256;                   // fingerprints held per file before writing
  static_assert(FILE_BUCKETS * BUF_PER_FILE * FP_BYTES <= sizeof workArea, "the waiting fingerprints fit the work area");
  uint8_t (*pending)[BUF_PER_FILE * FP_BYTES] = (uint8_t (*)[BUF_PER_FILE * FP_BYTES])workArea;
  uint16_t pendingN[FILE_BUCKETS];
  uint32_t subCount[BUCKETS];                            // how many go in each piece, known before sorting
  char line[512];
  int lineLen = 0;
  bool lineTooLong = false;
  uint32_t lines = 0, names = 0;
  bool failed = false;
  char why[64] = "";                                     // what went wrong, for the dashboard
  void fail(const char *what, const char *path) {
    if (!failed) snprintf(why, sizeof why, "%s %s", what, path);
    failed = true;
  }

  explicit Builder(FS &f) : fs(f) {}

  static void bucketPath(int b, char *out) { snprintf(out, 16, "/b%d.tmp", b); }
  void removeBuckets() {
    char p[16];
    for (int b = 0; b < FILE_BUCKETS; b++) { bucketPath(b, p); fs.remove(p); }
  }

  void begin() {
    memset(pendingN, 0, sizeof pendingN);
    memset(subCount, 0, sizeof subCount);
    lineLen = 0;
    lineTooLong = false;
    lines = names = 0;
    failed = false;
    why[0] = 0;
    removeBuckets();
    fs.remove("/list.new");
  }

  void flushFile(int b) {
    if (!pendingN[b]) return;
    char p[16];
    bucketPath(b, p);
    if (!fs.append(p, pending[b], pendingN[b] * FP_BYTES)) fail("writing", p);
    pendingN[b] = 0;
  }

  void addName(const char *name) {
    const uint64_t fp = fingerprint(name);
    subCount[bucketOf(fp)]++;
    const int b = bucketOf(fp) / SUB_BUCKETS;
    putFp(pending[b] + pendingN[b] * FP_BYTES, fp);
    if (++pendingN[b] == BUF_PER_FILE) flushFile(b);
    names++;
  }

  void endLine() {
    lines++;
    if (!lineTooLong) {
      line[lineLen] = 0;
      char name[NAME_MAX_LEN + 2];
      if (parseListLine(line, name, sizeof name)) addName(name);
    }
    lineLen = 0;
    lineTooLong = false;
  }

  void feed(const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n && !failed; i++) {
      const char c = (char)data[i];
      if (c == '\n') endLine();
      else if (lineLen < (int)sizeof line - 1) line[lineLen++] = c;
      else lineTooLong = true;
    }
  }

  // Sorts everything into /list.new. Returns the number of names in it, or -1 (and no /list.new).
  long finish(uint32_t built) {
    if (lineLen) endLine();
    for (int b = 0; b < FILE_BUCKETS; b++) flushFile(b);
    uint32_t total = 0;
    uint8_t h[HEADER];
    uint32_t biggest = 1;
    for (int i = 0; i < BUCKETS; i++) biggest = subCount[i] > biggest ? subCount[i] : biggest;
    if (biggest > SORT_MAX && !failed) {
      char n[24];
      snprintf(n, sizeof n, "(%lu in one piece)", (unsigned long)biggest);
      fail("the list is too big; choose a smaller one", n);
    }
    uint64_t *v = workArea;                              // the waiting fingerprints are all written by now
    static const int OUT_FPS = 256;
    static uint8_t outBuf[OUT_FPS * FP_BYTES];           // packing into bytes, 1.5 KB at a time
    char p[16];
    for (int b = 0; b < FILE_BUCKETS && !failed; b++) {
      bucketPath(b, p);
      for (int sub = 0; sub < SUB_BUCKETS && !failed; sub++) {
        const int want = b * SUB_BUCKETS + sub;
        uint32_t n = 0;
        fs.scan(p, [&](const uint8_t *d, size_t len) {
          for (size_t i = 0; i + FP_BYTES <= len; i += FP_BYTES) {
            const uint64_t fp = getFp(d + i);
            if (bucketOf(fp) == want && n < biggest) v[n++] = fp;
          }
        });
        n = sortUnique(v, n);
        for (uint32_t i = 0; i < n && !failed;) {
          int k = 0;
          for (; k < OUT_FPS && i < n; k++, i++) putFp(outBuf + k * FP_BYTES, v[i]);
          if (!fs.append("/list.new", outBuf, k * FP_BYTES)) fail("writing", "/list.new");
        }
        total += n;
      }
      fs.remove(p);
    }
    if (failed || !total) {
      removeBuckets();                                   // leave no half-made files
      fs.remove("/list.new");
      return -1;
    }
    // The footer goes on last, so a list cut short by a power cut never ends in a valid one.
    writeHeader(h, total, built);
    if (!fs.append("/list.new", h, HEADER)) { fs.remove("/list.new"); return -1; }
    return total;
  }
};
