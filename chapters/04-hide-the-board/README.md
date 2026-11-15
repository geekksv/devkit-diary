# 4 · Hide the board

> **Needs:** one ESP32 DevKit V1 and a **power bank** to run it from. Two to eight phones.
> **Status:** compiles against the core, and the game logic is tested on a laptop against a
> simulated radio (figures below). Not yet flashed — every number here about how a real house
> bends the signal is still an expectation, and the chapter says which.

Everyone joins the board's WiFi. One player hides the board somewhere in the house — behind the
books, in a coat pocket, under a cushion — and everyone else hunts for it with their phone.
Each phone shows a meter that warms up as it gets closer: freezing, cold, warm, hot, burning.

Whoever finds it presses the **BOOT** button on the board. The board does not ask who pressed
it. It credits the hunter whose phone it hears loudest at that moment — so the only way to
win is to be standing there holding it.

---

## What makes it different from the other chapters

In every chapter so far the board was a server, and where it sat did not matter. Here **where
it is** is the game. The board is not hosting the treasure hunt; it is the treasure, and the
only clue is its own radio.

It is also the first chapter where the board's two built-in parts are used: the **BOOT button**
becomes the "I found it" key, and the **blue LED** says what the board is doing. Still nothing
attached.

## How it works

The same captive-portal trio as before — `WIFI_AP`, `WebServer`, `DNSServer` — with chapter
3's redirect, so the page always lives at `192.168.4.1`.

**The measurement is free.** An access point already knows how strongly it hears every phone
connected to it: `esp_wifi_ap_get_sta_list()` returns each one's MAC address and signal
strength in dBm. Chapter 1 drew these as bars, and had to admit it could not say *whose* bar
was whose.

**The hard part is the label.** A web request carries an IP address, not a MAC. So the board
needs a bridge from "the phone that just asked" to "the radio it is talking through", and that
bridge is the DHCP server's lease table: `esp_wifi_ap_get_sta_list_with_ip()` pairs every
connected MAC with the IP address the board handed it. On every request:

1. take the caller's IP from the connection (`web.client().remoteIP()`),
2. find that IP in the lease table to get the MAC,
3. find that MAC in the station list to get the signal.

Every page poll is therefore also a measurement. The page polls twice a second, so that is how
often each meter updates — and the phone being measured is exactly the one that asked.

## The thing worth understanding

**Signal strength is a terrible ruler.**

In free space, signal falls off smoothly with distance, and you could turn dBm into metres. A
house is not free space. Between the board and a phone are walls, furniture, water — which
includes the person holding the phone — and reflections off every flat surface. In practice:

- **Turning around** puts your body between phone and board and can cost 5–10 dB, the same as
  walking several metres away.
- **Moving your hand** changes the reading from one second to the next with nobody going
  anywhere.
- **Through a wall** can read colder than further away down an open corridor.

So an honest meter has to give up on precision and settle for **direction**. The game does
not show distance; it shows *warmer* and *colder*, and the design is about making those two
words trustworthy despite the noise.

> **What you learn:** how to get a usable signal out of a noisy one — and why "how far away is
> the WiFi" is a much harder question than it sounds.

## Taming the noise

Three layers, each answering a different kind of noise:

**1 · Smoothing the reading.** Every new reading moves the meter only 30% of the way towards it
— an exponential moving average, in `hear()`. A single wild reading moves the meter a little;
a real change keeps pulling it until it arrives. The cost is lag: after walking away, the
meter takes a second or two to cool. (The laptop tests found this mattering: a player who had
just stepped away from the board was still, for a moment, loud enough to claim it.)

**2 · Comparing against the past, not the last reading.** *Warmer* means the smoothed meter is
more than **2 dB** higher than it was **three seconds ago** (`remember()`, `trend()`).
Comparing two consecutive readings would say *warmer, colder, warmer* while you stand still.
Three seconds is long enough for a real step to show and short enough to still feel live.

**3 · Bands, not numbers.** The meter maps the signal onto 0–100° between −90 dBm (the edge of
range) and −35 dBm (a phone against the board), then into five words. A word only changes when
the meter crosses a band edge, and the phone buzzes once when you cross into a hotter band — so
eyes can stay on the room.

The page also says it plainly: *walk slowly and watch the arrow, not the number.*

## Who found it

Pressing BOOT during a hunt is a claim, and the board decides whose:

- the **loudest hunter**, by smoothed signal,
- who is **at least −55 dBm** — roughly, holding the board or standing over it,
- whose phone has **polled in the last three seconds**, so a phone left lying near the board
  cannot win for someone in the next room,
- and **never the hider**, who knows where it is and may well be standing next to it.

If nobody qualifies, the LED blinks twice — *not you* — and the hunt goes on. Two people
reaching the board together is settled by whoever is holding it, since holding it is what
makes your phone loudest.

## The flow of a round

| Phase | Hunters see | Hider sees | The LED |
|---|---|---|---|
| **lobby** | players and scores, and how loud the board hears you | the same | off |
| **hiding** | *close your eyes* and a 60 s countdown | *hide it now*, and a start button | blinking |
| **hunt** | their own meter, the arrow, their rank, the clock | everyone's meter, live | **off** — it would give it away |
| **found** | who found it, how long it took, the scores | the same | a burst of fast blinks |

The hunt starts when the hider presses **BOOT** on the board (the natural thing to do as you
walk away from the hiding place), taps *start the hunt*, or the 60 s run out. The hider can end
a round nobody is going to win.

**Rank is shown, other hunters' meters are not.** Knowing you are *closest of 3* is part of the
fun; seeing someone else's meter at *burning* would just tell you to follow them.

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the page; anything else redirects here |
| `/join` | POST | `n`, `t` | join with a name; with a known token, rejoin |
| `/me` | GET | `t` | the game as this player sees it, and a fresh reading of their signal |
| `/hide` | POST | `t` | *I'll hide it* — needs at least one other player |
| `/go` | POST | `t` | the hider starts the hunt from the phone |
| `/end` | POST | `t` | the hider ends a hunt with no winner |
| `/lobby` | POST | `t` | back to the lobby after a round |
| `/leave` | POST | `t` | leave the game |

## Limits

- **Run it from a power bank from the start.** Moving the board from a laptop's USB to a
  battery is a power cut: it restarts, and every player is dropped.
- **Eight phones** — the DHCP ceiling again, and that includes the hider.
- **Range is the WiFi's.** Hide it so far away that a phone drops off the network and that
  phone's meter goes blank — *listening…* — until it comes back.
- **The thresholds are guesses.** −90, −35 and −55 dBm are where a typical phone and a typical
  house should land; they are constants at the top of the sketch and the first thing to tune.
- **Phones differ.** Two phones in the same spot can read several dB apart, because their
  antennas differ. Fine for *warmer*, unfair for *who is closer* between two different phones.
- **Bodies count.** A hunter standing between another hunter and the board makes that other
  phone read colder. Nothing can be done about it, and in practice it adds to the game.

## Where it could go next

1. **Calibrate per phone.** At the start of each round, every hunter holds their phone against
   the board for three seconds; the meter's −35 becomes *that phone's* reading. Removes most of
   the phone-to-phone unfairness.
2. **A heat trail for the hider.** Keep each hunter's meter over time and draw it after the
   round — who went straight there, who circled the kitchen three times.
3. **Team play.** Two teams, two boards, each hiding theirs from the other.
4. **A lighthouse for a stalled hunt.** After two minutes with nobody warmer than *cold*, the
   LED blinks once every ten seconds — a hint that costs the hider nothing to give.

Bluetooth looks like a better fit — it is what item trackers use — but a web page cannot scan
for Bluetooth devices, so the phones could not hear the board that way. That dead end is why
this chapter measures the WiFi the phones are already on.

## Running it

Open `src/hide/hide.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash. The default
partition is fine. `ui.h` must sit beside the `.ino`. Then **move the board to a power bank**.

1. Everyone joins the WiFi **devkit-hide** and opens `http://192.168.4.1`, then picks a name.
2. One player taps **I'll hide it**. Everyone else looks away. The blue light blinks.
3. The hider hides the board and presses **BOOT** on it (or taps *start the hunt*).
4. Hunters follow their meters. Whoever finds it holds their phone next to it and presses
   **BOOT**.

Serial at 115200 logs phones joining and leaving, every phase change, every BOOT press, and who
it was credited to at what signal — the place to look when a claim is refused.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module on the default partition, with
no warnings from the sketch: **957,052 bytes — 73% of 1.31 MB**, and global variables at
**14% of RAM**.

`esp_wifi_ap_get_sta_list_with_ip()` and its `wifi_sta_mac_ip_list_t` were read out of the
installed core's headers rather than taken from a tutorial. Building this also turned up a small
one: the core already defines `BOOT_PIN`, so the sketch's own name for the button clashed and
had to be renamed.

The game logic was driven on a laptop against a simulated radio: joining and names, hiding and
the 60 s timer, BOOT starting the hunt, hunters not seeing each other's meters while the hider
sees all of them, the smoothed reading settling, *warmer* appearing as a hunter walks in, a
claim refused when nobody is close, the hider never credited even when loudest, a phone that
stopped polling unable to claim, the hider leaving or giving up, and a ninth player turned away.

**Not yet confirmed on hardware:** all of it on a real board — and in particular whether the
station list's signal updates fast enough to follow someone walking, how far −90 dBm reaches
in a real house, and whether −55 dBm is the right line for *holding it*.

## Sources

- The ESP32 Arduino core's bundled `WebServer`, `DNSServer`, and ESP-IDF's
  `esp_wifi_ap_get_sta_list()` and `esp_wifi_ap_get_sta_list_with_ip()`.
- Chapter 1, for the signal bars this chapter finally puts names to, and chapter 3, for the
  redirect.
