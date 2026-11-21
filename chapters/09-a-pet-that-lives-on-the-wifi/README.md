# 9 · A pet that lives on the WiFi

> **Needs:** one ESP32 DevKit V1, a phone, and one or more computers (macOS, Windows or Linux)
> on the same WiFi.
> **Status:** flashed, and running on a real board with real computers checking in. The pet's
> life and its moves between computers are tested on a laptop over a simulated week; the desktop
> helper, the installer and the phone page are tested end to end against a stand-in board
> (figures below).

A pet that lives on your computer screen, over whatever you have open — a **border collie
puppy, a kitten, a bunny, a fox kit or a panda cub**. Put it wherever you like and it stays there, breathing, blinking,
looking about. Every few minutes it gets up, walks off the edge, and turns up walking in on
**another computer** in the house, to its spot there. Its hunger, happiness and energy run on the board, and so does the one thing that
makes it a pet rather than a screensaver: it is in **one place at a time**. A phone opens its page
to adopt it, feed it, play, put it to bed, or call it to a particular screen.

It runs on your **home WiFi** (`http://pet.local`), or makes its own network **`devkit-pet`** as
a fallback, the same as chapters 7 and 8.

---

## What's different this time: something on the computer

Every chapter so far needed nothing but a browser. This one can't. A web page can only draw
inside its own tab. Nothing in a browser can put a window on top of other apps, let alone move it
along the bottom of the screen. So each computer runs a small **helper**: one Python file that
opens a borderless, always-on-top, transparent window the size of the pet and moves it around.
Inside that window is a tiny web view running the very same drawing code the phone page uses, so
the pet looks identical everywhere.

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
2. installs the window toolkit, **PyQt6**, and its web view, **PyQt6-WebEngine**, which draws the
   pet (a one-off download of a few minutes, about 150 MB; if the web view can't be installed, the
   pet still works and shows as an emoji);
3. fetches the helper from the board;
4. sets it to **start by itself at login**: a LaunchAgent on macOS, a Startup shortcut on
   Windows, an autostart entry on Linux. It starts it straight away too.

Run it once and you are done; it survives reboots. Running it again is safe (it updates the
helper). If Windows has no Python, the installer installs it for that user with `winget`. On a
Mac that has never had the developer tools, macOS offers to install them, and you run the
command again once they're in.

To take the pet off a computer later, the installer's last line tells you how; it's the same
shape of command: `curl -fsSL http://pet.local/u | sh`, or `irm http://pet.local/u.ps1 | iex`
on Windows. It removes the helper, its login item and its private environment. The pet itself
lives on the board and isn't touched.

To take it off a computer: `curl -fsSL http://pet.local/u | sh` (or
`irm http://pet.local/u.ps1 | iex`). The pet itself stays on the board.

The first time it runs, macOS may ask whether Python may **find devices on your local network**,
and Windows may ask about the firewall. Allow it: that's how the helper reaches the board.

## On the screen

**What it looks like.** Five baby animals drawn as animated characters: a **border collie
puppy** (black and white, folded ears, a white-tipped plume of a tail), a fluffy white **kitten**
with blue eyes, a cinnamon **bunny** with tall pink-lined ears, an orange **fox kit** with a white
mask and black-tipped ears, and a chubby **panda cub**. They're drawn with care for light and
volume rather than flat colour: soft shading, a light catching the top edges, fur with a sheen and
fluffy edges, glossy eyes with depth (iris detail, catchlights, the upper lid's shadow), blush on
the cheeks, and a faint light outline so even the black puppy shows on a dark desktop.

They're alive in small ways: they blink, breathe, wag or sway a tail, tilt their heads when
curious (the eyebrows lift), and their ears bounce and settle after they move. Walking is a real
trot with short chubby legs bending at the knee; the bunny hops. Stroked, they close their eyes in
a smile, hop and send up hearts; fed, they chew with their own food above them (🦴 🐟 🥕 🍗 🎋).

All of it is one file, **`helper/pets.js`**: the drawing and the animation, used by both the phone
page (on a canvas) and the desktop helper (in its web view). There are no images; every pet is
drawn from shapes, so it stays sharp at any size, and the whole of it is about 36 KB.

**How big it is.** Medium by default (about 120 px tall). The **Size on screen** slider on the
phone page goes from small (70 px) to huge (240 px). The board keeps the setting, and every
computer resizes the pet within a second. It walks at a speed that matches its size.

**Where it is: wherever you put it.** Each computer has the pet's own **spot**, bottom right
until you choose. **Drag it** anywhere — the middle of the screen, the right edge, up on top of
a window — and let go: it stays exactly there, and that's its spot on that computer from now on
(remembered in `~/.devkit-pet/spot`). It doesn't wander about. While it stays, it does small
things: breathes, blinks, sways its tail, turns to look the other way, does a little hop.

**It only walks to travel.** Every three to ten minutes, if another computer is online, it gets
up, walks from its spot to the nearer edge of the screen, and goes. On the next computer it walks
in from the opposite edge to *its spot there*, and settles. With no other computer online, it
never walks at all. You can catch it as it sets off and drop it back: that cancels the trip.

- Its window is only as big as the pet, so it never blocks a click anywhere else, and it never
  takes the keyboard focus.
- **Click it** to stroke it (hearts). **Right-click** for its page, *Send to another computer*,
  *Pause here for 15 minutes*, or *Quit*.
- **Asleep**, it lies down on its spot with a drifting *z Z*. **Hungry**, it asks with its
  favourite food in a bubble now and then: 🐟 🦴 🥕 🍗 🎋.
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
| `/adopt` | POST | `n`, `k` | a new pet: name, and `k`=0 cat, 1 dog, 2 bunny, 3 fox, 4 panda |
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
helper/pets.js         the five pets: the drawing and the animation, for the page and the helper
helper/embed.py        copies pets.js into the helper and the page, and regenerates files.h —
                       run it after changing anything in helper/
```

## Limits

- **Python and a download.** The helper needs Python 3.9+, PyQt6 and PyQt6-WebEngine. The
  installer handles all three, but the first install needs the internet and about 150 MB, and
  the helper's web view uses some memory (as any small web app does). If the board is on its own `devkit-pet` network,
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
warnings from the sketch: **1,117,940 bytes — 85% of 1.31 MB** (the helper and installers it
serves, and pets.js in both the page and the helper, are about 110 KB of that), globals at **15% of RAM**.

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
on Qt's offscreen screen (the window logic; the drawing is checked natively, below). Adopted, it walked in to its spot and then stayed perfectly
still; after a while it travelled, walking off one edge and in on the other computer, only ever on
one screen. A click registered as a stroke. Dragged up and to the left and dropped, it stayed
exactly there and saved that as its spot; called away from the phone it left, and called back it
walked in to that same spot and settled. Put to bed, it slept on its spot; woken, it stayed put.
Set to 200 px from the phone, it grew without leaving its spot. **12 of 12 checks, on two runs
in a row.** Writing the test caught two real bugs before they shipped:
a method named `event` that quietly replaced Qt's own, and the macOS-only window calls running on
a non-macOS backend.

**The helper on a real screen**, natively on macOS against the stand-in board, once for each of
the five pets: the web view drew the pet exactly as the phone page does, on a fully transparent
window; a click inside the web view came back to Python, reached the board as a stroke, and the
pet answered with a smile, a hop and hearts.

**The installer, end to end,** run for real (`curl … | sh`) into a sandboxed home folder with
`launchctl` stubbed: it made the private environment, installed PyQt6 6.10 and PyQt6-WebEngine, fetched a helper
byte-for-byte identical to the source, saved the board's addresses, wrote a valid LaunchAgent
that starts at login and restarts on a crash, and started it; running it a second time was
fine; the uninstaller removed everything. **10 of 10.**

**The phone page**, in a browser against the stand-in board with two computers checking in:
adopting a panda through its own buttons, feeding it, moving the size slider, and calling it to
the other computer all reached the board.

**The drawing**, all fifteen poses (five pets walking, sitting and sleeping) rendered from pets.js
in a browser and in the helper's web view: the same picture, because it's the same code.

**On hardware:** flashed and running; a Mac and a second computer have checked in with it and the
pet has been stroked on screen. **Not yet confirmed:** this version on the real board and real
computers; the Windows installer and helper (written for PowerShell 5.1 and `winget`, not run);
Linux; and a full day of a pet moving between real screens.

## Sources

- The ESP32 Arduino core's `WebServer`, `Preferences` and `esp_random()`.
- Qt 6 / PyQt6: frameless translucent always-on-top tool windows, `WA_MacAlwaysShowToolWindow`;
  Qt WebEngine with a transparent page background for the pet itself.
- Reference photos of border collie puppies, kittens, rabbits, fox kits and panda cubs on
  Wikimedia Commons, used to get the markings and proportions right (none are included).
- AppKit, through `ctypes`: `NSApplication.setActivationPolicy` (no Dock icon) and
  `NSWindow.collectionBehavior` (every desktop, over full-screen apps).
- Chapter 7 and 8 for the dual-network `wifi_setup.h`; chapter 8 for the referee shape.
