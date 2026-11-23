#pragma once
//
// Translation through two free services, no account or key. Plain C++ (addresses and reading
// the answers), so the same code is tested on a laptop against real answers.
//
//   1. Google, through the address Google's own Chrome extension uses:
//        clients5.google.com/translate_a/t?client=dict-chrome-ex&sl=auto&tl=hi&q=...
//      answers  [["नमस्ते, आप कैसे हैं?","en"]]   (the translation, and the language it detected).
//      The better-known translate.googleapis.com ...client=gtx address answers some home
//      networks with a "Sorry, unusual traffic" page, so it is not used.
//   2. MyMemory, as the fallback:
//        api.mymemory.translated.net/get?q=...&langpair=Autodetect|hi
//      answers {"responseData":{"translatedText":"...","detectedLanguage":"fr"},
//               "quotaFinished":false,"responseStatus":200,...}
//      About 5,000 characters a day without an account.

#include <string>
#include <cstring>
#include <cstdint>

namespace tr {

inline void putUtf8(std::string &o, uint32_t c) {
  if (c < 0x80) o += (char)c;
  else if (c < 0x800) { o += (char)(0xC0 | c >> 6); o += (char)(0x80 | (c & 0x3F)); }
  else if (c < 0x10000) {
    o += (char)(0xE0 | c >> 12); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F));
  } else {
    o += (char)(0xF0 | c >> 18); o += (char)(0x80 | ((c >> 12) & 0x3F));
    o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F));
  }
}

inline int hex4(const char *p) {
  int v = 0;
  for (int i = 0; i < 4; i++) {
    const char c = p[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else return -1;
  }
  return v;
}

// Reads a JSON string starting at its opening quote; returns the position after the closing one.
inline size_t readString(const char *s, size_t n, size_t i, std::string &out) {
  out.clear();
  for (i++; i < n; i++) {
    const char c = s[i];
    if (c == '"') return i + 1;
    if (c != '\\') { out += c; continue; }
    if (++i >= n) break;
    switch (s[i]) {
      case 'n': out += '\n'; break;
      case 't': out += '\t'; break;
      case 'r': case 'b': case 'f': break;
      case 'u': {
        if (i + 4 >= n) return n;
        const int c1 = hex4(s + i + 1);
        i += 4;
        if (c1 < 0) break;
        uint32_t cp = (uint32_t)c1;
        if (c1 >= 0xD800 && c1 < 0xDC00 && i + 6 < n && s[i + 1] == '\\' && s[i + 2] == 'u') {
          const int c2 = hex4(s + i + 3);
          if (c2 >= 0xDC00 && c2 < 0xE000) { cp = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00); i += 6; }
        }
        putUtf8(out, cp);
        break;
      }
      default: out += s[i];      // \" \\ \/
    }
  }
  return n;
}

inline void urlEncode(std::string &o, const char *s) {
  static const char *H = "0123456789ABCDEF";
  for (; *s; s++) {
    const uint8_t c = (uint8_t)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~')
      o += (char)c;
    else { o += '%'; o += H[c >> 4]; o += H[c & 15]; }
  }
}

// ── Google ──────────────────────────────────────────────────────────────────

inline std::string googleUrl(const char *to, const char *text) {
  std::string u = "https://clients5.google.com/translate_a/t?client=dict-chrome-ex&sl=auto&tl=";
  urlEncode(u, to);
  u += "&q=";
  urlEncode(u, text);
  return u;
}

// [["translation","src"]], or ["translation"] when the source language was given.
inline bool googleParse(const char *s, size_t n, std::string &text, std::string &src) {
  text.clear();
  src.clear();
  int depth = 0, strings = 0;
  std::string str;
  for (size_t i = 0; i < n;) {
    const char c = s[i];
    if (c == '"') {
      i = readString(s, n, i, str);
      if (strings == 0) text = str;
      else if (strings == 1 && depth == 2) src = str;
      strings++;
      continue;
    }
    if (c == '[') depth++;
    else if (c == ']') { if (--depth == 0) break; }   // only the first translation is wanted
    else if (c == '{' || c == '<') return false;       // an error page or an object: not this
    i++;
  }
  return !text.empty();
}

// ── MyMemory ────────────────────────────────────────────────────────────────

inline std::string myMemoryUrl(const char *to, const char *text) {
  std::string u = "https://api.mymemory.translated.net/get?q=";
  urlEncode(u, text);
  u += "&langpair=Autodetect%7C";
  urlEncode(u, to);
  return u;
}

// The string value of the first "key": in the text, or false.
inline bool stringAfter(const char *s, size_t n, const char *key, std::string &out) {
  std::string k = std::string("\"") + key + "\":";
  const char *end = s + n;
  for (const char *p = s; p + k.size() < end; p++) {
    if (memcmp(p, k.data(), k.size())) continue;
    size_t i = (p - s) + k.size();
    while (i < n && s[i] == ' ') i++;
    if (i >= n || s[i] != '"') return false;
    readString(s, n, i, out);
    return true;
  }
  return false;
}

inline bool myMemoryParse(const char *s, size_t n, std::string &text, std::string &src) {
  text.clear();
  src.clear();
  const std::string all(s, n);
  if (all.find("\"responseStatus\":200") == std::string::npos) return false;
  if (all.find("\"quotaFinished\":true") != std::string::npos) return false;
  if (!stringAfter(s, n, "translatedText", text)) return false;
  stringAfter(s, n, "detectedLanguage", src);
  // A warning arrives as a "translation" in capitals.
  if (text.rfind("MYMEMORY WARNING", 0) == 0) { text.clear(); return false; }
  return !text.empty();
}

}  // namespace tr
