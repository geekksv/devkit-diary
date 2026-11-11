# 1 · A chat room that needs nothing

> **Needs:** one ESP32 DevKit V1. Nothing else — not even a router.
> **Status:** sketch complete, logic tested off-hardware, first flash pending.

---

## What this does

The board makes its own WiFi network. Phones join it and type to each other.

There is no router, no internet, no account, no company in the middle. Nothing said in the room
leaves the room, because there is nowhere for it to go. Pull the power and the conversation is
gone — not deleted, just never written anywhere.

Join the network and the chat opens by itself, without anyone being told an address.

## Why that works

Three things the chip already does, used together.

**It is an access point, not just a client.** The radio has two modes and most projects only
ever use one. `WIFI_AP` makes the board the network. Phones see `devkit-chat` in their WiFi
list and join it like any other.

**It is a web server.** The "app" is a web page the board serves — no install, no store, works
on anything with a browser, which is every phone in the room.

**It lies about DNS, on purpose.** This is the part that makes it feel finished rather than
technical. Every phone, on joining an unfamiliar network, quietly requests a known URL to find
out whether it is behind a hotel login page. Android asks for one address, iOS another, Windows
a third. The board runs a DNS server that answers *every* lookup with its own address, so
whichever probe the phone sends arrives at the board — and the board answers it with the chat.

The phone concludes it is behind a captive portal and pops the page up without being asked.
The same mechanism that produces airport WiFi login screens, pointed at something worth having.

---

## How many people can actually join

Eight. Not because of the chat, but because of the DHCP server — and the stock default is
worse than that.

| Limit | Value | Set by |
|---|---|---|
| Phones associated at once | **8** | `MAX_STATIONS`, passed to `softAP()` |
| **Arduino core's default if you don't pass it** | **4** | 5th positional arg of `WiFi.softAP()` |
| DHCP leases available | **8** | `CONFIG_LWIP_DHCPS_MAX_STATION_NUM` in the core's `sdkconfig` |
| Roster slots (names) | 24 | `MAX_USERS` |
| Messages retained | 40 | `MAX_MSGS`, ring buffer |
| Message length | 140 chars | `MAX_LEN` |
| Name length | 15 chars | `NAME_LEN` |
| Name released after | 120 s idle | `IDLE_MS` |

Two of those rows are the whole story:

**`WiFi.softAP(ssid, pass)` caps the room at four phones.** `max_connection` is the fifth
positional parameter and it defaults to `4`. Nothing warns you; the fifth person simply cannot
join. This sketch passes 8 explicitly:

```cpp
WiFi.softAP(AP_NAME, AP_PASS, 1, 0, MAX_STATIONS);
//                            ^  ^  ^
//                      channel  |  max_connection — the one that matters
//                               ssid_hidden
```

**Eight is the ceiling worth asking for, not the radio's limit.** The MAC layer will associate
more, but the core ships with a DHCP pool of 8 (`CONFIG_LWIP_DHCPS_MAX_STATION_NUM=8`). Ask for
10 and the ninth phone associates, waits for an address that never arrives, and shows the user a
WiFi network that is connected and broken — which is a worse failure than being told the room is
full. Going beyond 8 means rebuilding the core with a larger pool, which is outside this book's
one-board rule in spirit if not in hardware.

**The roster is deliberately bigger than the radio limit.** 24 name slots for 8 phones is not
a mistake: a name is held for `IDLE_MS` after its phone stops polling, so that closing your
browser for thirty seconds does not hand your name to someone else. Names outlive radios on
purpose, so the two numbers measure different things.

> For a workshop of sixty, this is the wrong chapter — that is a separate idea in
> [PROJECTS.md](../../PROJECTS.md), and it needs the board to serve a page and then get out of
> the way rather than relay every message.

---

## Range

The module is an ESP32-WROOM-32 with a PCB trace antenna and a maximum transmit power of
**+20 dBm**, settable in half-decibel steps down to +2 dBm via `WiFi.setTxPower()`. That is
ordinary consumer WiFi power — the board is not weak, it is just small.

What you actually get depends on the room, so the page measures it instead of guessing. The AP
knows the signal strength of every associated phone for free, and the sketch reads it:

```cpp
wifi_sta_list_t list;
esp_wifi_ap_get_sta_list(&list);
// list.sta[i].rssi  — "current average rssi of sta connected"
```

The chat shows one bar per connected radio, strongest first, with the weakest called out. Walk
away from the board until a bar turns red and you have measured the range in your own room,
which is the only number that matters.

How to read it:

| RSSI | Means |
|---|---|
| better than −55 dBm | strong — same room, nothing in the way |
| −55 to −67 dBm | good — usable through a wall or across a flat |
| −67 to −78 dBm | weak — messages arrive, slowly |
| below −78 dBm | about to drop; the phone may stay associated and stop working |

That last row is the behaviour worth knowing: a phone does not cleanly disconnect at the edge
of range. It stays joined, the polling times out, and the page says *lost the board* while the
WiFi icon still shows connected. The signal card is there so you can tell that apart from a
crash.

⚠️ **The bars are radios, not people.** They are not labelled with names, and that is
deliberate rather than unfinished: identity here is a token, and an HTTP request carries no MAC
address, so there is no honest way to put a name on a bar. Mapping them would mean guessing.

---

## Orchestration

The board is the only authority. Phones hold no state that matters except a token.

```
┌──────────────┐                                 ┌────────────────────────────┐
│    phone     │                                 │        ESP32 (AP)          │
│              │  1. join devkit-chat            │                            │
│              │ ───────────────────────────────▶│  DHCP hands out a lease    │
│              │                                 │  (pool of 8)               │
│              │  2. OS captive-portal probe     │                            │
│              │ ───────────────────────────────▶│  DNSServer: every name     │
│              │ ◀───────────────────────────────│  resolves to 192.168.4.1   │
│              │                                 │                            │
│  join screen │  3. POST /join  n=<name>        │  roster: name unique among │
│              │ ───────────────────────────────▶│  the present? allocate a   │
│              │ ◀───────────────────────────────│  slot, mint a token        │
│              │     { ok, token, name }         │                            │
│   chat view  │                                 │  users[] : token→name,seen │
│              │  4. GET /msgs?since=N&t=<tok>   │                            │
│              │ ───────────────────────────────▶│  refresh seen; return only │
│              │ ◀───────────────────────────────│  messages after N          │
│              │     every 900 ms                │                            │
│              │                                 │                            │
│              │  5. POST /say   t=<tok>&x=<txt> │  look up name FROM TOKEN,  │
│              │ ───────────────────────────────▶│  stamp it, append to ring  │
└──────────────┘                                 └────────────────────────────┘
```

### The endpoints

| | Method | Takes | Returns | Notes |
|---|---|---|---|---|
| `/` | GET | — | the page | also served for *every* unknown path, so any portal probe works |
| `/join` | POST | `n`, optional `t` | `{ok, token, name}` or `{ok:false, err}` | `err` ∈ `taken` · `empty` · `full` |
| `/msgs` | GET | `since`, `t` | messages after `since`, roster, signals | also the liveness ping |
| `/say` | POST | `t`, `x` | `{ok}` | **no name parameter exists** |
| `/leave` | POST | `t` | `{ok}` | frees the name immediately |

### Identity, and the design that had to be thrown away

The first version put a name field next to the message box and sent both with every message. It
worked, and it was wrong in a way that only appears with more than two people in the room:

- the name could change between messages, so a conversation had no stable speakers
- you could type **someone else's name** and talk as them

Neither is a coding mistake. The code did what it was told. The design trusted the sender to
declare who it was, and a sender will say anything. Hiding the field after the first message
would have hidden the problem, not fixed it — any client can post by hand and the board would
believe it.

So identity moved to the board. You pick a name once; the board checks it is unused among the
people present, stores it, and returns a random 32-bit token from `esp_random()`. Every message
carries the **token** and never a name, and the board stamps the name from its own table.

There is now no request a client *can* send that posts under another name, because the protocol
has no field for one. Three consequences fall out of that:

- **A valid token short-circuits `/join`** and returns the name you already have, ignoring
  whatever was asked for — so a hand-made rename request bounces, not just the UI.
- **A reload keeps you in.** The token lives in `localStorage` and is re-validated on load.
- **A reboot is visible.** All tokens die with the board, so a rejected token bounces the page
  back to the join screen saying *names were released* rather than silently failing to send.

### Why polling, and what it costs

Each phone issues `GET /msgs?since=N` every 900 ms. `since` is a monotonic message counter, so
the response contains only what that phone has not seen — usually an empty array. Re-sending the
whole log every second is what turns eight phones into a board that stops answering.

`WebServer` holds one `_currentClient` and services one request at a time inside `loop()`. With
eight phones that is roughly nine requests a second to satisfy, interleaved with
`dns.processNextRequest()`. Comfortable; it is also the thing that will give out first if you
raise the station count.

### Memory

Fixed allocations, no heap growth during a conversation:

```
msgs[40]   × (16 + 141 + 4)   ≈ 6.4 KB
users[24]  × (4 + 16 + 4)     ≈ 0.6 KB
page (PROGMEM, streamed)       ≈ 11 KB flash, not RAM
```

The page is streamed by `uiSend()` rather than assembled into a `String`, because a 6 KB page
built in RAM on a board running two WiFi stacks is the kind of fragmentation that shows up as a
reboot an hour later. The JSON responses are still `String`-built, which is the one remaining
allocation worth watching: a phone that has just joined pulls up to 40 messages at once.

---

## What it will not do

**It is not private between people in the room.** Everyone on the network can read everything;
that is what a room is. It is private from *outside* the room, which is a different claim.

**There is no authentication.** Nothing stops someone joining and picking an unused name. The
token prevents impersonation of a *present* name, not strangers arriving. On an open network,
anyone in radio range is in the room — set `AP_PASS` to eight characters or more if that
matters.

**It forgets everything on reboot.** 40 messages in RAM, no flash, no log, no history.

**Nine phones is a worse experience than eight.** See the DHCP note above.

## Not yet verified

Honest about which of this is read and which is run:

- **Tested:** the page's logic against a stub board and a stub DOM — join, name lock, duplicate
  refusal, reload, rename attempt, reboot, leave. Every element id resolves and every endpoint
  the script calls is one the firmware serves.
- **Verified by reading the installed core (3.3.11):** the station and DHCP limits above, the
  `wifi_sta_info_t.rssi` field, and that `esp_random()` needs `<esp_random.h>` explicitly — it
  is *not* reachable through `Arduino.h` on 3.x, and the sketch would not have compiled without
  it.
- **Not tested:** it has not been flashed. Open questions: whether the portal actually pops on
  current Android and iOS; how many phones before 900 ms polling stutters; and whether an idle
  phone really releases its name or keeps polling often enough to hold it.

---

## Running it

Open `src/local-chat/local-chat.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash.
`ui.h` must sit beside the `.ino` — the IDE only compiles what is in the sketch folder.

Serial at 115200 prints one line: the network name and the address. Everything after that
happens on the phones. Join **`devkit-chat`** and the page should open by itself; if it does
not, open `http://192.168.4.1`.

## Sources

- [ESP32 Wi-Fi API guide — SoftAP mode and `max_connection`](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html)
- [`esp_wifi_ap_get_sta_list()` and `wifi_sta_info_t`](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html)
- [ESP32-WROOM-32 datasheet — antenna and +20 dBm transmit power](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf)
- [Captive portal detection, and the URLs each OS probes](https://en.wikipedia.org/wiki/Captive_portal)
- [`esp_random()` — the hardware RNG behind the tokens](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/random.html)
