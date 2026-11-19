# 8 · The sound of the room

> **Needs:** one ESP32 DevKit V1, and any device with a browser and a speaker — a laptop, or a
> phone with headphones.
> **Status:** compiles against the core; the frame-tally arithmetic and the whole sonification
> are tested on a laptop, and the page renders and runs in a real browser against simulated
> air (figures below). Not yet flashed.

WiFi is a sound you cannot hear. Your router, your neighbours' routers, every phone and laptop
nearby are all broadcasting in the 2.4 GHz band, constantly, right through the room. This
project makes the board **listen** to that activity and a browser **turn it into music**, so you
can finally hear the invisible radio weather around you.

Join the WiFi `devkit-air`, open `http://192.168.4.1`, press **play the air**, and:

- **each network nearby becomes a held note** — its channel is the pitch, its signal is the
  loudness — so the networks around you sound together as a slow, drifting chord;
- **the live traffic on the board's own channel becomes a shimmer** on top, which rises when
  devices are busy and ripples when someone walks between the board and a router.

Leave it on and it plays an ambient piece that is different in every room and never repeats,
composed entirely by the air.

---

## What makes it different from the other chapters

Every chapter so far made the board *do* something — serve a game, refuse a bad move, pipe a
file. This one makes the board let you **perceive** something you never could: the radio ocean
you are swimming in all the time. It is the first chapter whose output is not information but a
*sensation*.

It is also the only chapter that puts the radio in **monitor mode**, and the only one whose
real work happens half on the chip and half in the browser's sound engine.

## It only ever listens

The radio is put in **promiscuous mode** — the mode every WiFi analyser uses — purely to
*count* the frames flying past. It is worth being plain about what that is and is not:

- It **listens**. It never transmits a frame into anyone else's network, never sends a
  deauthentication, never connects to a network it was not invited to. Counting the air is as
  passive as listening to rain.
- The one thing it transmits is **its own open access point**, `devkit-air`, so your phone or
  laptop can join it and hear the result — exactly what any WiFi gadget does.

Monitor mode is the foundation of both WiFi analysis and WiFi attacks; the difference is
entirely in what you do with what you hear. This chapter only hears.

## The thing worth understanding: two layers from one radio

A single radio cannot be in two places at once, and that shapes everything.

**The chord comes from scans.** Every five seconds the board runs a normal WiFi **scan**, which
hops the radio across all thirteen channels and lists every network it finds, with each one's
channel and signal strength. This is the full, accurate picture of the networks around you —
every channel — and it is what the sustained notes are built from. A scan and monitor mode
cannot run at once, so sniffing is switched off for the ~1.5 s a scan takes and back on after;
the chord refreshes, the shimmer takes a breath.

**The shimmer comes from sniffing.** Between scans the radio sits **camped on one channel**
(the board's own, channel 1 by default) in promiscuous mode, and a callback counts every frame
that passes — how many, how big, how strong, of what kind. That live count is the fast,
reactive layer: the pulse of the air on the board's channel, updated four times a second.

So the two layers are honestly different things, and the page uses them for different jobs: a
slow harmonic bed that maps *all* the networks around you, and a quick shimmer that is the live
traffic on the one channel the board can watch continuously. (Which channel it camps on is
`CAMP_CHANNEL` at the top of the sketch; changing it while clients are connected would drop
them, so it is a boot-time choice, not a live one.)

> **What you learn:** monitor mode and WiFi scanning — how a radio watches the air instead of
> joining it — and that one radio means a real trade-off between watching everywhere slowly and
> watching one place continuously.

## The ear: a callback that must do almost nothing

`esp_wifi_set_promiscuous_rx_cb()` registers a function the WiFi driver calls for **every frame
the radio overhears** — hundreds or thousands a second, deep inside the WiFi task. A callback
like that has strict rules: it must not allocate memory, must not print, must not call back into
the network stack, and must return in microseconds, or the driver falls behind and the board
resets.

So the callback (`sniff()`) does exactly one thing: it reads three fields from the frame's
receive-control header —

| Field | What it is |
|---|---|
| `rx_ctrl.channel` | the channel the frame arrived on |
| `rx_ctrl.rssi` | its strength, in dBm (a negative number) |
| `rx_ctrl.sig_len` | its length in bytes |

— plus its class (management / control / data) from the callback's own argument, and **adds one
frame to a tally** (`airstats.h`). Nothing else. All the arithmetic that turns a pile of tallies
into rates happens later, in `loop()`, away from the radio.

The callback writes the tally on **core 0** (the WiFi task); `loop()` reads and clears it on
**core 1**. There is no lock: `loop()` copies the tally and zeroes it, and a frame or two may
fall across that seam. For a stream meant to be *heard*, not audited, that is the right call —
a lock in a thousand-times-a-second callback would cost more than a lost frame is worth. The
chapter says so rather than pretending the numbers are exact.

## The voice: turning numbers into music

The board sends the browser a small bundle of numbers four times a second; the browser makes
the sound, because a browser's Web Audio engine is far better at it than a chip with no speaker.
The mapping (all in the page) is where the craft is:

- **Channel → pitch.** Channels 1–13 are mapped onto a **pentatonic scale** across two octaves —
  channel 1 the lowest note, channel 13 the highest. A pentatonic scale has no dissonant
  intervals, so however the networks fall, the chord stays consonant. (Pick the key in the top
  bar.)
- **Signal → loudness.** −90 dBm (the edge of what the board hears) fades to silence; −45 dBm
  and stronger is full. A strong router is a present note; a far network is a distant whisper.
- **A tiny per-network detune**, derived from the network's address, so two networks on one
  channel beat gently against each other rather than ringing as one dead tone.
- **Channel busyness → vibrato depth.** The live per-channel activity deepens a slow vibrato on
  that channel's notes, so a busy channel glitters while a quiet one sits still.
- **Live traffic → shimmer.** A band of filtered noise, whose loudness follows the overall
  frame rate, is the air's hiss; a sudden burst above the running average rings a soft bell.
- **It never clips.** Every network adds a voice, so the voices share a headroom budget that
  shrinks as more appear (`0.9 / √count`), and a compressor on the master bus catches the rest.
  No matter how crowded the air, the sound stays safe.

Appearing networks fade in over a second or two; vanishing ones fade out and are freed. (The
first version left a voice stuck when a network left at the exact instant the audio clock read
zero — found and fixed by the test below.)

## The picture

The page also *shows* what it hears, so it is worth looking at even muted. Each network is a
glowing orb: its **colour** is its channel (a hue around the wheel), its **x** is the channel
across the width, its **y** is the signal strength (strong high, weak low), its **size** is the
loudness, and it **pulses** with that channel's live activity. Orbs drift to their places, trail
softly, brighten as networks appear and dim away as they go — a constellation that breathes with
the room.

## What the board sends

`GET /air`, four times a second, about 1.5 KB:

```json
{ "camp": 1, "up": 42, "here": 1,
  "live": { "rate": 141, "kbps": 45.9, "busiest": 92, "mgmt": 120, "data": 300, "ctrl": 40,
            "per": [ …13 per-channel frame rates… ] },
  "nets": [ { "s": "HomeWiFi", "b": "4d:ca:…", "c": 6, "r": -51, "o": 1, "h": 0 }, … ] }
```

`s` name, `b` address, `c` channel, `r` signal, `o` secured, `h` hidden. The board's own
`devkit-air` beacon is left out of the list, so it does not sing to itself.

## Limits

- **The sound comes from your device**, not the board — the board has no speaker, and should
  not; a browser makes far better sound. It needs a user tap to start (browsers require that
  for audio), hence the **play** screen.
- **The shimmer is the board's channel only.** The chord covers every channel (from scans); the
  live layer can only watch the one channel the radio is camped on. That is the one-radio
  trade-off, stated honestly.
- **It is an impression, not an instrument.** It is tuned to sound good and to move with real
  change, not to measure anything precisely.
- **No internet, nothing attached** — and no settings; it is meant to be switched on and heard.
- **Monitor mode is passive here.** Nothing in this chapter transmits into another network.

## Where it could go next

1. **Hop the camp channel** on a slow cycle (1 → 6 → 11), so the shimmer samples the whole band
   — at the cost of dropping listeners each hop, so it would need its own standalone mode.
2. **Record a minute** and let you replay or download the piece.
3. **A second instrument for Bluetooth** — the same idea for the BLE devices around you.
4. **Tie the key to the time of day**, so mornings and evenings have their own mood.

## Running it

**From the flasher:** choose *The sound of the room*, *Flash*. No WiFi setup — it makes its own
network.

**From the Arduino IDE:** open `src/air/air.ino`, choose **ESP32 Dev Module**, default
partition; `airstats.h`, `air_page.h` and `ui.h` sit beside the `.ino`.

Then join the WiFi **devkit-air**, open `http://192.168.4.1`, turn the volume up a little, and
press **play the air**. The serial line prints the network count on each scan.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module with no warnings from the sketch:
**947,728 bytes — 72% of 1.31 MB**, globals at **15% of RAM**. A probe first confirmed the
unusual part — promiscuous sniffing, a SoftAP and the web server all running together — before
any of the rest was written.

**The frame tally**, on a laptop: a mix of data, management and control frames across several
channels, summed over a one-second and a half-second window, checked for the right rates,
per-channel averages, byte totals, busiest channel, an out-of-range channel folding into a
spare bucket, and an empty tally that must not divide by zero.

**The whole sonification**, on a laptop: the page's own audio code was run against a recording
stand-in for Web Audio, fed synthetic air, and checked — every pitch lands in the audible range
and rises with channel; every voice's loudness stays within 0–1 and a stronger signal is louder;
the shimmer stays within its bounds; a network that leaves fades out and is freed after its
release; one that returns is kept alive; a traffic burst rings a ping; muting silences it. This
is where the stuck-voice bug was caught. The canvas drawing was run the same way with no errors,
and the real page was loaded in Chrome against simulated air — it polled, drew its breathing
orbs and updated its readouts with no console errors.

**Not yet confirmed on hardware:** all of it on a real board — how a real room actually *sounds*,
how busy the air really is where you are, whether the chord is as pleasant as it is on paper,
and the periodic scan's ~1.5 s gap in the shimmer.

## Sources

- ESP-IDF Wi-Fi: `esp_wifi_set_promiscuous()`, `esp_wifi_set_promiscuous_rx_cb()`, and the
  `wifi_pkt_rx_ctrl_t` fields (`channel`, `rssi`, `sig_len`), read from the installed core.
- The Web Audio API — oscillators, gains, a compressor and a convolution reverb — in the
  browser.
- Chapters 1 and 4, for the SoftAP and the per-network signal that this chapter turns into
  sound.
