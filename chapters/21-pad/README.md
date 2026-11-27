# 21 · Phone Pad

> **Needs:** one ESP32 DevKit V1 on your home WiFi, a phone, and a PC with Bluetooth.
> **Status:** written and tested on a laptop — the input→HID logic (24 checks) and the whole
> phone → WebSocket → board → HID path, end to end in a browser against a board simulator (a tap
> on "Jump" comes out as the Space key, a stick as a gamepad axis, the trackpad as mouse movement).
> **Not yet confirmed on the board: the Bluetooth pairing and the keys actually landing on a PC,
> and WiFi+Bluetooth latency** — that's the hardware test.

Turn your phone into a **build-your-own game controller** for any PC, with the board as the bridge.
The phone shows a touch controller you design yourself; the board appears to the PC as a real
**Bluetooth keyboard + mouse + gamepad** — no driver, no app, no software on the PC at all.

```
your finger ── WiFi (WebSocket) ──► ESP32 ── Bluetooth HID ──► PC game
```

## Using it

1. Flash **Phone Pad** and give it your WiFi.
2. On the PC: **Bluetooth settings → Add device → "Phone Pad"**. It pairs like a real controller.
3. On the phone: open **http://pad.local**, turn it landscape, and play. The default layout is a
   gamepad (D-pad, four buttons, an aim stick). Press — the PC reacts.

## Design your own controller

Tap **Edit**. Now you can:
- **Add widgets:** buttons, a D-pad, analog sticks, a trackpad, a tilt-steer pad.
- **Move and resize** them by dragging (they snap to a grid).
- **Map each one** (tap the pencil): a button can send a **keyboard key**, a **gamepad button**,
  or a **mouse click**; a D-pad can be WASD, arrows, or a real gamepad d-pad; a stick can be an
  analog stick or WASD; the trackpad moves the mouse (tap to click, two fingers to scroll); the
  tilt pad steers with the phone's motion sensor.
- **Turbo:** any button can auto-repeat.
- **Save, switch and share:** layouts are kept on the board; **Share** gives a code another phone
  can **Import**. Name one per game ("Racing", "Emulator", "Platformer").

Multi-touch throughout — move, jump and shoot at once. A haptic buzz on each press, a keep-awake
lock, and a fullscreen option.

## The thing worth understanding

**Why the board is needed at all.** A phone can't tell a PC "I'm a game controller" — browsers have
no such power — and you don't want to install anything on the PC. The ESP32 bridges the gap: it
speaks **WiFi** to the phone and, to the PC, presents itself over **Bluetooth** as a standard HID
device (keyboard + mouse + gamepad in one). Every PC accepts HID with no driver, exactly like
plugging in an Xbox controller. The board is the translator in the middle.

**Why it's split the way it is.** The phone knows the layout and decides what each widget *means*;
it sends the board tiny **intents** — a few bytes like `k 44 1` (hold the Space key) or
`a 2 114` (right stick to +114). The board (`pad.h`) turns a stream of intents into the three HID
reports the PC reads, and sends them over Bluetooth. Keeping the board "dumb" makes it fast and
makes the logic testable on a laptop.

**Why WebSockets.** A game controller sends many updates a second (an analog stick never stops
moving). A normal web request each time would be far too slow, so the taps ride a **WebSocket** —
one open pipe, a few bytes each way, a few milliseconds.

**The honest latency.** Round trip is WiFi (~10 ms) plus Bluetooth (~10 ms). Great for platformers,
racing, emulators, strategy and casual games; **not** for competitive first-person shooters. The
two radios share one chip, which is the part the hardware test has to prove comfortable.

## Endpoints

| Path | What it does |
|---|---|
| WebSocket `:81/ws` | the controller intents (see `pad.h` for the little language) |
| `GET /status` | is the PC paired, how many messages |
| `GET /layouts` | the saved layout names |
| `GET /layout?name=` · `POST /layout` | load / save a layout |
| `GET /dellayout?name=` | delete one |

## Files

- `src/pad/pad.ino`: the Bluetooth HID device (keyboard+mouse+gamepad), the WebSocket, the flash
  that holds your layouts.
- `src/pad/pad.h`: intents → HID reports, tested on a laptop.
- `src/pad/pad_page.h`: the controller — play mode and the layout builder.
- `src/pad/wifi_setup.h`, `improv.h`, `ui.h`: WiFi setup, shared with the other chapters.
- `tools/pad_test.cpp`: the HID logic on a laptop:
  `c++ -std=c++17 -I../src/pad pad_test.cpp -o t && ./t`

**Partition scheme:** "No OTA (2MB APP/2MB SPIFFS)" — Bluetooth plus WiFi needs the room. The web
flasher sets it.

## Verified, and not

**On a laptop:**
- The HID state machine: keyboard (held keys, 6-key limit, modifiers), mouse (relative moves
  drained and clamped per report), gamepad (16 buttons, 4 axes, a hat), "release everything", and
  100,000 random lines that never crash the parser — **24 checks**.
- **End to end in a browser** against a board simulator that runs the real `pad.h`: a tap on Jump
  came out as the Space key, Run as the Shift modifier, a remapped button as a gamepad button, the
  aim stick as a positive X axis, and the trackpad as mouse movement.

**Not yet confirmed (the hardware test):**
- the PC pairing with the board over Bluetooth, and keystrokes actually landing in a game;
- how low the WiFi+Bluetooth latency really is, and whether the two radios stay smooth together;
- the builder on a real phone (drag, resize, save, share), and tilt steering with a real gyro.

This is Stage "everything" of a staged build: the next passes add polish once the hardware test
confirms the bridge.

## Sources

- The ESP32 core's own `BLEHIDDevice` and the Server_Gamepad example for the Bluetooth HID pattern.
- The USB-HID report descriptors for a boot keyboard, a relative mouse, and a gamepad with a hat.
- ESP-IDF's `esp_http_server` WebSocket support for the low-latency input pipe.
