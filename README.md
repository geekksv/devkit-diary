<!--
  The front page of a book, not a repo readme with chapters bolted on.

  Cover and progress bar are SVGs in assets/, drawn by the scripts in scripts/ and committed.
  Nothing on this page is fetched from a chart service at render time.

  Chapters are linked in the contents only once their folder exists. A table of contents full
  of 404s is worse than one that admits what has not been written yet.
-->

<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)"  srcset="assets/cover-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="assets/cover-light.svg">
  <img src="assets/cover-dark.svg" width="860" alt="devkit-diary — what one ESP32 can do with nothing attached to it">
</picture>

<br><br>

<picture>
  <source media="(prefers-color-scheme: dark)"  srcset="assets/progress-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="assets/progress-light.svg">
  <img src="assets/progress-dark.svg" width="720" alt="writing progress">
</picture>

</div>

---

## The premise

Every ESP32 guide starts with a shopping list. A sensor, a display, a breadboard, jumper
wires, and then — eventually — the board itself.

This one never does. Everything in this book runs on a single ESP32 DevKit V1 with **nothing
attached to it**. No modules, no breadboard, no wires. The only thing outside the board that
any chapter touches is a WiFi router, and you already have one of those.

It turns out this is not much of a limitation. Inside that module there is a second radio you
probably have not used, ten pins that sense a hand before it touches them, a third processor
that keeps running after the main two have gone to sleep, and a way to notice someone walking
past on the other side of a wall. Most project guides never reach any of it, because reaching
it does not require buying anything.

## How to read this

**Chapters stand alone.** Start anywhere. Each one says what it does, what the board is
actually doing underneath, and where it went wrong.

**The failures are kept.** Every chapter has a `notes.md` — the real log, written while
building, including the hours lost to things that turned out to be a bad USB cable. An
account where nothing goes wrong is not an account of building anything.

**Part IV is on purpose.** Three chapters about things that do not work: a sensor the
manufacturer deleted, a thermometer that cannot measure a room, and the three ways this board
restarts without being asked. Knowing where the floor gives way is worth as much as the rest.

**[The full catalogue is in PROJECTS.md →](PROJECTS.md)**
What every chapter does and why, with no code. **[The board itself is in HARDWARE.md →](HARDWARE.md)**

---

## Contents

### Part I · Things that should not be possible

| | | |
|---|---|---|
| 1 | [Seeing through a wall with WiFi](chapters/01-seeing-through-a-wall/) | ◐ in progress |
| 2 | The board can hear electricity | ○ planned |
| 3 | A theremin with no instrument | ○ planned |
| 4 | Finding things you lost | ○ planned |
| 5 | Knowing where you are without GPS | ○ planned |
| 6 | The computer that runs while asleep | ○ planned |
| 7 | Watching a clock go wrong | ○ planned |

### Part II · Things you will actually keep using

| | | |
|---|---|---|
| 8 | The slide clicker | ○ planned |
| 9 | Proof the internet went down | ○ planned |
| 10 | A quiz for a room with no internet | ○ planned |
| 11 | A chat room that needs nothing | ○ planned |
| 12 | Phones as controllers | ○ planned |
| 13 | Drop a file, grab it on the other device | ○ planned |
| 14 | Extending the WiFi that does not reach | ○ planned |
| 15 | The last time you need the cable | ○ planned |

### Part III · Measuring things you cannot see

| | | |
|---|---|---|
| 16 | Which channel is actually free | ○ planned |
| 17 | Walking the signal | ○ planned |
| 18 | A census of the air | ○ planned |
| 19 | What sleep really costs | ○ planned |
| 20 | Two cores, honestly | ○ planned |
| 21 | Is it actually random? | ○ planned |

### Part IV · Dead ends, kept on purpose

| | | |
|---|---|---|
| 22 | The sensor Espressif deleted | ○ planned |
| 23 | The thermometer that lies | ○ planned |
| 24 | Brownout, watchdog, reboot | ○ planned |

<sub>● written · ◐ in progress · ○ planned</sub>

---

## Colophon

| | |
|---|---|
| Board | ESP32 DevKit V1 — ESP32-WROOM-32, 30-pin |
| Core | arduino-esp32 3.x · *exact version pinned in [HARDWARE.md](HARDWARE.md)* |
| Cover | drawn by `scripts/cover.py`, not photographed |
| Progress | drawn by `scripts/progress.py`, counted from `chapters/` rather than hand-kept |
| Licence | MIT — take any of it |

The drawing scripts need Python and nothing else:

```
python scripts/cover.py
python scripts/progress.py
```

Written in the open, one chapter at a time, by **[Keshav Kumar](https://github.com/geekksv)**.
