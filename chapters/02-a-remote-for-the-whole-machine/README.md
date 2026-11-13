# 2 · A remote for the whole machine

> **Needs:** one ESP32 DevKit V1. A laptop to pair it to, and a phone to drive it from.
> **Status:** flashed and working. On the **Huge APP** partition (the default overflows — see
> below) it pairs to a laptop with no PIN, serves the page to a phone, and the buttons drive the
> laptop. Not yet checked: the two touch pins, and the boot heap figure.

Pair the board to a laptop the way you would pair any Bluetooth keyboard. From then on it can
press keys on that laptop — move slides, work the media keys, type whole sentences. The buttons
live on a web page the board serves to your phone over its own WiFi. Tap a button on the phone,
a key is pressed on the laptop.

The board is holding both radios open at once and bridging them. The phone never speaks to the
laptop; only the board does, and only as a keyboard the laptop already trusts.

---

## What makes it different from the other chapters

Every sketch so far was the board as a server: it made a network, something joined it, and the
thing it served **was** the output. Chapter 1's chat room is the whole of chapter 1 on one
screen.

This one splits in half. The page the board serves to your phone is not the output — it is the
**control surface**, the row of buttons. The output is on a different screen entirely, the
laptop's, and the board reaches it over a different radio. The first time you watch a tab open
on your laptop because you tapped your phone, the two-radio bridge stops being a diagram and
becomes obvious.

It is also the only chapter that runs WiFi and Bluetooth together. That is not free, and the
section on coexistence below is about the bill.

## How it works

**Two radios, one chip, at the same time.**

- **`WIFI_AP` + `WebServer` + `DNSServer`** — the same captive-portal trio as chapter 1. The
  phone joins `devkit-remote`, the page opens by itself, and every button is a `fetch` to the
  board.
- **`BLEHIDDevice`** — the board advertises itself as a Bluetooth keyboard. The laptop pairs
  it with no PIN (`ESP_IO_CAP_NONE`, "just works" bonding, exactly like a shop-bought
  keyboard), remembers it, and reconnects on its own next time.

A request lands on the board over WiFi; the board translates it to a HID report and notifies it
over Bluetooth; the laptop's input stack treats that report as a key press. Nothing on the
laptop can tell it apart from a key pressed on a real keyboard, because at the HID layer there
is nothing to tell apart.

## The thing worth understanding

The laptop accepts this board as a keyboard because **it says it is a keyboard**, and nothing
checks.

There is no step in the pairing where the laptop verifies the claim — HID has no such step, by
design, because a keyboard is the thing you would use to approve the keyboard. The device
declares a *report map* (here, "I am a keyboard, and separately a set of media keys"), the OS
files it under keyboards, and from then on it believes whatever reports arrive. The whole of
`HID_REPORT_MAP` in the sketch is the board making that declaration.

This is the same mechanism behind the standing advice never to plug in a USB stick you found in
a car park: the port cannot tell a keyboard from a storage device from a thing that is both,
because the device is the only one who says. Here it is doing something you asked for, on a
laptop you own, after a pairing you approved — but the reason it *can* is the same reason that
advice exists.

> **What you learn:** how a device declares what it is, and that nothing downstream verifies
> the claim.

## Two reports in one device

A HID device describes itself with a *report map*. This board's declares two reports:

| Report ID | What it is | Why both |
|---|---|---|
| 1 | **Keyboard** — modifier byte, a reserved byte, six key slots | arrows, letters, function keys, typed text |
| 2 | **Consumer control** — one 16-bit usage | the media keys |

They are separate on purpose. The arrow keys and letter keys are *keyboard* usages, and they
only do anything in whatever app has focus — Right Arrow moves a slide only if the slideshow is
in front. The media keys live on a different HID page, **Consumer**, and the OS routes those
system-wide: Play/Pause reaches whatever is playing, even when the browser is buried three
windows down. That is why the media row on the page is the reliable one, and the slide row is
not — it has nothing to do with Bluetooth and everything to do with which usage page the key
comes from.

## The report carries no ID — a detail that bites

Each input report here installs a **Report Reference descriptor** (`inputReport(1)` and
`inputReport(2)` in the sketch). That descriptor is where the report ID lives, so the host
learns it during enumeration. The value the board then notifies is **just the data** — eight
bytes for a key, two for a media usage — with **no report-ID byte in front**.

This is worth stating because the core's own `Server_Gamepad` example does the opposite: it
prepends the report ID into the payload. With a single gamepad and a lenient test page that
survives, because a byte of slop reads as a slightly-off joystick axis. A keyboard host is not
lenient — an extra leading byte shifts the modifier into the reserved slot and the key into a
modifier, and the press either does nothing or does the wrong thing. The sketch follows the
spec (and the common keyboard libraries), not the example.

## Typing without a lookup table of my own

Typing a URL or a sentence means turning each character into a key position and a shift state —
`y` is usage `0x1C` with no modifier, `!` is `0x1E` with Shift. The core already ships that
table: `HIDKeyboardTypes.h` carries a 128-entry ASCII map, compiled in only when you
`#define US_KEYBOARD` first. The sketch reuses it rather than hand-rolling one, which keeps the
promise the book makes everywhere — nothing to install beyond the board support.

The limit comes with it: it is a **US layout**. Characters it cannot reach, and anything that
needs a dead key or a different physical layout, are skipped rather than mistyped. A `£` on a
UK-set laptop will not come out right, and the honest fix is a layout the board cannot know
without being told.

## The buttons

| Group | Buttons | Sends |
|---|---|---|
| **Slides** | Back · Next · Start · Black · Exit | Left/Right arrow, F5, B, Esc — *keyboard* |
| **Media** | ⏮ · ⏯ · ⏭ · Vol− · Mute · Vol+ | Consumer usages — *system-wide* |
| **Browser** | open a site · New tab · Close tab · Fullscreen | Ctrl/Cmd+T, Ctrl/Cmd+W, F11 |
| **Type** | a text box | each character, as a key press |

**The PC / Mac switch** flips exactly one thing: the command key the browser shortcuts use —
Control on a PC, Command on a Mac. Arrows and media keys are identical on both, so the switch
touches only New tab, Close tab, Fullscreen and the address-bar step of "open a site".

## "Open a site" is the flashy one, and the fragile one

Tap **YouTube** and a new tab opens on the laptop with youtube.com loading. It looks like magic
and it is held together with tape: there is no HID key for "launch the browser", so the macro
assumes a browser window is already the focused app. It presses Ctrl/Cmd+T — which puts the
cursor in the address bar on every major browser — waits 300 ms for that to land, types the
address, and presses Enter.

If the focused app is not a browser, it types the address into whatever is. The pause is a
guess that is usually right and occasionally too short. This is kept, not hidden, because it is
the honest shape of the trick: a keyboard can only press keys, and "open a site" is a little
play performed on the assumption that the stage is set.

## The no-phone clicker

Two capacitive touch pins mean the board also works with no phone at all: hold it like a remote
and your thumb finds **GPIO 32 (next)** and **GPIO 33 (previous)** on the edge header. They are
next to each other, and neither is a strapping pin or wired to the flash, so they are safe to
touch.

This is the secondary input and the less reliable one, and the chapter keeps it partly to make
that point. A touch on the classic ESP32 reads *lower* than rest — the opposite direction from
the S2 and S3 — and "rest" is not a fixed number. It drifts with temperature, and with whether
the board is on USB or a battery. The sketch learns a baseline at boot (`calibrate()`) and
fires when a reading falls below 60% of it, which is a wide margin chosen to tolerate some drift
without triggering on noise. Leave the board running for an hour and the baseline it measured
cold may no longer fit. **This is exactly why a bought remote has real buttons** — a mechanical
switch does not drift.

## Coexistence: the bill for two radios

WiFi and Bluetooth share one antenna and one 2.4 GHz front end. Running both means the radio is
time-sliced between them, which the core supports because it is compiled with
`CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y` — you can read that flag out of the installed
`sdkconfig`. Without it, bringing up the second radio would fail outright.

With it, both run, but neither gets the whole radio. In practice:

- The control page feels a touch less instant than chapter 1's chat, because serving your phone
  and notifying the laptop are taking turns on the same hardware.
- This is the **heaviest sketch in the book** for memory. The free heap at boot is printed on
  the serial line on purpose — it is the first number to watch if the board starts rebooting
  under load, and the one figure here that only the first flash can fill in, since it is a
  run-time measurement rather than something the compiler reports.

An honest chapter about a board doing two things it was not quite built to do at once has to
include the cost, not just the trick.

## It does not fit the default partition — and that is the first lesson of the build

The very first time this compiled, it failed, and not for a bug:

```
Sketch uses 1710515 bytes (130%) of program storage space. Maximum is 1310720 bytes.
Sketch too big
```

Nothing is wrong with the code — there is simply too much of it. The Bluetooth stack
(Bluedroid) and the full WiFi stack are *both* linked in, and together the binary is about
**1.71 MB**. The board's default partition layout — "Default 4MB with spiffs" — gives the
application only **1.31 MB**, because it reserves the rest for a second copy for over-the-air
updates and a filesystem. Every other sketch in the book fits inside that 1.31 MB with room to
spare. This one is the first that does not, and the reason is exactly the two-radios trick the
chapter is about: you pay for the second stack in flash whether you like it or not.

The fix is a board menu, not a code change. **Tools → Partition Scheme → "Huge APP (3MB No OTA
/ 1MB SPIFFS)"** hands the application 3 MB by giving up the OTA slot this sketch was never
going to use. With that selected the same binary reports **1,710,547 bytes — 54% of 3 MB** —
and global variables sit at 19% of RAM, leaving about 258 KB for the stacks and buffers the two
radios need at run time. Giving up OTA is the honest trade: a remote you reflash over USB does
not need the ability to update itself wirelessly, and chapter 14 — the over-the-air one — is
where that slot earns its keep instead.

> **What you learn:** that "it compiled" and "it fits" are two different questions, and that the
> partition table is a real decision, not a default to ignore.

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the control page; also served for any unknown path, so portal probes work |
| `/state` | GET | — | `{ble, stations, heap, up}` — is a laptop connected, and board health |
| `/key` | POST | `k`, `os` | one named action — slide, media or browser key |
| `/type` | POST | `x` | types the text onto the laptop |
| `/open` | POST | `u`, `os` | new tab, type the address, Enter |

Every action that reaches the laptop first checks a host is connected; with none, it answers
`{ok:false, err:"noble"}` and the page shows **no laptop paired** rather than tapping into
nothing. The page polls `/state` so that pill stays honest even if the laptop wanders out of
range.

## Limits

- **Pair it to one laptop at a time.** It bonds with whoever pairs it; point it somewhere else
  and you re-pair. That is how a single keyboard behaves.
- **US keyboard layout only** — see the typing section. Non-US characters are skipped.
- **"Open a site" assumes a browser is focused.** It is a macro, not a protocol.
- **The touch pins drift.** Fine for a demo, not a product; the phone is the dependable input.
- **No encryption of the control page.** Anyone on the board's WiFi can drive the laptop.
  Set `AP_PASS` to 8+ characters if the room is not yours.

## Running it

Open `src/remote/remote.ino` in the Arduino IDE, choose **ESP32 Dev Module**, and — this one
matters here and nowhere else in the book — set **Tools → Partition Scheme → "Huge APP (3MB No
OTA / 1MB SPIFFS)"**, or the build overflows the default partition (see above). `ui.h` must sit
beside the `.ino`. Then:

1. On the **laptop**: Bluetooth settings → add device → pick **devkit-remote**. It pairs with
   no PIN.
2. On the **phone**: join the WiFi **devkit-remote**; the page should open by itself, or visit
   `http://192.168.4.1`.
3. The page's top-left pill turns green when the laptop is connected. Tap away.

Serial at 115200 prints both names and the free heap on boot.

## Verified, and not

The HID report map, the two-report (keyboard + consumer) split, the report-reference descriptor
behaviour that keeps the report ID out of the payload, the bundled `US_KEYBOARD` ASCII table,
and the coexistence flag were all read out of the installed core (3.3.11). The sketch compiles
against it for ESP32 Dev Module — overflowing the default partition at 1.71 MB and fitting Huge
APP at 54% of 3 MB, both found by compiling, not guessed.

Then it was flashed. **Confirmed on hardware:** the "just works" pairing is accepted with no
PIN prompt, the laptop files the board under keyboards, the phone gets the page over the board's
WiFi, and the buttons move slides, work the media keys, open a site and type onto the laptop —
all with both radios up at once. Still an expectation: the **two touch pins** (the no-phone
clicker was not exercised, so its calibration on warm hardware is untested), and the exact
**free heap at boot**, which prints on the serial line but has not been written down here yet.

## Sources

- [HID over GATT (HOGP) — report reference descriptors](https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile-1-0/)
- [USB HID Usage Tables — Keyboard (0x07) and Consumer (0x0C) pages](https://usb.org/document-library/hid-usage-tables-15)
- The ESP32 Arduino core's bundled `BLEHIDDevice`, `HIDKeyboardTypes.h`, and the
  `Server_Gamepad` example it ships.
