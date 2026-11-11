# 1 · A chat room that needs nothing

> **Needs:** one ESP32 DevKit V1. Nothing else — not even a router.
> **Status:** written · sketch complete, first flash pending (see [notes.md](notes.md))

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

**It is a web server.** Fifteen or so phones can hold a TCP connection to it at once. The
"app" is a web page the board serves — no install, no store, works on anything with a browser,
which is every phone in the room.

**It lies about DNS, on purpose.** This is the part that makes it feel finished rather than
technical. Every phone, on joining an unfamiliar network, quietly requests a known URL to find
out whether it is behind a hotel login page. Android asks for one address, iOS another, Windows
a third. The board runs a DNS server that answers *every* lookup with its own address, so
whichever probe the phone sends arrives at the board — and the board answers it with the chat.

The phone concludes it is behind a captive portal and pops the page up without being asked.
The same mechanism that produces airport WiFi login screens, pointed at something worth having.

## The problem that turned out to be the interesting one

The first version had a name field sitting next to the message box. You typed your name, you
typed a message, you sent both.

It worked, and it was wrong, in a way that only shows up with more than two people in the room:

- You could change your name between messages, so a conversation had no stable speakers.
- You could type **someone else's** name and speak as them. Not a bug in the code — the code
  did exactly what it was told. The design simply trusted the sender to say who they were.

Both come from the same mistake: the name travelled with every message. Fixing the page so the
name field disappears after the first message would hide it, not fix it. Anyone can send an
HTTP request by hand, and the board would believe it.

So identity moved to the board:

1. You arrive at a **join screen** and pick a name once.
2. The board checks nobody present is already using it, stores it, and hands back a random
   32-bit **token**.
3. Every message you post carries the token. **Never the name.** The board looks up who that
   token belongs to and stamps the message itself.

There is now no request a client *can* send that posts under another name, because the client
has no way to state a name at all. The page hiding the field is a nicety; the token is the
reason it holds.

A name is yours while you are present, and released a couple of minutes after your phone goes
quiet — or straight away if you press *leave*. Rejoining with a token you already hold returns
the name you already have and ignores whatever was asked for, so even a hand-made rename
request bounces. A reload keeps you in: the token sits in `localStorage` and is re-checked on
load, so refreshing does not cost you your name.

> **The general version of this, which is worth more than the chapter:** if a client tells you
> who it is, that is a claim, not a fact. Give it a handle and keep the identity on your side
> of the wire. The same reasoning is why a web application stores a session rather than
> trusting a username in the request body.

## What it will not do

**It is not private between people in the room.** Everyone on the network can read everything;
that is what a room is. It is private from *outside* the room, which is a different claim.

**There is no authentication.** Nothing stops someone joining and picking a name nobody has
taken. The token stops impersonation of a *present* name, not strangers arriving. On an open
network, anyone in radio range is in the room.

**It forgets everything on reboot.** Messages live in a 40-entry ring buffer in RAM. There is
no flash, no log, no history. For this chapter that is a feature.

**It will not scale past a room.** The AP accepts a limited number of stations, and a browser
polling twice a second from twenty phones is already most of what a single core wants to do
while also serving pages.

## The idea being tested

> A working group chat — stable identities, no impersonation, no setup for the people using it
> — fits on one chip with nothing attached, and the people in the room need to be told nothing
> except which network to join.

The second half is the test. Handing someone an IP address to type is a demo. Having the page
appear on its own is the difference between a project and something you would actually use at a
workshop where the venue WiFi cannot be trusted.

---

## Running it

Open `src/local-chat/local-chat.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash.
Serial at 115200 prints one line: the network name and the address. Everything after that
happens on the phones.

Join **`devkit-chat`** and the page should open by itself. If it does not, open
`http://192.168.4.1`.

The network is open by default. Set `AP_PASS` to eight characters or more to lock it.

## Build log

See **[notes.md](notes.md)** — the real log, including the design that had to be thrown away
and the bug that would have stopped it compiling.

## Sources

- [ESP32 SoftAP — Espressif's own documentation for the AP mode](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html)
- [Captive portal detection, and the URLs each OS probes](https://en.wikipedia.org/wiki/Captive_portal)
- [`esp_random()` — the hardware RNG used for the tokens](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/random.html)
