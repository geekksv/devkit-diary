#pragma once
//
// Reading what the free services send back, without a JSON or XML library.
//
// The board never holds a whole response it does not need. Small answers (the weather is about
// 1 KB) are read whole and searched; big ones (a news feed can be 150 KB, Wikipedia's "on this
// day" 130 KB) are read as they arrive, byte by byte, and abandoned as soon as the few values
// the mirror shows have been found.
//
// Nothing in here touches Arduino or the network: a Source is anything that hands out bytes,
// so the same code is tested on a laptop against saved real responses.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ── a source of bytes ────────────────────────────────────────────────────────

struct Source {
  virtual int next() = 0;         // the next byte, or -1 when the response is over
  virtual ~Source() {}
};

// Appends one code point as UTF-8. Returns the new length, never past `cap - 1`.
static inline size_t putUtf8(char *out, size_t len, size_t cap, uint32_t cp) {
  char b[4];
  int n;
  if (cp < 0x80) { b[0] = (char)cp; n = 1; }
  else if (cp < 0x800) { b[0] = (char)(0xC0 | cp >> 6); b[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
  else if (cp < 0x10000) {
    b[0] = (char)(0xE0 | cp >> 12); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    b[2] = (char)(0x80 | (cp & 0x3F)); n = 3;
  } else {
    b[0] = (char)(0xF0 | cp >> 18); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); n = 4;
  }
  if (len + n >= cap) return len;   // a character that does not fit is dropped whole
  memcpy(out + len, b, n);
  return len + n;
}

static inline int hexval(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Reads bytes until `pat` has gone past, or the source ends. A tiny matcher that restarts
// correctly for the patterns used here (none of them repeats its own beginning).
static inline bool skipPast(Source &s, const char *pat) {
  const size_t n = strlen(pat);
  size_t m = 0;
  for (int c; (c = s.next()) >= 0;) {
    if (c == pat[m]) { if (++m == n) return true; }
    else m = (c == pat[0]) ? 1 : 0;
  }
  return false;
}

// Reads the rest of a JSON string — the opening quote already consumed — decoding escapes,
// including \uXXXX and surrogate pairs. Non-breaking spaces become plain spaces.
static inline bool readJsonString(Source &s, char *out, size_t cap) {
  size_t len = 0;
  for (int c; (c = s.next()) >= 0;) {
    if (c == '"') { out[len] = 0; return true; }
    if (c != '\\') { if (len + 1 < cap) out[len++] = (char)c; continue; }
    const int e = s.next();
    switch (e) {
      case 'n': case 'r': case 't': c = ' '; break;
      case 'b': case 'f': c = -2; break;
      case 'u': {
        uint32_t cp = 0;
        for (int i = 0; i < 4; i++) { const int h = hexval(s.next()); if (h < 0) { out[len] = 0; return false; } cp = cp << 4 | h; }
        if (cp >= 0xD800 && cp < 0xDC00) {               // a surrogate pair: read the low half
          if (s.next() == '\\' && s.next() == 'u') {
            uint32_t lo = 0;
            for (int i = 0; i < 4; i++) { const int h = hexval(s.next()); if (h < 0) break; lo = lo << 4 | h; }
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
        }
        if (cp == 0xA0) cp = ' ';
        len = putUtf8(out, len, cap, cp);
        c = -2;
        break;
      }
      default: c = e;                                      // \" \\ \/
    }
    if (c >= 0 && len + 1 < cap) out[len++] = (char)c;
  }
  out[len] = 0;
  return false;
}

// A JSON number at the source's position (after any spaces).
static inline bool readJsonNumber(Source &s, double &v) {
  char b[32];
  size_t n = 0;
  int c;
  while ((c = s.next()) == ' ') {}
  while (c >= 0 && (strchr("+-.eE", c) || (c >= '0' && c <= '9')) && n + 1 < sizeof(b)) {
    b[n++] = (char)c;
    c = s.next();
  }
  b[n] = 0;
  if (!n) return false;
  v = strtod(b, NULL);
  return true;
}

// ── a whole small body, searched by key ──────────────────────────────────────

struct TextSource : Source {     // a string already in memory, read from a position
  const char *p;
  explicit TextSource(const char *s) : p(s) {}
  int next() override { return *p ? (uint8_t)*p++ : -1; }
};

// The number after "key": anywhere in `body`, searched from `from`.
static inline bool jsonNumber(const char *body, const char *key, double &v, const char *from = NULL) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *at = strstr(from ? from : body, pat);
  if (!at) return false;
  TextSource s(at + strlen(pat));
  return readJsonNumber(s, v);
}

static inline bool jsonString(const char *body, const char *key, char *out, size_t cap, const char *from = NULL) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char *at = strstr(from ? from : body, pat);
  if (!at) { out[0] = 0; return false; }
  TextSource s(at + strlen(pat));
  return readJsonString(s, out, cap);
}

// The numbers in "key":[a,b,c] — up to `max`. Returns how many were read.
static inline int jsonNumbers(const char *body, const char *key, double *v, int max, const char *from = NULL) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":[", key);
  const char *at = strstr(from ? from : body, pat);
  if (!at) return 0;
  TextSource s(at + strlen(pat));
  int n = 0;
  while (n < max) {
    if (!readJsonNumber(s, v[n])) break;
    n++;
    if (*(s.p - 1) == ']') break;      // the number reader stopped on the closing bracket
  }
  return n;
}

// ── the weather (Open-Meteo forecast, timeformat=unixtime) ───────────────────

const int DAYS = 6;                       // today and five more

struct Weather {
  bool ok;
  int32_t offset;                         // the city's offset from UTC, seconds
  double temp, feels, wind;
  int humidity, code, isDay;
  int days;
  uint32_t dayStart[DAYS], sunrise[DAYS], sunset[DAYS];
  int dayCode[DAYS];
  double hi[DAYS], lo[DAYS];
};

static inline bool parseWeather(const char *body, Weather &w) {
  memset(&w, 0, sizeof(w));
  double v;
  if (!jsonNumber(body, "utc_offset_seconds", v)) return false;
  w.offset = (int32_t)v;
  const char *cur = strstr(body, "\"current\":{");
  const char *daily = strstr(body, "\"daily\":{");
  if (!cur || !daily) return false;
  if (!jsonNumber(body, "temperature_2m", w.temp, cur)) return false;
  jsonNumber(body, "apparent_temperature", w.feels, cur);
  if (jsonNumber(body, "relative_humidity_2m", v, cur)) w.humidity = (int)v;
  if (jsonNumber(body, "weather_code", v, cur)) w.code = (int)v;
  jsonNumber(body, "wind_speed_10m", w.wind, cur);
  if (jsonNumber(body, "is_day", v, cur)) w.isDay = (int)v;

  double t[DAYS], c[DAYS], hi[DAYS], lo[DAYS], rise[DAYS], set[DAYS];
  const int n = jsonNumbers(body, "time", t, DAYS, daily);
  if (n < 1 || jsonNumbers(body, "weather_code", c, DAYS, daily) != n ||
      jsonNumbers(body, "temperature_2m_max", hi, DAYS, daily) != n ||
      jsonNumbers(body, "temperature_2m_min", lo, DAYS, daily) != n ||
      jsonNumbers(body, "sunrise", rise, DAYS, daily) != n ||
      jsonNumbers(body, "sunset", set, DAYS, daily) != n) return false;
  w.days = n;
  for (int i = 0; i < n; i++) {
    w.dayStart[i] = (uint32_t)t[i];
    w.dayCode[i] = (int)c[i];
    w.hi[i] = hi[i];
    w.lo[i] = lo[i];
    w.sunrise[i] = (uint32_t)rise[i];
    w.sunset[i] = (uint32_t)set[i];
  }
  w.ok = true;
  return true;
}

// ── air quality (Open-Meteo air-quality, current=us_aqi) ─────────────────────

static inline int parseAqi(const char *body) {
  const char *cur = strstr(body, "\"current\":{");
  double v;
  if (!cur || !jsonNumber(body, "us_aqi", v, cur)) return -1;
  return (int)v;
}

// ── the next nationwide public holiday (Nager.Date NextPublicHolidays) ───────
// Regional ones ("global":false — a bank holiday in one county) are skipped.

static inline bool parseHoliday(const char *body, char *name, size_t ncap, char *date, size_t dcap) {
  for (const char *at = strstr(body, "{\"date\":\""); at; at = strstr(at + 1, "{\"date\":\"")) {
    const char *end = strstr(at + 1, "{\"date\":\"");
    const char *global = strstr(at, "\"global\":true");
    if (!global || (end && global > end)) continue;
    TextSource d(at + 9);
    readJsonString(d, date, dcap);
    jsonString(body, "name", name, ncap, at);
    return name[0] && date[0];
  }
  name[0] = date[0] = 0;
  return false;
}

// ── a city by name (Open-Meteo geocoding) ────────────────────────────────────

struct Place {
  char name[40], region[40], country[40], cc[4];
  double lat, lon;
};

// Up to `max` places from a search. Each result object starts {"id":…
static inline int parsePlaces(const char *body, Place *out, int max) {
  int n = 0;
  for (const char *at = strstr(body, "{\"id\":"); at && n < max; at = strstr(at + 1, "{\"id\":")) {
    Place &p = out[n];
    memset(&p, 0, sizeof(p));
    const char *end = strstr(at + 1, "{\"id\":");
    // Fields are searched from the object's start; a field missing here could otherwise be
    // read from the next result, so anything found past `end` is discarded.
    char tmp[40];
    if (!jsonString(body, "name", p.name, sizeof(p.name), at)) continue;
    if (!jsonNumber(body, "latitude", p.lat, at) || !jsonNumber(body, "longitude", p.lon, at)) continue;
    const char *r = strstr(at, "\"admin1\":\"");
    if (r && (!end || r < end)) jsonString(body, "admin1", p.region, sizeof(p.region), at);
    const char *c = strstr(at, "\"country\":\"");
    if (c && (!end || c < end)) jsonString(body, "country", p.country, sizeof(p.country), at);
    const char *k = strstr(at, "\"country_code\":\"");
    if (k && (!end || k < end)) { jsonString(body, "country_code", tmp, sizeof(tmp), at); strncpy(p.cc, tmp, 3); p.cc[3] = 0; }
    n++;
  }
  return n;
}

// ── news headlines, from any RSS feed, streamed ──────────────────────────────

const int HEADLINES = 8;
const int HEADLINE_LEN = 160;

// Decodes the entities a feed title uses — &amp; &lt; &gt; &quot; &apos; and numeric ones.
static inline size_t putEntity(const char *ent, char *out, size_t len, size_t cap) {
  uint32_t cp = 0;
  if (ent[0] == '#') cp = (ent[1] == 'x' || ent[1] == 'X') ? strtoul(ent + 2, NULL, 16) : strtoul(ent + 1, NULL, 10);
  else if (!strcmp(ent, "amp")) cp = '&';
  else if (!strcmp(ent, "lt")) cp = '<';
  else if (!strcmp(ent, "gt")) cp = '>';
  else if (!strcmp(ent, "quot")) cp = '"';
  else if (!strcmp(ent, "apos")) cp = '\'';
  else if (!strcmp(ent, "nbsp")) cp = ' ';
  if (!cp) return len;
  if (cp == 0xA0) cp = ' ';
  return putUtf8(out, len, cap, cp);
}

// The text of one <title>…</title>, the opening tag already consumed. Strips CDATA markers and
// stray tags, decodes entities, collapses whitespace.
static inline void readTitle(Source &s, char *out, size_t cap) {
  size_t len = 0;
  bool space = false;                           // whitespace seen, not yet written
  // Every character goes out through here, so a pending space always lands before it —
  // whether the character came from the text, an entity or a false start of "</title>".
  auto put = [&](char ch) {
    if (space && len && len + 1 < cap) out[len++] = ' ';
    space = false;
    if (len + 1 < cap) out[len++] = ch;
  };
  const char *close = "</title>";
  size_t m = 0;                                 // how much of "</title>" is pending
  char ent[12];
  int e = -1;                                   // >= 0 while inside &…;
  for (int c; (c = s.next()) >= 0;) {
    if (c == close[m]) { if (++m == 8) break; continue; }
    if (m) {                                    // a false start of "</title>": it was text
      for (size_t i = 0; i < m; i++) put(close[i]);
      m = (c == '<') ? 1 : 0;
      if (m) continue;
    }
    if (e >= 0) {
      const bool name = (c == '#' && e == 0) || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      if (c == ';' && e > 0) {
        ent[e] = 0;
        char one[8];
        const size_t n = putEntity(ent, one, 0, sizeof(one));
        one[n] = 0;
        for (size_t i = 0; i < n; i++) {
          if (one[i] == ' ') space = len > 0; else put(one[i]);
        }
        e = -1;
        continue;
      }
      if (name && e < 10) { ent[e++] = (char)c; continue; }
      // Not an entity after all — a bare "&" (legal inside CDATA: "Tom & Jerry"). Keep it.
      put('&');
      for (int i = 0; i < e; i++) put(ent[i]);
      e = -1;
      if (c == '&') { e = 0; continue; }
    }
    if (c == '&') { e = 0; continue; }
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') { space = len > 0; continue; }
    put((char)c);
  }
  if (e > 0) {                                  // the title ended inside "&…": it was text
    put('&');
    for (int i = 0; i < e; i++) put(ent[i]);
  }
  out[len] = 0;
  // CDATA arrives as literal text: take the inside of <![CDATA[ … ]]>. Entities inside it were
  // already decoded above — feeds put &amp; in CDATA titles often enough that decoding helps.
  char *cd = strstr(out, "<![CDATA[");
  if (cd) {
    char *end = strstr(cd, "]]>");
    if (end) *end = 0;
    memmove(out, cd + 9, strlen(cd + 9) + 1);
  }
  // trim
  size_t L = strlen(out);
  while (L && out[L - 1] == ' ') out[--L] = 0;
  char *start = out;
  while (*start == ' ') start++;
  if (start != out) memmove(out, start, strlen(start) + 1);
}

// The titles of the first items of an RSS feed. The feed's own <title> (the channel name)
// comes before the first <item> and is skipped by looking for "<item" first. Atom feeds use
// <entry>, which is accepted too.
static inline int parseRss(Source &s, char titles[][HEADLINE_LEN], int max) {
  int n = 0;
  // Find "<item" or "<entry" — whichever comes first — then that item's <title.
  const char *a = "<item", *b = "<entry";
  while (n < max) {
    size_t ma = 0, mb = 0;
    bool found = false;
    for (int c; (c = s.next()) >= 0;) {
      ma = (c == a[ma]) ? ma + 1 : (c == a[0]);
      mb = (c == b[mb]) ? mb + 1 : (c == b[0]);
      if (ma == 5 || mb == 6) { found = true; break; }
    }
    if (!found) break;
    if (!skipPast(s, "<title")) break;
    for (int c; (c = s.next()) >= 0 && c != '>';) {}    // past any attributes: <title type="html">
    readTitle(s, titles[n], HEADLINE_LEN);
    if (titles[n][0]) n++;
  }
  return n;
}

// ── "on this day", streamed from Wikipedia's feed ────────────────────────────
// Each event is {"text":"…","pages":[…],"year":1869}. A page inside it also has a "text" key
// (its namespace: {"id":0,"text":""}), so an event is found by the opening brace in front.

const int EVENTS = 5;
const int EVENT_LEN = 240;

struct Event { int year; char text[EVENT_LEN]; };

static inline int parseOnThisDay(Source &s, Event *ev, int max) {
  int n = 0;
  while (n < max && skipPast(s, "{\"text\":\"")) {
    readJsonString(s, ev[n].text, EVENT_LEN);
    double y;
    if (!skipPast(s, "\"year\":") || !readJsonNumber(s, y)) break;
    ev[n].year = (int)y;
    n++;
  }
  return n;
}
