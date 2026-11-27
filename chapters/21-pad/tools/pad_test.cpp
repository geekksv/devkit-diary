// The HID state machine on a laptop:  c++ -std=c++17 -I../src/pad pad_test.cpp -o t && ./t
#include "pad.h"
#include <string>

static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static void line(HidState &h, const char *s) { char buf[256]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0; h.applyLine(buf); }

int main() {
  HidState h;
  uint8_t b[16];

  // Keyboard: hold W (0x1a) and D (0x07), with shift.
  line(h, "k 26 1;k 7 1;m 2 1");
  CHECK(h.kbdDirty);
  h.keyboardReport(b);
  CHECK(b[0] == 1 && b[1] == 2 && b[2] == 0);                 // report id, shift modifier
  CHECK((b[3] == 26 || b[4] == 26) && (b[3] == 7 || b[4] == 7));
  CHECK(!h.kbdDirty);                                         // cleared after the report
  // Release W; D stays.
  line(h, "k 26 0");
  h.keyboardReport(b);
  bool hasW = false, hasD = false;
  for (int i = 3; i < 9; i++) { hasW |= b[i] == 26; hasD |= b[i] == 7; }
  CHECK(!hasW && hasD);
  // No more than 6 keys at once; a 7th is dropped, not an overflow.
  h.reset();
  line(h, "k 4 1;k 5 1;k 6 1;k 7 1;k 8 1;k 9 1;k 10 1");
  h.keyboardReport(b);
  int held = 0;
  for (int i = 3; i < 9; i++) held += b[i] != 0;
  CHECK(held == 6);
  // Holding the same key twice doesn't take two slots.
  h.reset();
  line(h, "k 4 1;k 4 1");
  h.keyboardReport(b);
  held = 0;
  for (int i = 3; i < 9; i++) held += b[i] == 4;
  CHECK(held == 1);

  // Gamepad buttons and axes.
  h.reset();
  line(h, "b 0 1;b 5 1;a 0 -120;a 1 127;a 3 -200");           // axis 3 over-range is clamped
  h.gamepadReport(b);
  CHECK(b[0] == 3);
  CHECK((int8_t)b[1] == -120 && (int8_t)b[2] == 127 && (int8_t)b[4] == -127);
  CHECK(b[6] == (1 | (1 << 5)) && b[7] == 0);                 // buttons 0 and 5
  line(h, "b 0 0");
  h.gamepadReport(b);
  CHECK(b[6] == (1 << 5));
  // Hat / d-pad.
  line(h, "h 2");
  h.gamepadReport(b);
  CHECK(b[5] == 2);
  line(h, "h 99");
  h.gamepadReport(b);
  CHECK(b[5] == 8);                                           // out of range -> centred

  // Mouse: relative move, drained and clamped per report, remainder carried over.
  h.reset();
  line(h, "p 10 -5;c 0 1;s 3");
  h.mouseReport(b);
  CHECK(b[0] == 2 && b[1] == 1 && (int8_t)b[2] == 10 && (int8_t)b[3] == -5 && (int8_t)b[4] == 3);
  CHECK(!h.mouseDirty);                                       // nothing left over
  line(h, "p 300 0");                                         // bigger than one report can carry
  h.mouseReport(b);
  CHECK((int8_t)b[2] == 127 && h.mouseDirty);                 // sends 127, keeps the rest
  h.mouseReport(b);
  CHECK((int8_t)b[2] == 127);                                 // the next chunk
  line(h, "c 0 0");
  h.mouseReport(b);
  CHECK(b[1] == 0);

  // "release everything" clears all three.
  h.reset();
  line(h, "k 4 1;b 0 1;c 0 1;a 0 100");
  line(h, "x");
  h.keyboardReport(b); CHECK(b[1] == 0 && b[3] == 0);
  h.gamepadReport(b); CHECK(b[6] == 0 && (int8_t)b[1] == 0);
  h.mouseReport(b); CHECK(b[1] == 0);

  // A batched line with several commands applies them all; junk is skipped, never crashes.
  h.reset();
  line(h, "k 4 1 ; k 5 1 ; zzz ; b 3 1 ; ");
  h.keyboardReport(b);
  held = 0; for (int i = 3; i < 9; i++) held += b[i] != 0;
  CHECK(held == 2);
  h.gamepadReport(b);
  CHECK(b[6] == (1 << 3));
  // random bytes never crash the parser
  srand(1);
  for (int i = 0; i < 100000; i++) {
    char junk[40];
    int n = rand() % 39;
    for (int j = 0; j < n; j++) junk[j] = (char)(rand() % 256);
    junk[n] = 0;
    HidState t;
    t.applyLine(junk);
    t.keyboardReport(b); t.mouseReport(b); t.gamepadReport(b);
  }
  CHECK(true);

  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
