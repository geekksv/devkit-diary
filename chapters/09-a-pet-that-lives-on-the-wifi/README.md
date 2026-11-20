# 9 · A pet that lives on the WiFi

> **Needs:** one ESP32 DevKit V1, a phone, and one or more computers (macOS, Windows or Linux)
> on the same WiFi.
> **Status:** compiles against the core. The pet's life and its moves between computers are
> tested on a laptop over a simulated week. The desktop helper, the installer and the phone page
> are tested end to end against a stand-in board (figures below). Not yet flashed.

A cat or a dog that walks along the bottom of your computer screen, over whatever you have
open. Now and then it wanders off the edge, and turns up walking in on **another computer**
in the house. Its hunger, happiness and energy run on the board, and so does the one thing that
makes it a pet rather than a screensaver: it is in **one place at a time**. A phone opens its page
to adopt it, feed it, play, put it to bed, or call it to a particular screen.

It runs on your **home WiFi** (`http://pet.local`), or makes its own network **`devkit-pet`** as
a fallback, the same as chapters 7 and 8.

---

## What's different this time: something on the computer

Every chapter so far needed nothing but a browser. This one can't. A web page can only draw
inside its own tab. Nothing in a browser can put a window on top of other apps, let alone move it
along the bottom of the screen. So each computer runs a small **helper**: one Python file that
opens a borderless, always-on-top window the size of the pet and walks it around.

The board stays the brain. The helper decides nothing about the pet. It only draws what the
board says and reports two things back: "it walked off my screen on this side" and "someone
clicked it".

### Adding a computer: one command, once

The phone page shows the command, with a **Copy** button:

```sh
curl -fsSL http://pet.local/i | sh          # macOS or Linux, in Terminal
```
```powershell
irm http://pet.local/i.ps1 | iex            # Windows, in PowerShell
```

**The board itself serves the installer and the helper**, so the version on the computer always
matches the firmware, and the installer knows the board's address without being told. It fills
in both `pet.local` and the board's plain IP, for networks that won't resolve `.local` names.
The command:

1. makes a **private Python environment** in `~/.devkit-pet` (`%LOCALAPPDATA%\devkit-pet` on
   Windows), so nothing is installed system-wide and no admin password is asked for;
2. installs the window toolkit, **PyQt6**, into it (a one-off download of a minute or two);
3. fetches the helper from the board;
4. sets it to **start by itself at login**: a LaunchAgent on macOS, a Startup shortcut on
   Windows, an autostart entry on Linux. It starts it straight away too.

Run it once and you are done; it survives reboots. Running it again is safe (it updates the
helper). If Windows has no Python, the installer installs it for that user with `winget`. On a
Mac that has never had the developer tools, macOS offers to install them, and you run the
command again once they're in.

To take it off a computer: `curl -fsSL http://pet.local/u | sh` (or
`irm http://pet.local/u.ps1 | iex`). The pet itself stays on the board.

The first time it runs, macOS may ask whether Python may **find devices on your local network**,
and Windows may ask about the firewall. Allow it: that's how the helper reaches the board.

## On the screen

**What it looks like.** An illustrated pet, not pixels: an orange tabby cat with green eyes,
stripes and whiskers, or a golden pup with floppy ears and a pink tongue. It's drawn from smooth
shapes with soft shading, so it stays sharp at any size. It's alive in small ways: legs stride
when it walks, the tail sways (or wags, for the dog), the ears swing, it blinks, and it breathes
when it sits or sleeps. Each pose is a **rig**: a list of shapes, and how each one moves around
its own pivot. The phone page and the desktop helper draw the *same* rig data, one on a canvas
and one with Qt, so they always match.

**How big it is.** Medium by default (about 120 px tall). The **Size on screen** slider on the
phone page goes from small (70 px) to huge (240 px). The board keeps the setting, and every
computer resizes the pet within a second. It walks at a speed that matches its size.

- It **walks** along the bottom of the main screen, above the Dock or taskbar, stops to **sit**,
  and walks on. Its window is only as big as the pet, so it never blocks a click anywhere else,
  and it never takes the keyboard focus.
- **Click it** to stroke it (hearts). **Drag it** to pick it up and drop it (it falls back to the
  floor). **Right-click** for its page, *Send to another computer*, *Pause here for 15 minutes*,
  or *Quit*.
- **Asleep**, it lies down with a drifting *z Z*. **Hungry**, it says so with a little 🐟 or 🦴
  bubble now and then.
- What you do on the phone shows up on the screen where it is: hearts when stroked, a bite to
  eat, a few jumps when you play.
- On a Mac it shows on **every desktop and over full-screen apps**, and it has no Dock icon.

## The thing worth understanding

**A pet that can be in two places isn't a pet. One referee has to own where it is.**

Each computer's helper sends a **beat** to the board about once a second. The board keeps the
list of computers, and exactly one of them holds the pet. Everything that moves it goes through
the board:

- **Walking off a screen.** The helper walks the pet past the edge and tells the board which side
  it left from. The board picks the next computer that's online and records an **arrival** on
  it, coming in from the *opposite* edge, as if the screens stood side by side. Off the right of
  your laptop, in from the left of the desktop. Alone on the network, it turns round and walks
  back in where it left.
- **A computer going quiet.** A computer that hasn't beaten for 10 seconds (asleep, lid closed,
  off the network) is treated as gone. If it was holding the pet, the board hands the pet to
  another computer. If there's none, the pet waits nowhere until one comes back.
- **Called from the phone.** The board moves the pet. The old screen sees "not here any more" and
  its pet runs off the nearest edge, while the new screen sees a new arrival and walks the pet in.

The **arrival counter** is what keeps the screens honest. A helper walks the pet in only when the
number changes, so a slow network, a late beat or a helper restarting can never put two pets on
two screens. It's the same shape as Ludo's referee (chapter 8) and the file drop's pipe
(chapter 7): one authority, thin clients that only draw what they're told.

> **What you learn:** presence by heartbeat, handing one piece of state between clients without
> it ever being in two places, and why that needs a counter rather than a yes/no.

## Its life

All in `pet.h`, with no radio or web in it, so it can be run for simulated days on a laptop:

| | Awake | Asleep |
|---|---|---|
| **Fullness** | empties over about **6 hours** | empties half as fast |
| **Happiness** | fades over about **5 hours**, twice as fast when hungry | holds |
| **Energy** | runs down over about **10 hours** | back to full in about **1½ hours** |

- It **dozes off** when it's exhausted, or at night (22:00–07:00) once it's a little tired. It
  **gets up** in the morning when rested.
- **Woken by hand**, it stays up for 20 minutes before it may doze again.
- **Feed** adds a good meal (not when it's nearly full, or asleep). **Pet** cheers it up, asleep
  or not. **Play** cheers it up a lot but costs energy and a little food (not when it's too
  tired). **Sleep** and **Wake** do what they say.
- Its mood is the most pressing of: *asleep*, *hungry*, *tired*, *lonely*, *happy*, *content*.

The board has no clock of its own, and **no timezone to set**: every beat carries the computer's
local time of day, and the board uses the latest one. Its state is written to flash every five
minutes and after anything you do, so a power cut costs it at most five minutes of its day.

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the phone page |
| `/state` | GET | — | the pet, and every computer it could be on |
| `/adopt` | POST | `n`, `k` | a new pet: name, and `k`=0 cat / 1 dog |
| `/act` | POST | `a` | `feed`, `pet`, `play`, `sleep` or `wake` |
| `/call` | POST | `i` | bring the pet to computer `i` |
| `/size` | POST | `v` | how tall it stands on screen, 70–240 px |
| `/release` | POST | — | say goodbye (then adopt again) |
| `/beat` | GET | `id`, `n`, `h` | a computer checks in (id, name, local hour) → is the pet here, the arrival count and side, its state |
| `/leave` | POST | `id`, `s` | the pet walked off this screen on side `s` |
| `/poke` | POST | `id` | it was clicked |
| `/i`, `/i.ps1` | GET | — | the installer, with the board's addresses filled in |
| `/u`, `/u.ps1` | GET | — | the uninstaller |
| `/devkit_pet.py` | GET | — | the helper |

## Files

```
src/pet/pet.ino        the board: life ticking, beats, handoffs, storage, serving everything
src/pet/pet.h          the pet's life and where it is — the part tested on a laptop
src/pet/pet_page.h     the phone page
src/pet/files.h        GENERATED: the helper and installers, for the board to serve
src/pet/wifi_setup.h, improv.h, ui.h   home WiFi or its own network (shared with chapters 7, 8)
helper/devkit_pet.py   the desktop helper
helper/install.sh, install.ps1, uninstall.sh, uninstall.ps1
helper/art.py          the cat and dog as rigs of shapes; writes them into the helper and the page
helper/embed.py        regenerates files.h — run it after changing anything in helper/
```

## Limits

- **Python and a download.** The helper needs Python 3.9+ and PyQt6. The installer handles both,
  but the first install needs the internet. If the board is on its own `devkit-pet` network,
  install while the computer is still on your normal WiFi (or put the board on your home WiFi,
  which is the better way to run this chapter anyway).
- **The main screen only,** along its bottom edge. It doesn't climb onto other windows' title
  bars. That would need macOS accessibility permissions just to find the windows, and it's left
  out.
- **Linux:** fine on X11. Under Wayland a window may not place itself, so the helper runs through
  XWayland. Some distributions need `libxcb-cursor0` for Qt's X11 support.
- **Eight computers.** A ninth waits until one has been gone a while.
- **Anyone on the network can look after the pet,** and a computer could claim to be another.
  It's a pet, on your own WiFi; there are no accounts.

## Running it

**From the flasher:** choose *A pet that lives on the WiFi*, *Flash*, and use the WiFi card for
your home network. Then open `http://pet.local` on your phone, adopt a cat or a dog, and run the
command it shows on each computer.

**From the Arduino IDE:** open `src/pet/pet.ino`, choose **ESP32 Dev Module**, default
partition; every `.h` sits beside the `.ino`.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module on the default partition, with no
warnings from the sketch: **1,081,008 bytes — 82% of 1.31 MB** (the helper and installers it
serves are about 50 KB of that, the pets' drawings included), globals at **15% of RAM**.

**The pet's life and its moves**, on a laptop, one rule at a time: hungry after a working
morning, fed back up, refusing food when full; dozing at night, staying up when woken by hand
and then dozing again, up just after seven and staying up; too tired to play twice, napping when
exhausted. Then the moves: the first computer to check in gets the pet; off one right edge and in
from the next left edge, and back; a stale "it left" from a computer that no longer has it
changes nothing; alone, it turns round; called from a phone; its computer going quiet sends it to
another; every computer gone leaves it nowhere until one returns; a ninth computer waits for a
slot. Then **a simulated week** of four computers dropping in and out at random, random care and
2,016 walks between screens: every stat stayed in range, and the pet was never left on a
computer that had gone.

**The helper, end to end,** against a stand-in board that speaks the same JSON and follows the
same rules: two copies of the real helper as two "computers", "Kesh-MacBook" and "Office-PC",
on Qt's offscreen screen. Adopted, the pet walked onto one screen, sat, wandered off its edge,
and walked in on the other from the opposite side, only ever on one screen. A click registered as
a stroke on the board. Called from the phone, it ran off one screen and onto the other. Put to bed,
it lay down; woken, it got up. When its computer went quiet, it left, and the board brought it
to the other one. Set to 200 px from the phone, it grew on screen. **12 of 12 checks.** The test also caught two real bugs before they shipped:
a method named `event` that quietly replaced Qt's own, and the macOS-only window calls running on
a non-macOS backend. The helper also ran natively on macOS against the stand-in for nine
seconds: it checked in, held the pet, and printed nothing.

**The installer, end to end,** run for real (`curl … | sh`) into a sandboxed home folder with
`launchctl` stubbed: it made the private environment, installed PyQt6 6.10, fetched a helper
byte-for-byte identical to the source, saved the board's addresses, wrote a valid LaunchAgent
that starts at login and restarts on a crash, and started it; running it a second time was
fine; the uninstaller removed everything. **10 of 10.**

**The phone page**, in a browser against the stand-in board with two computers checking in:
adopting a dog through its own buttons, feeding it, moving the size slider, and calling it to
the other computer all reached the board.

**The drawing**, rendered both ways (the browser's canvas and the helper's QPainter) for all six
poses, both ways round: the same picture.

**Not yet confirmed:** any of it on a real board; the Windows installer and helper (written for
PowerShell 5.1 and `winget`, not run); Linux; the first-run network prompts on each OS; and how
it feels to have a pet wander between real screens for a day.

## Sources

- The ESP32 Arduino core's `WebServer`, `Preferences` and `esp_random()`.
- Qt 6 / PyQt6: frameless translucent always-on-top tool windows, `WA_MacAlwaysShowToolWindow`.
- AppKit, through `ctypes`: `NSApplication.setActivationPolicy` (no Dock icon) and
  `NSWindow.collectionBehavior` (every desktop, over full-screen apps).
- Chapter 7 and 8 for the dual-network `wifi_setup.h`; chapter 8 for the referee shape.
