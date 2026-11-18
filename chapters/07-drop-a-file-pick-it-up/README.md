# 7 · Drop a file, pick it up

> **Needs:** one ESP32 DevKit V1, and two devices with a browser — a laptop and a phone.
> **Status:** flashed from the flasher and working: a file sent from a phone to a laptop
> through the board. The pipe is also tested on a laptop against real TCP connections — with a
> 20 MB file, a 4.4 GB file, and every way a transfer can fail (figures below).

Open the board's page on a laptop and on a phone. Each sees the other in a list of the devices
here. Pick a file on one, choose the other; it says *accept*, and the file goes across — with
no cable, no cloud, no account and no app.

**The board never keeps the file.** It is a pipe: it takes a few kilobytes from the sender,
hands them to the receiver, and takes the next few. So a file can be far bigger than the board's
whole 4 MB of flash — a long video goes through as happily as a photo — and nothing is left
behind afterwards. (*How* big, honestly, is [below](#how-big-a-file-really).) The only thing the board does keep is short text notes: a link, a code, a
paragraph you would otherwise email to yourself. Those clear themselves after a day.

On your home WiFi it answers at `http://drop.local`. With no WiFi set up, or none in reach, it
makes its own network, `devkit-drop`, and works there instead — so it is useful anywhere.

---

## What makes it different from the other chapters

Every earlier chapter that moved data kept it: the chat's messages, the games' boards, the
mirror's forecast. All of it fit in a few kilobytes. This chapter moves data that does **not**
fit — and the way to handle data that is bigger than you are is not to hold it at all.

It is also the first chapter that needs **two connections open at once**, which the web server
every chapter has used cannot do. So it gets a second, smaller server, written at the level of
raw sockets.

## How a file goes across

1. **Every page is a device.** Each browser makes up an id, keeps it, and guesses a name
   ("iPhone", "Mac", "Android phone" — editable). Its once-a-second poll is also a heartbeat,
   so the list of devices here is always the list of pages actually open.
2. **The sender offers.** Pick a file, pick a device, *send*: the board records an offer — the
   file's name, type and exact size, from whom, to whom. Nothing of the file has moved yet.
3. **The receiver accepts — and the accept *is* the download.** Its page shows *"Mac is sending
   you report.pdf, 4.2 MB — accept and download"*. That button is an ordinary link to the
   board, and tapping it opens the download. It has to be: browsers only let a download through
   that starts directly from a tap, and Safari is strict about it.
4. **The sender's page sees the acceptance** and starts uploading — to the same board, on the
   pipe's port.
5. **The board joins the two.** It sends the receiver the download's headers — the name, the
   type, the size, so the browser shows a normal download with a progress bar — and then moves
   the bytes across, four kilobytes at a time.

## The thing worth understanding

**A pipe needs almost no memory, because the network already knows how to wait.**

The board reads up to 4 KB from the sender's connection, writes it to the receiver's, and goes
round again. If the receiver is slow — a phone on a weak signal — the board's write waits, so it
stops reading, so the sender's data piles up in the TCP buffers between them, so the sender's
browser **slows down by itself**. Nobody wrote that rule; it is TCP's flow control, and it is
why a 320 KB chip can move a 500 MB file without ever holding more than one small buffer of it.

Any sensible server that stores what it is sent needs as much space as the biggest thing anyone
might send. A pipe needs as much space as one round of the loop.

> **What you learn:** streaming — moving data that is bigger than you are — and how TCP's flow
> control makes a slow link slow the whole chain down instead of overflowing it.

## Two servers on one board

| Port | Server | Does |
|---|---|---|
| 80 | the usual `WebServer` | the page, the device list, offers, notes, settings — one short request at a time |
| 8080 | `pipe.h`, raw sockets | `GET /get` (the receiver's download) and `POST /put` (the sender's upload), held open together |

The pipe server reads each request's first line and headers itself, leaves the body untouched
in the socket, and keeps both sockets until the last byte has crossed. Between rounds it hands
the loop back, so the page on port 80 keeps answering during a transfer — the sender's progress
comes from there.

Because the page (port 80) and the pipe (port 8080) are different *origins* to a browser, the
upload is a cross-origin request; the pipe answers the browser's preflight (`OPTIONS`) and adds
`Access-Control-Allow-Origin` to its replies. An upload with a progress bar always gets a
preflight, so this is not optional.

## When it goes wrong

Every one of these was driven on purpose (below), and each leaves the board ready for the next
transfer rather than stuck:

| What happens | What each side sees |
|---|---|
| The receiver closes the page or cancels mid-way | the sender: *the receiver closed the download* |
| The sender closes the page mid-way | the receiver's download fails; the board: *the sender stopped* |
| Nobody accepts within a minute | the sender: *nobody accepted it in time* |
| One side connects and the other never does | given up after a minute, not held forever |
| Nothing moves for 20 seconds mid-transfer | given up |
| The receiver declines | the sender: *declined* |
| A request for an offer that is not this one, or without the PIN | refused (403) |
| An upload whose size is not the size offered | refused (400) |

**One transfer at a time.** The board has one radio and the file crosses it twice — in from
the sender, out to the receiver — so two transfers at once would only halve each other. A second
sender is told the board is busy.

## How big a file, really

"Any size" is true of the board, and only of the board. The limits that are left are elsewhere:

- **The board's memory: no limit.** It never holds more than one 4 KB buffer of the file.
- **Sizes are 64-bit.** The first version counted bytes in 32-bit numbers — the ESP32's natural
  size — and a file of 4 GB or more would have wrapped round to a small number and broken. A
  long 4K phone video passes 4 GB easily, so the counts are now 64-bit, including the one place
  where the bytes left is compared with a buffer size, which on this chip is still 32 bits. A
  4.4 GB file went through the pipe on a laptop intact (below).
- **Time.** Every byte crosses the board's single radio twice, so a big file takes a while. As a
  guide, at 500 KB/s a 100 MB video takes about three and a half minutes, and 1 GB about
  thirty-five. The real speed on the board is printed after every transfer.
- **The screen.** A phone whose screen locks may stop its upload or download, and a transfer
  that stands still for 20 seconds is given up. For anything large, keep the screens on.
- **The other end's storage**, and the browser: both phones and laptops handle multi-gigabyte
  downloads, but the space has to be there.

So in practice: photos, documents, screen recordings and videos of a few hundred megabytes go
across without thinking about it; multi-gigabyte files work, slowly, with the screens kept on.

## Speed

The file crosses the board's single radio twice, and the board's WiFi is the slow part. Through
the pipe on a laptop, with no radio involved, 20 MB moved at about **10 MB/s** — the pipe
itself is not the limit. On the board's WiFi it will be far lower; the serial line prints the
real figure after every transfer (`pipe: done — … KB/s`). **The measured speed on hardware is
still owed.**

## Notes, the PIN, and the clock

- **Notes** are the one thing stored, on the board's flash (LittleFS): up to 60, each up to
  8 KB, with who wrote it and when. Every device here sees them; *copy* puts one on the
  clipboard. Browsers only allow the clipboard API on `https` pages, so on the board's plain
  `http` page the copy falls back to the older way, which works in most browsers.
- **Notes clear themselves** after 24 hours by default — 1 hour, 7 days or never in settings.
- **The optional PIN** (4–8 digits) guards everything, the pipe included: the right PIN gets a
  browser a token, sent with every request. Five wrong PINs lock the door for a minute — four
  digits are only 10,000 guesses, and that should take a week, not a minute. Setting a new PIN
  signs every other device out. **Holding BOOT for five seconds** clears the PIN and the WiFi
  — the way back in for someone locked out; the notes stay.
- **The clock.** Expiry needs the time. On home WiFi the board asks NTP; on its own network
  there is no internet, but every phone that opens the page knows the time exactly, so the page
  sends it and the board sets its clock from the first one.

## With no WiFi at all

The board does not need a router, the internet, or any network but its own — only power. That
is what `devkit-drop` is for: a train, a classroom, a field, anywhere two devices need to swap a
file and there is no signal.

1. **Power the board** — a phone charger, a power bank, or a laptop's USB port. On a power bank
   the whole thing fits in a pocket.
2. **Wait about twenty seconds.** A board that has a home WiFi saved tries it first; when that
   network is not there, it gives up and starts its own network, `devkit-drop`, by itself. A
   board that was never given a WiFi starts it straight away.
3. **Join `devkit-drop` on both devices.** On a phone:
   - if it says the network has no internet, choose to **stay connected**;
   - **turn mobile data off** for the moment — otherwise a phone that decides this WiFi is
     useless may send the page's requests over mobile data instead, and they never reach the
     board (chapter 3 met exactly this).
4. **Open `http://192.168.4.1`** on both — the page often opens by itself. `drop.local` is for
   home WiFi only; on the board's own network, use the address.
5. **Send as usual.**

While they are on `devkit-drop`, neither device has the internet. Switch back afterwards.

## Two networks

| | Home WiFi (`drop.local`) | Its own network (`devkit-drop`) |
|---|---|---|
| Setting up | once, from the flasher's WiFi card — or `/setup` on its own network | none |
| Your internet while using it | kept | lost, while you are on `devkit-drop` |
| Works with no WiFi around | no | yes |

`wifi_setup.h` from chapter 6 does both, with one new switch (`appOnAp`): in the mirror the
board's own network only ever shows the WiFi page, because a mirror with no internet has
nothing to show; here the drop itself runs there, and the WiFi page moves to `/setup`. The
mirror's copy of the file carries the same change and behaves as before.

## Limits

- **Office and guest networks usually block it.** Most of them isolate devices from each other
  ("client isolation"): they can reach the internet but not one another. Use `devkit-drop`
  there.
- **Not encrypted.** It is plain `http` on the local network; anyone on that WiFi can see the
  traffic. The PIN keeps people out of the drop, not out of the air.
- **Both devices must be there at once.** That is the price of keeping nothing.
- **iPhone downloads** go to the Files app, not Photos.
- **Phones going to sleep** stop their page's heartbeat; a phone that locks mid-transfer may
  drop the download.

## Running it

**From the flasher:** choose *Drop a file, pick it up*, *Flash*, then either use the WiFi card
for your home network or skip it and join `devkit-drop`.

**From the Arduino IDE:** open `src/drop/drop.ino`, choose **ESP32 Dev Module**, default
partition. Every `.h` must sit beside the `.ino`.

Then open the page on two devices on the same network, and send something.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module with no warnings from the sketch:
**1,073,066 bytes — 81% of 1.31 MB**, globals at **17% of RAM**.

**The pipe, on a laptop:** `pipe.h` itself — not a copy — compiled against stand-ins for the
Arduino pieces backed by real TCP sockets, and driven with `curl` as sender and receiver:

- a **20 MB** random file — five times the board's whole flash — went through and came out with
  the same MD5, at about 10 MB/s with no radio in the way;
- the download carried the exact size and a name with an umlaut and an emoji in both forms
  (`filename` and RFC 5987 `filename*`);
- the browser's preflight was answered with the CORS headers an upload needs;
- refused, as they should be: a sender before the receiver had accepted, a wrong token, an
  upload of the wrong size;
- a receiver that hung up after 2 MB: the sender was told *the receiver closed the download* at
  once, and the pipe reset;
- a receiver whose sender never came: given up after the wait, not held.

**Then 64-bit sizes:** a **4,400,000,000-byte** file — past the 4 GiB line where 32-bit counts
wrap — went through the same pipe with the right `Content-Length`, every byte received, and the
same MD5.

**Confirmed on hardware:** flashed from the flasher, and a file sent from a phone to a laptop
through the board.

**Not yet measured on hardware:** the speed over the board's WiFi (printed after each
transfer), a file of several gigabytes, Safari on an iPhone, notes and the PIN, and the
fallback to `devkit-drop` when the home WiFi is away.
