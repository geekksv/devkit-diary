#pragma once
//
// Improv Serial: an open standard for handing a device its WiFi name and password over the USB
// cable it was just flashed through. The same protocol ESPHome devices speak, documented at
// improv-wifi.com.
//
// One packet:   "IMPROV"  version(1)  type  length  data…  checksum
//                                                           └ the low byte of the sum of every
//                                                             byte before it
//
// Packets share the serial line with the board's ordinary log; a reader ignores anything that
// is not a well-formed packet, and the log ignores the packets.
//
// Pure byte handling — no Arduino — so the board and the flasher page can both be tested
// against it.

#include <stdint.h>
#include <string.h>

namespace improv {

const uint8_t VERSION = 1;

enum Type : uint8_t { CURRENT_STATE = 0x01, ERROR_STATE = 0x02, RPC = 0x03, RPC_RESULT = 0x04 };
enum State : uint8_t { READY = 0x02, PROVISIONING = 0x03, PROVISIONED = 0x04 };
enum Error : uint8_t { NONE = 0x00, BAD_PACKET = 0x01, UNKNOWN_RPC = 0x02, NO_CONNECT = 0x03, UNKNOWN = 0xFF };
enum Command : uint8_t { WIFI_SETTINGS = 0x01, GET_STATE = 0x02, GET_INFO = 0x03, GET_NETWORKS = 0x04 };

const size_t MAX_DATA = 200;     // an SSID is ≤ 32 bytes and a WPA2 password ≤ 64: plenty

// Writes one packet into `out` (at least len + 10 bytes). Returns its size.
inline size_t build(uint8_t type, const uint8_t *data, size_t len, uint8_t *out) {
  memcpy(out, "IMPROV", 6);
  out[6] = VERSION;
  out[7] = type;
  out[8] = (uint8_t)len;
  if (len) memcpy(out + 9, data, len);
  uint8_t sum = 0;
  for (size_t i = 0; i < 9 + len; i++) sum += out[i];
  out[9 + len] = sum;
  return 10 + len;
}

// An RPC result's data: the command it answers, then length-prefixed strings.
inline size_t buildResult(uint8_t command, const char *const *strings, int count, uint8_t *data) {
  size_t n = 2;
  data[0] = command;
  for (int i = 0; i < count; i++) {
    const size_t l = strlen(strings[i]);
    if (n + 1 + l > MAX_DATA) break;
    data[n++] = (uint8_t)l;
    memcpy(data + n, strings[i], l);
    n += l;
  }
  data[1] = (uint8_t)(n - 2);
  return n;
}

// Collects packets out of a stream of bytes, one byte at a time. feed() returns true when a
// whole, correctly checksummed packet has arrived; it is then in type/len/data.
struct Reader {
  uint8_t buf[9 + MAX_DATA + 1];
  size_t at = 0;
  uint8_t type = 0, len = 0;
  const uint8_t *data = buf + 9;

  bool feed(uint8_t b) {
    if (at < 6) {                                      // matching "IMPROV"
      if (b == (uint8_t)"IMPROV"[at]) buf[at++] = b;
      else at = (b == 'I') ? (buf[0] = b, 1) : 0;
      return false;
    }
    buf[at++] = b;
    if (at == 7 && b != VERSION) { at = 0; return false; }
    if (at == 9 && buf[8] > MAX_DATA) { at = 0; return false; }
    if (at < 9 || at < 10 + (size_t)buf[8]) return false;
    // the whole packet is in: check it
    const size_t total = 10 + buf[8];
    uint8_t sum = 0;
    for (size_t i = 0; i < total - 1; i++) sum += buf[i];
    const bool ok = sum == buf[total - 1];
    type = buf[7];
    len = buf[8];
    at = 0;
    return ok;
  }
};

// Reads the WiFi name and password out of a WIFI_SETTINGS RPC:
//   data = command, length, ssid_len, ssid…, pass_len, pass…
inline bool parseWifi(const uint8_t *data, size_t len, char *ssid, size_t scap, char *pass, size_t pcap) {
  if (len < 3 || data[0] != WIFI_SETTINGS) return false;
  const size_t body = data[1];
  if (2 + body > len) return false;
  const size_t sl = data[2];
  if (3 + sl > 2 + body || sl >= scap) return false;
  memcpy(ssid, data + 3, sl);
  ssid[sl] = 0;
  const size_t pl = data[3 + sl];
  if (4 + sl + pl > 2 + body || pl >= pcap) return false;
  memcpy(pass, data + 4 + sl, pl);
  pass[pl] = 0;
  return sl > 0;
}

}  // namespace improv
