# Build log — chapter 2

Dated entries, written while building. Failures stay in.

---

## 2026-11-10 — before touching the board

Wrote the chapter's claim down first, deliberately, so it cannot be quietly adjusted later to
match whatever ends up working:

> A single board, with no sensor, can tell an empty room from an occupied one — and survives
> being moved to a different room without rewriting anything.

Open questions going in:

- Does the Arduino core expose CSI at all, or does this need ESP-IDF? Suspect IDF.
- How much data per second, and does serial keep up or does it need buffering?
- The router is the transmitter here. Does it need to be sending anything in particular, or is
  ordinary background traffic enough?
- Where does the board sit? First instinct is "anywhere", and every source says otherwise.

Next: confirm which framework this needs before writing a line, and record the exact core
version in HARDWARE.md.
