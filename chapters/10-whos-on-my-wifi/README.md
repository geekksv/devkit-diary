# 10 · Who's on my WiFi

> **Needs:** one ESP32 DevKit V1 and a phone, on your home WiFi. Computers are optional (for the
> messages).
> **Status:** written and tested on a laptop. The scanner's packets were tried against a real home
> network from a Mac, and the page, the messages and the installer were tested end to end against
> a stand-in board (figures below). Not yet run on the board itself.

A network scanner in the spirit of Angry IP Scanner and Advanced IP Scanner, living on the board.
It finds everything on your WiFi, including phones and PCs that ignore ping, and works out what
each device is. It keeps watching, and tells you when something new joins. A phone opens
**http://scan.local** to see the list, name things, check a device's open ports, wake a sleeping
PC, watch a live latency graph and export the list as a spreadsheet.

As an extra, any computer that runs a small inbox helper can be **sent a message** from the list.
The message appears on top of whatever is open there, with a box to reply.

## What it finds out

For every address on your network:

| | How | What you see |
|---|---|---|
| **Is anything there?** | ARP: "who has 192.168.1.7?" Everything on a WiFi must answer this, firewall or not, so it finds phones and Windows PCs that ignore ping. | the device, its IP and MAC address |
| **Who made it** | The first half of the MAC address is assigned to a maker by the IEEE. The board carries 14,046 of those prefixes from 111 makers likely in a home (54 KB). | Apple, Samsung, TP-Link, Espressif… |
| **What it calls itself** | Five questions, best answer wins: **Bonjour browsing** (printers, Macs, TVs, speakers announce themselves), **Bonjour** asked directly, **NetBIOS** (Windows), the **router's DNS** (which often knows names from DHCP), and **UPnP** (TVs and routers describe themselves in XML). | "Keshav's MacBook Air", "HP Smart Tank 580-590 series", "DESKTOP-K2M8QJ", "[TV] Living Room" |
| **How fast it answers** | One ping. | 4 ms, in green, amber or red |
| **What it is** | Guessed from its name, maker, the services it offers and its open ports; you can correct it. | 📶 router, 📱 phone, 💻 computer, 📺 TV, 🖨️ printer, 🔊 speaker, 🎮 console, 📷 camera, 💡 smart home… |
| **Open ports** *(when you tap)* | It tries to connect to 25 common ports: web, SSH, file sharing, Remote Desktop, printing, AirPlay, Chromecast, Plex… A web page's `<title>` is read too, with an **Open ↗** link. | which doors are open |

**Phones and "private addresses".** Phones now use a made-up MAC address on each network, so the
first half means nothing and the maker is unknown. The scanner recognises these (one bit in the
address says so). It shows them as "Private address" and guesses they are a phone.

## What you can do with it

- **Watch the list.** Here now, or everything ever seen, with when each device was last seen. You
  can search by name, IP, maker or MAC, and sort by IP, name, speed or last seen.
- **Know when something new joins.** The first scan is the baseline. Anything that appears after
  it is marked **NEW**, and a banner says so until you mark it as yours. If you like, your
  computers get a message too ("Something new joined your WiFi…").
- **Name things.** "Espressif device at .84" becomes "Kitchen plug", and you can set its type. The
  board remembers it by its MAC address, so the name follows it even when its IP changes.
- **Wake a PC.** Wake-on-LAN sends the "magic packet" to a sleeping computer. The PC has to have
  Wake-on-LAN switched on in its settings or BIOS.
- **Latency graph.** The board pings once a second and draws the last two minutes, with figures
  for the last minute: now, average, jitter (how jumpy it is) and loss. It marks the result
  Smooth, OK or Laggy. Ping the **router** to see whether your WiFi is the problem, **1.1.1.1** to
  include your internet, or **any device** from its page.
- **Export** everything as a CSV for a spreadsheet. A device name that starts with `=` can't run
  as a formula.
- **Settings:** how often a full scan runs (1–60 minutes, 5 by default), and whether to tell your
  computers about new devices.

A **full scan** (who's there, ping, names, services, UPnP) runs every few minutes. A **quick
scan** (who's there) runs every minute in between, so the list stays current. A device that
misses three in a row has left, because phones asleep often skip one.

## Messages, the extra

Any computer can be given a 📨 button in the list. Run this once on it:

```
curl -fsSL http://scan.local/i | sh          # macOS, Linux
irm http://scan.local/i.ps1 | iex            # Windows (PowerShell)
```

It is small: one Python file, using only the standard library and the Python already on the
computer, so nothing is downloaded from the internet. Unlike chapter 9's pet, this takes a couple
of seconds. It starts by itself at login.

Then, from the phone, pick the computer (or **Everyone**), type, and send. Within two seconds a
window appears on top of whatever is open there, with a box to reply. The reply shows up in the
conversation on the phone.

How it works: **nothing listens on the computer.** The helper asks the board "anything for me?"
every two seconds and collects its messages. The board knows which row of the list the helper is
by the address the question came from. No firewall door is opened, and nothing else on the WiFi
can reach the computer through it.

The window is the system's own: an AppleScript dialog on macOS, a small always-on-top window on
Windows, and `zenity` or `kdialog` on Linux (falling back to a Python window or a notification).
To remove it, the installer's last line tells you how: `curl -fsSL http://scan.local/u | sh`,
or `irm http://scan.local/u.ps1 | iex`.

Why not send a message to *any* device? Because nothing modern accepts one. Windows used to have
`net send`, but it was removed after being abused. Phones and TVs have no such door at all. A
computer has to agree to listen, and the helper is that agreement.

## The thing worth understanding

**ARP, eight at a time.** To send anything to 192.168.1.7, the board first broadcasts "who has
192.168.1.7? tell me your MAC". The device has to answer, or nothing on the WiFi could ever reach
it. That makes ARP a scan that a firewall can't hide from.

The catch on an ESP32 is that lwIP, its network stack, remembers only **ten** answers. So the
scanner asks eight addresses, waits 350 ms, reads back what answered, and moves on. A whole /24
(254 addresses) takes about twelve seconds. The ARP table belongs to lwIP's own thread, so both
the asking and the reading are handed to that thread (`tcpip_callback`) rather than done from the
scanner's.

**Five ways to ask a name, because no one way works.** Macs answer a Bonjour question sent
directly to them. Windows answers NetBIOS. Many routers answer a reverse DNS lookup for their DHCP
clients. Printers, TVs and speakers ignore all three, but they *announce* themselves when anyone
asks the Bonjour group "which printers are here?". So the scanner asks every way at once, matches
each answer to its question by an ID, and keeps the best. On a real home network, the direct
questions got one name and the browse got five: a printer, two MacBooks, a Raspberry Pi and the
chapter 9 pet board.

**Everything in its own task.** Scanning blocks: waiting for ARP, for pings, for names. So it runs
in a FreeRTOS task of its own, and the web server in `loop()` never waits for it. The device table
is shared between the two behind a mutex. The latency pinger runs in a third task (ESP-IDF's
`esp_ping`) and writes to its own ring buffer.

## Endpoints

| Path | Method | Body | |
|---|---|---|---|
| `/` | GET | | the page |
| `/state` | GET | `v` (device list version), `t` (phone's clock) | status, latency, messages; devices only when changed |
| `/scan` | POST | | scan now |
| `/ports` | POST | `ip` | check that device's ports |
| `/name` | POST | `m` (MAC), `n` name, `k` kind | name it, set its type |
| `/seen` | POST | `m` or `all` | mark as known |
| `/forget` | POST | `m` | forget a device |
| `/wake` | POST | `m` | Wake-on-LAN |
| `/watch` | POST | `w`: `router`, `internet` or an IP | what the latency graph pings |
| `/settings` | POST | `every` (min), `alert` (0/1) | |
| `/send` | POST | `to` (IP or `all`), `t` | message a computer |
| `/inbox` | GET | `n` name, `o` os | the helper collects its messages |
| `/reply` | POST | `t` | the helper's reply |
| `/export.csv` | GET | | every device |
| `/i`, `/i.ps1` | GET | | the inbox installer |
| `/u`, `/u.ps1` | GET | | the uninstaller |
| `/devkit_inbox.py` | GET | | the inbox helper |

## Files

```
src/scanner/scanner.ino    the board: the scanner task, the device list, messages, the web side
src/scanner/net.h          devices, packets, guesses and sums — no WiFi, tested on a laptop
src/scanner/probe.h        the network side: ARP, ping, names, Bonjour, UPnP, ports, wake
src/scanner/oui.h          GENERATED by tools/oui.py: makers by MAC prefix
src/scanner/page.h         the phone page
src/scanner/files.h        GENERATED by helper/embed.py: the inbox helper and installers
src/scanner/wifi_setup.h, improv.h, ui.h   getting onto your WiFi (shared with chapters 7–9)
helper/devkit_inbox.py     the inbox helper for computers
helper/install.sh, install.ps1, uninstall.sh, uninstall.ps1
helper/embed.py            regenerates files.h — run it after changing anything in helper/
tools/oui.py               regenerates oui.h from the IEEE list
tools/net_test.cpp         the laptop tests for net.h
```

## Limits

- **Home WiFi only.** It has to be on the network it scans. Its own network, `devkit-scan`, is
  only for setting up WiFi.
- **A /24 at most.** On a bigger subnet it scans the 254 addresses around itself.
- **Names aren't guaranteed.** An Android phone or a cheap smart plug may answer none of the five
  questions. It then shows its maker ("Espressif device") or "Private device" until you name it.
- **Mesh and guest networks.** Devices on a separate guest network, or isolated by the router's
  "client isolation", can't be seen. That is the point of those features.
- **The partition.** The firmware is 1.2 MB, more than the default layout allows, so this chapter
  uses **Minimal SPIFFS** (1.9 MB app). The flasher picks it for you. In the Arduino IDE, choose
  *Tools → Partition Scheme → Minimal SPIFFS*. What it learns (names, first-seen times) is kept in
  a small file in that SPIFFS area, so it survives restarts.
- **Be polite.** Scan your own network. Port checks only happen when you tap the button, one
  device at a time.

## Running it

1. Flash **Who's on my WiFi** from the flasher and give it your WiFi on the same page, or join
   `devkit-scan` from a phone and pick your network there.
2. Open **http://scan.local** (or the address the Serial Monitor prints). The first full scan
   takes about 20 seconds.
3. Optional: on each computer you want to message, run the one line from the **Messages** card.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module with the Minimal SPIFFS partition
and no warnings from the sketch: **1,204,849 bytes, 61% of 1.9 MB**, globals at **28% of RAM**.
The first build used the stock `HTTPClient` for UPnP and page titles and came to 99% of the
default partition, because it brings TLS with it. A 30-line plain HTTP GET replaced it and saved
107 KB.

**The logic, on a laptop** (`tools/net_test.cpp`), **82 checks**:
- maker lookup (Apple, Espressif, Raspberry Pi, TP-Link, Intel; a private address gives none) and
  the table's sort order;
- the device table: first scan as baseline, newcomers marked new, three misses to leave, moving
  to a new IP drops what was learned at the old one, and a full table makes room by dropping the
  oldest device you haven't named that isn't present;
- name priority;
- type guesses from names, makers, ports and services;
- every packet built and parsed: reverse lookups (Bonjour and router style, with compression),
  NXDOMAIN, a pointer loop, NetBIOS node status, a Bonjour browse reply shaped like a real
  printer's;
- UPnP XML, SSDP headers and web page titles;
- the Wake-on-LAN packet;
- latency stats and jitter;
- CSV quoting, including formula defusing.

**On a real home network, from a Mac**, with the exact packet code the board uses (net.h, built
for macOS):
- The Bonjour browse found a printer ("HP Smart Tank 580-590 series", offering printing, scanning
  and a web page), two MacBooks by their owners' names, a Raspberry Pi and the chapter 9 pet
  board.
- The direct Bonjour question named the Mac itself.
- That router didn't answer reverse DNS, and these devices had NetBIOS off. That is why there are
  five ways to ask.

**The page and the messages, end to end**, against a stand-in board that speaks the same JSON:
- The real inbox helper, fetched from the board the way the installer does, was byte-identical to
  the source.
- It checked in, and the computer's row got its 📨. A second copy refused to start.
- A message typed into the page's own box reached it and was marked delivered, and the reply came
  back into the conversation.
- From a device's page, naming it, Wake, checking ports and following it on the latency graph all
  reached the board.
- Marking a new device as known, switching the graph to the internet, and exporting the CSV all
  worked.
- **13 of 13.**

**The installer**, run for real (`curl … | sh`) into a sandboxed home folder with `launchctl`
stubbed:
- It took 1.7 s, with no downloads.
- It fetched the helper byte-for-byte, saved the board's addresses, and wrote a valid
  LaunchAgent that starts the helper at login with this computer's Python and restarts it on a
  crash. Then it started it.
- It ends by saying how to remove it, marked optional.
- Running it again was fine, and the uninstaller removed everything.
- **9 of 9.**

The macOS message dialog compiles as AppleScript (`osacompile`). It wasn't shown during testing.

**Not yet confirmed:**
- the scanner on the board itself: the ARP sweep, `esp_ping`, Bonjour from the ESP32, storage;
- Wake-on-LAN on a real PC;
- the Windows installer and message window (written for PowerShell 5.1 and tkinter, not run);
- Linux.

## Sources

- lwIP: `etharp_request`, `etharp_get_entry`, `tcpip_callback`; ESP-IDF `esp_ping` (`ping_sock.h`).
- RFC 6762 / 6763 (Multicast DNS, DNS-SD: browsing, and replies to a query's own port); RFC 1035
  (DNS messages, name compression); RFC 1002 (NetBIOS node status); UPnP Device Architecture
  (SSDP `M-SEARCH`, device description).
- The IEEE MA-L public listing (`oui.csv`) for makers.
- AMD's Magic Packet (Wake-on-LAN) format.
- Chapters 7–9 for `wifi_setup.h`; chapter 9 for the one-line installer.
