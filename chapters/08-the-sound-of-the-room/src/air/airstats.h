#pragma once
//
// Counting the frames the board overhears, per channel. Nothing here touches the radio or
// Arduino, so the arithmetic is tested on a laptop (airstats_test).
//
// The promiscuous-mode callback — which runs deep inside the WiFi task, thousands of times a
// second — may do only the cheapest thing: add one frame to a tally. Everything that turns a
// pile of tallies into rates happens later, in summarize(), away from the radio.

#include <stdint.h>
#include <string.h>

// WiFi channels run 1..14; index 0 collects anything with an out-of-range channel.
const int AIR_CHANS = 15;

// Frame classes, as the callback is told them.
enum AirType { AIR_MGMT = 0, AIR_CTRL = 1, AIR_DATA = 2, AIR_MISC = 3 };

struct AirTally {
  uint32_t pkts[AIR_CHANS];
  uint32_t bytes[AIR_CHANS];
  int32_t  rssiSum[AIR_CHANS];
  uint32_t mgmt, ctrl, data, misc;

  void clear() { memset(this, 0, sizeof(*this)); }

  // One overheard frame. `ch` is the channel it arrived on, `rssi` its strength in dBm (a
  // negative number), `len` its length in bytes, `type` its class.
  void add(int ch, int rssi, uint32_t len, int type) {
    if (ch < 0 || ch >= AIR_CHANS) ch = 0;
    pkts[ch]++;
    bytes[ch] += len;
    rssiSum[ch] += rssi;
    if (type == AIR_MGMT) mgmt++;
    else if (type == AIR_CTRL) ctrl++;
    else if (type == AIR_DATA) data++;
    else misc++;
  }
};

// A tally reduced to rates over a window of `ms` milliseconds — the numbers the page sonifies.
struct AirSummary {
  float rate;                 // frames per second, all channels together
  float kbps;                 // kilobytes per second
  float perChan[AIR_CHANS];   // frames per second on each channel
  int   avgRssi[AIR_CHANS];   // mean strength of what was heard there, 0 if nothing
  float busiest;              // the largest perChan value, so the page can normalise
  uint32_t mgmt, ctrl, data, misc;
};

static inline void summarize(const AirTally &t, uint32_t ms, AirSummary &s) {
  memset(&s, 0, sizeof(s));
  const float sec = ms > 0 ? ms / 1000.0f : 1.0f;
  uint32_t totalPkts = 0, totalBytes = 0;
  for (int c = 0; c < AIR_CHANS; c++) {
    totalPkts += t.pkts[c];
    totalBytes += t.bytes[c];
    s.perChan[c] = t.pkts[c] / sec;
    s.avgRssi[c] = t.pkts[c] ? (int)(t.rssiSum[c] / (int32_t)t.pkts[c]) : 0;
    if (s.perChan[c] > s.busiest) s.busiest = s.perChan[c];
  }
  s.rate = totalPkts / sec;
  s.kbps = totalBytes / 1024.0f / sec;
  s.mgmt = t.mgmt;
  s.ctrl = t.ctrl;
  s.data = t.data;
  s.misc = t.misc;
}
