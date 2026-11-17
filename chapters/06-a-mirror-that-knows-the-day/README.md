# 6 · A mirror that knows the day

> **Needs:** one ESP32 DevKit V1, your home WiFi, and any screen with a browser to be the
> mirror — an old tablet or phone behind two-way glass, or a laptop.
> **Status:** flashed from the flasher and running: WiFi handed over through the flasher's card,
> the mirror on a real screen. The readers, the WiFi hand-over and the page were also tested on a
> laptop (figures below).

The board joins your home WiFi and becomes the brain of a smart mirror. Every few minutes it
fetches the weather, the air quality, a handful of headlines, something that happened on this
day, and the next public holiday — all from free services that need no account and no key —
and serves a mirror page to any screen on the same network.

Open `http://mirror.local` on an old tablet, put it behind a sheet of two-way mirror acrylic,
and the text floats in the glass: the time, the weather now and for five days, sunrise and
sunset, a greeting, a headline that changes every fifteen seconds.

**By day it is white text on black; at night, black text on white** — switched at your own
city's sunrise and sunset, which the weather brings with it.

---

## What makes it different from the other chapters

Every chapter so far made its own network and stayed inside it. Nothing they did touched the
internet, and nothing could: `devkit-chat`, `devkit-xo`, `devkit-boxes` are islands.

This is the first chapter that **reaches out**. The board joins a real network and talks to
servers on the other side of the world, over HTTPS — which brings three things the islands
never needed: getting the board onto someone's WiFi without their password in the code,
proving to the board that the server is who it claims, and reading answers far bigger than the
board's memory.

## What is on the mirror, and where it comes from

| On the mirror | From | Key? | How often |
|---|---|---|---|
| The time | NTP (`pool.ntp.org`) | none | kept by the board |
| Weather now, 5-day forecast, sunrise, sunset | [Open-Meteo](https://open-meteo.com) forecast | none | every 15 min |
| Air quality (US AQI) | Open-Meteo air quality | none | hourly |
| Your city, found by name | Open-Meteo geocoding | none | when you search |
| Headlines | any RSS feed — eight to pick from, or your own | none | every 15 min |
| On this day | [Wikipedia's feed](https://api.wikimedia.org/wiki/Feed_API) | none | at local midnight |
| Next public holiday | [Nager.Date](https://date.nager.at) | none | twice a day |
| Greeting, ticking clock, day or night | worked out on the page | — | every second |
| Your message | the settings page | — | when you change it |

Settings live on the board and are changed from a phone at `http://mirror.local/settings`: your
name, **a message of your own** — up to 120 characters, shown under the greeting — your city, °C or °F, 24- or 12-hour clock, the headline feed, and the screen — *by the
sun*, *always black* or *always white*.

## Getting onto the WiFi — without the password in the code

The firmware for this chapter is public: anyone can flash it from the
[flasher](https://geekksv.github.io/devkit-diary/). So it cannot contain anyone's WiFi, and it
does not. The name and password are handed to the board after it is flashed and kept in its
flash (NVS), which survives restarts and re-flashing. Two ways in, both in `wifi_setup.h`:

**Over the USB cable, from the flasher.** Straight after flashing, the flasher shows a *Connect
the board to your WiFi* card. *Scan* asks the board which networks it can see; pick yours, type
the password, *Connect*. The password goes from the browser tab, down the cable, to the board —
never over the internet, never to GitHub. The board joins, answers with its address, and
restarts into the mirror.

The conversation on the cable is **Improv Serial**, an open standard from the ESPHome project
for exactly this — rather than a protocol invented here. Its packets share the serial line with
the board's ordinary log: each starts with the bytes `IMPROV`, carries a type, a length and a
checksum, and anything that is not a well-formed packet is just log. `improv.h` is the board's
side; the flasher's side is in `workbench/app.js`.

**From a phone, with no computer.** A board with no saved network — or one it cannot reach —
opens its own network, `devkit-mirror-setup`, with a page that lists the networks it can see.
Join it from a phone (the page opens by itself, as every chapter's does), pick your WiFi, type
the password. The board saves it and restarts; if it cannot join, the setup network comes back
and says why — *the network was not found*, or *it did not accept the password*.

The phone page saves first and tests after the restart, on purpose. Testing the password on the
spot would move the board's radio to the home network's channel, and the phone showing the
page — joined to the board's own network on another channel — would drop off mid-answer.

**Holding BOOT for five seconds** forgets the network, for a new router or a move.

## The thing worth understanding

**The board can always call out; nobody can call in.**

Your router lets the board open a connection to `api.open-meteo.com` and get an answer back,
because the board started it. Nothing outside your home can open a connection *to* the board —
the router has no idea which device on the inside an unasked-for knock is meant for, and drops
it. That one rule shapes everything here: the board *fetches* on a schedule rather than being
*sent* the weather, and the mirror screen *asks* the board every five seconds rather than being
told. It is also why the mirror only works on your own WiFi, and why it is safe there.

> **What you learn:** why every smart device in a house polls, and why "reach it from
> anywhere" always needs a server in the middle.

## HTTPS on a chip with 320 KB of RAM

Every service here is HTTPS-only, so the board does TLS: it checks the server's certificate
against a list of root authorities it trusts, then encrypts the conversation.

- **The list is already on the board.** The ESP32 core is built with
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL` — Mozilla's full set of root certificates,
  compressed into the firmware. The sketch hands it to each connection
  (`setCACertBundle`); no certificate is pasted into the code, so none goes stale when a site
  renews its own.
- **One connection at a time.** A TLS session costs roughly 40 KB of RAM while it is open.
  The fetcher and the settings page's city search share one lock, so two never overlap.
- **It costs flash.** TLS, the HTTP client and `mirror.local` naming together take the sketch
  to **93% of the default partition** — the fullest in the book after chapter 2. It fits; it
  does not leave much room. If it ever stops fitting, **Huge APP** (3 MB for the program,
  no over-the-air slot) is the same trade chapter 2 makes.

## Reading answers bigger than the board

Some answers are small: the weather is about **1 KB** and is read whole. Others are not —
Wikipedia's "on this day" for one date is **130 KB**, and a news feed can be **150 KB**. The
board never holds those. `parse.h` reads them **as they arrive**, byte by byte, keeps only what
the mirror shows, and **hangs up** when it has it:

| Answer | Size | Read before hanging up |
|---|---|---|
| Wikipedia, on this day (17 Nov) | 132,733 bytes | **49,203** — five events |
| The Guardian, world | 151,917 bytes | **24,472** — eight headlines |
| BBC World | 16,628 bytes | **6,455** |
| The Hindu | 50,114 bytes | **6,468** |

Two details that only showed up against the real responses, and are now in the readers:

- **Wikipedia's events contain a decoy.** Each event is `{"text":"…","pages":[…],"year":…}` —
  but every page inside it also has a `"text"` field, the empty name of its namespace. A reader
  that looked for the next `"text":"` would print nothing. An event's own text always follows
  an opening brace, so the reader looks for `{"text":"`.
- **Not every holiday is everyone's.** Nager.Date's next holiday in the UK is St Andrew's Day —
  a holiday in Scotland only. The reader skips entries marked `"global":false` and shows the
  next one for the whole country.

Feeds are requested over **HTTP/1.0** on purpose: it makes servers send the body plain, rather
than in chunks with size markers that would otherwise land in the middle of a headline.

## Two cores, finally used

A fetch over HTTPS can take a second or two. Done in `loop()`, the mirror page would freeze
for that long every time. So the fetcher runs as its own task on **core 0**, beside the WiFi
stack, and the web server stays on **core 1**. They share the fetched data through a mutex,
so the page never reads a half-written forecast.

## Day and night

The page decides the theme itself, every second, from the city's sunrise and sunset that came
with the weather: **between sunrise and sunset, white on black; otherwise black on white.**
The change fades over two seconds. Before the first weather arrives, 07:00–19:00 local time
stands in for the sun. *Always black* and *always white* pin it.

The time on the mirror is the board's (synced over NTP), shown in the city's own time zone —
the weather brings the offset, so daylight saving changes arrive with the next forecast.

## Any screen

The mirror is one page drawn for every shape: everything is sized from the screen's smaller
side, so it scales from a phone to a monitor without a breakpoint per device, and nothing ever
scrolls.

- **Landscape** (a monitor, a tablet on its side): three columns — clock, *on this day*,
  weather and forecast — with the greeting and a headline below.
- **Portrait** (a phone, a tablet standing up): the clock and the weather now share the top
  row, the forecast gets a full-width row of its own, and *on this day* moves under the
  greeting.

The colon in the clock blinks with the seconds. It is set from the page's own one-second tick
rather than a CSS animation, so it still blinks on a screen with *reduce motion* switched on —
where every animation is turned off, as chapter 5's last-move mark found out.

Tap the screen once for full screen; the cursor hides itself when the mouse is still. The
**BOOT button** cycles four views on every screen at once: *everything*, *weather* (a large
forecast), *headlines* (all eight as a list), and *clock* (just the time, large — a night
light).

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the mirror page |
| `/settings` | GET | — | the settings page |
| `/api/data` | GET | — | everything the mirror shows, ~3 KB — polled every 5 s |
| `/api/settings` | GET / POST | name, msg, units, clock24, theme, feed, custom, place | read or save settings |
| `/api/places` | GET | `q` | up to five cities matching a name |
| `/api/forget-wifi` | POST | — | forget the network and restart into setup |

In setup mode, `/` is the WiFi page, with `/setup/scan` and `/setup/save` behind it.

## Limits

- **The screen has to stay awake.** A web page can only keep a screen on through the Wake
  Lock API, which browsers allow on `https` pages only; the board serves plain `http` on your
  network. Set the tablet's own *screen timeout* to *never*.
- **`mirror.local` is not universal.** Laptops and iPhones resolve it; some Android phones do
  not. The address it also works on is on the serial line, in the flasher after setup, and on
  the settings page.
- **The font comes from Google Fonts**, loaded by the screen. Without it the mirror falls back
  to a narrow system font and still works.
- **Free services ask for restraint.** The schedule above keeps the board to a few dozen
  requests an hour. Open-Meteo is free for non-commercial use.
- **US AQI** is the air-quality scale shown everywhere, whatever the country's own index is.
- **One mirror per network** answers to `mirror.local`; a second board would need its own
  name (`HOST` at the top of the sketch).

## Where it could go next

1. **Calendar.** A public or secret iCal link — Google Calendar gives one — streamed the same
   way as the feeds, for the next three events.
2. **A compliment, sometimes.** The classic smart-mirror touch: a line by time of day, from a
   small list on the board.
3. **Two pages on a timer.** Weather for ten seconds, news for ten — for a screen too small
   for everything at once.
4. **Brightness by the sun.** The page could dim the text an hour after sunset, not just swap
   colours.

## Running it

**From the flasher** (no Arduino IDE): open the [flasher](https://geekksv.github.io/devkit-diary/)
in desktop Chrome or Edge, choose *A mirror that knows the day*, *Flash*, then use the *Connect
the board to your WiFi* card that appears.

**From the Arduino IDE:** open `src/mirror/mirror.ino`, choose **ESP32 Dev Module**, flash. The
default partition is fine. Every `.h` file must sit beside the `.ino`. Then join
`devkit-mirror-setup` from a phone to give it your WiFi.

Then, on any screen on the same WiFi, open `http://mirror.local` (or the address on the serial
line), and on a phone open `http://mirror.local/settings` to choose your city.

Serial at 115200 logs the WiFi steps, every fetch and what it brought, the free heap, and each
BOOT press.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module with no warnings from the sketch:
**1,229,000 bytes — 93% of the default 1.31 MB**, globals at **17% of RAM**. The
certificate bundle the sketch hands to each connection was checked to be built into the core
(`_binary_x509_crt_bundle_start`, in `libmbedtls`).

**The readers**, on a laptop, against real responses saved from each service: the weather
(temperature, code, humidity, wind, offset, six days of highs, lows, sunrises and sunsets), air
quality, a holiday (skipping a regional one), five cities from one name, five events from
Wikipedia after 49 KB of 132 KB, and eight headlines from each of the eight feeds. Plus made-up
titles for the cases the feeds did not happen to contain: entities next to spaces, CDATA, a
bare `&`, a false `</t`, Atom `<entry>`, and JSON escapes including a surrogate-pair emoji. The
spaces case was a real bug — `A &amp; B` came out as `A& B` — found and fixed this way.

**The WiFi hand-over**, both ways: the board's code built a serial stream — log lines, a state
packet, device info, two scanned networks, the end of the scan, a deliberately corrupted packet
and the joined address — and the flasher's code read all six good packets, kept the log, and
treated the corrupted one as text. The flasher's code built a WiFi request for `Home WiFi ☕`
with a password containing spaces, and the board's code read both back exactly.

**The page**, rendered in Chrome with real data at 390×844, 820×1180, 1180×820 and 1440×900, in
both themes and all four views. That found a phone layout where the weather column ran off the
right edge, which is why the forecast now gets its own row in portrait.

**The first build on GitHub failed**, and no build on this laptop could have caught it. Before
compiling a `.ino`, the Arduino build writes a declaration for every function in it, using a
tool (`ctags`) that only ships for Intel machines — so on this Apple-silicon laptop that step
had been switched off, and every local build passed. GitHub's build runs it, and it mangled the
one templated function in the sketch: `'F' has not been declared`. The HTTPS helpers now live
in `net.h`, where that step never looks, which is also where they read best.

**Confirmed on hardware:** flashed from the flasher, WiFi handed over through its *connect*
card, and the mirror running on a real screen — HTTPS from the board, the fetcher on its own
core, the page drawing the data.

**Not yet confirmed on hardware:** the phone setup network (`devkit-mirror-setup`), the
five-second BOOT reset, the free heap with TLS open, and the custom message and blinking clock,
which were added after that test.

## Sources

- [Improv Wi-Fi](https://www.improv-wifi.com/) — the serial protocol for handing over WiFi.
- [Open-Meteo](https://open-meteo.com/en/docs) — forecast, air quality and geocoding APIs.
- [Wikimedia Feed API](https://api.wikimedia.org/wiki/Feed_API/Reference/On_this_day) — on this day.
- [Nager.Date](https://date.nager.at/Api) — public holidays.
- The ESP32 Arduino core's `NetworkClientSecure`, `HTTPClient`, `Preferences`, `ESPmDNS`, and
  ESP-IDF's certificate bundle.
