#pragma once
// The heart of Phone Pad, with nothing from Arduino in it, so a laptop can test it
// (tools/pad_test.cpp). The phone decides what each widget means and sends tiny intents over a
// WebSocket; this turns a stream of intents into the three HID reports the PC understands — a
// keyboard, a mouse and a gamepad. pad.ino sends those over Bluetooth; the layout itself (the
// widgets you design) lives on the phone and is saved as JSON blobs on the board.
//
// Intents are text, ';'-separated, a few bytes each, so they are cheap to send many times a second:
//   k <usage> <1|0>   a keyboard key down/up (HID usage code)
//   m <bit> <1|0>     a modifier (1=ctrl,2=shift,4=alt,8=gui) down/up
//   b <0..15> <1|0>   a gamepad button down/up
//   a <0..3> <-127..127>   a gamepad axis (0=LX,1=LY,2=RX,3=RY)
//   h <0..8>          the gamepad hat / d-pad (0=up..7, 8=centre)
//   p <dx> <dy>       move the mouse by dx,dy
//   c <0..2> <1|0>    a mouse button (0=left,1=right,2=middle)
//   s <dv>            scroll by dv
//   x                 release everything (sent when the page is hidden or a touch is lost)

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct HidState {
  // keyboard
  uint8_t mod = 0;
  uint8_t keys[6] = {0};
  // mouse (buttons are held; move and scroll are deltas, drained into each report)
  uint8_t mouseBtns = 0;
  int16_t mdx = 0, mdy = 0, mscroll = 0;
  // gamepad
  uint16_t gpBtns = 0;
  int8_t axes[4] = {0};
  uint8_t hat = 8;             // 8 = centred
  // which reports changed since last drain
  bool kbdDirty = false, mouseDirty = false, gpDirty = false;

  void reset() {
    mod = 0;
    memset(keys, 0, sizeof keys);
    mouseBtns = 0;
    mdx = mdy = mscroll = 0;
    gpBtns = 0;
    memset(axes, 0, sizeof axes);
    hat = 8;
    kbdDirty = mouseDirty = gpDirty = true;
  }

  void holdKey(uint8_t code, bool down) {
    if (!code) return;
    if (down) {
      for (uint8_t k : keys) if (k == code) return;
      for (uint8_t &k : keys) if (!k) { k = code; kbdDirty = true; return; }
    } else {
      for (uint8_t &k : keys) if (k == code) { k = 0; kbdDirty = true; }
    }
  }

  // Applies one intent command. Returns true if it was understood.
  bool apply(const char *cmd) {
    while (*cmd == ' ') cmd++;
    const char op = *cmd++;
    auto num = [&]() { return (int)strtol(cmd, (char **)&cmd, 10); };
    switch (op) {
      case 'k': { int c = num(), d = num(); holdKey((uint8_t)c, d != 0); return true; }
      case 'm': { int bit = num(), d = num(); if (d) mod |= bit; else mod &= ~bit; kbdDirty = true; return true; }
      case 'b': { int b = num(), d = num(); if (b < 0 || b > 15) return true; uint16_t m = 1 << b; if (d) gpBtns |= m; else gpBtns &= ~m; gpDirty = true; return true; }
      case 'a': { int ax = num(), v = num(); if (ax < 0 || ax > 3) return true; axes[ax] = (int8_t)(v < -127 ? -127 : v > 127 ? 127 : v); gpDirty = true; return true; }
      case 'h': { int v = num(); hat = (uint8_t)(v < 0 || v > 8 ? 8 : v); gpDirty = true; return true; }
      case 'p': { int dx = num(), dy = num(); mdx += dx; mdy += dy; mouseDirty = true; return true; }
      case 'c': { int b = num(), d = num(); if (b < 0 || b > 2) return true; uint8_t m = 1 << b; if (d) mouseBtns |= m; else mouseBtns &= ~m; mouseDirty = true; return true; }
      case 's': { int dv = num(); mscroll += dv; mouseDirty = true; return true; }
      case 'x': reset(); return true;
      default: return op == 0;   // trailing empty is fine
    }
  }

  // Applies a ';'-separated line of intents in place (modifies the buffer). Returns the count.
  int applyLine(char *line) {
    int n = 0;
    for (char *p = line; p && *p;) {
      char *semi = strchr(p, ';');
      if (semi) *semi = 0;
      if (*p && apply(p)) n++;
      p = semi ? semi + 1 : nullptr;
    }
    return n;
  }

  // ── the three reports (bytes the PC reads) ──
  // Keyboard: report ID, modifier, reserved, 6 keys.
  int keyboardReport(uint8_t *b) {
    b[0] = 1; b[1] = mod; b[2] = 0;
    for (int i = 0; i < 6; i++) b[3 + i] = keys[i];
    kbdDirty = false;
    return 9;
  }
  // Mouse: report ID, buttons, dx, dy, scroll. Deltas are drained (and clamped to one report).
  int mouseReport(uint8_t *b) {
    auto clamp = [](int v) { return (int8_t)(v < -127 ? -127 : v > 127 ? 127 : v); };
    b[0] = 2; b[1] = mouseBtns; b[2] = clamp(mdx); b[3] = clamp(mdy); b[4] = clamp(mscroll);
    mdx -= (int8_t)b[2]; mdy -= (int8_t)b[3]; mscroll -= (int8_t)b[4];
    mouseDirty = (mdx || mdy || mscroll);   // more left over: send again
    return 5;
  }
  // Gamepad: report ID, 4 axes, hat, 16 buttons (2 bytes).
  int gamepadReport(uint8_t *b) {
    b[0] = 3;
    for (int i = 0; i < 4; i++) b[1 + i] = (uint8_t)axes[i];
    b[5] = hat;
    b[6] = (uint8_t)(gpBtns & 0xFF);
    b[7] = (uint8_t)(gpBtns >> 8);
    gpDirty = false;
    return 8;
  }
};
