# 11 · Tambola

> **Needs:** one ESP32 DevKit V1 and the phones in the room. A laptop or TV for the caller screen
> is nice, not needed.
> **Status:** written and tested on a laptop. The rules, through the same code, over 2,000 whole
> games; the real page in a browser against a laptop build of the board's game (figures below).
> Not yet run on the board itself.

Tambola, or Housie, for the whole family, with the board as the caller and the referee. One phone
starts a game and gets a four-letter code. Everyone else joins with it and picks one to six
tickets. The board makes the tickets, draws the numbers, calls one every few seconds, and checks
every claim against what has actually been called. Nobody can cheat and nobody has to check a
ticket by hand. Any laptop or TV can show the big caller screen, and the calls are read out in
English, with the traditional calls, or in **Hindi**.

## Playing

1. **Start:** open **http://tambola.local**, type your name, choose your tickets, and tap **Start
   a new game**. You're the host. Everyone else types the code, or taps the game in the list.
2. **The host chooses:**
   - **Next number:** every 5, 8 or 12 seconds, or **I tap** to call each one by hand.
   - **Claims:** *Friendly* or *Strict* (below).
   - **Calling:** *English calls* ("Kelly's eye, number one", "Two fat ladies, 88"), plain
     *numbers* ("eight and eight, 88"), or **हिंदी** ("आठ आठ, अट्ठासी").
   - **Prizes:** Early Five, the three lines, Four corners and Full House are on by default, and
     **Second Full House** can be turned on. Full House is always on.
3. **Tap Start.** The ball on every phone shows the number, and a ring counts down to the next
   one. Tap numbers on your tickets to mark them.
4. **Claim** when you have a prize. The board checks your tickets. Everyone sees the win, and the
   caller announces it.
5. **New round** keeps everyone in the room with new tickets, and the leaderboard adds up the
   points.

**Tickets are proper tickets:**
- 3 rows by 9 columns, 15 numbers, 5 in every row and at least one in every column.
- Column 1 holds 1–9, column 2 holds 10–19 and so on to 80–90, each column read top to bottom in
  order.
- They come in **strips of six** that use every number from 1 to 90 exactly once, as in printed
  books. All your tickets come from one strip, so two of them never share a number.

**Claims and the two modes:**
- **Friendly**, for kids and mixed ages: called numbers glow on your tickets, *Mark called
  numbers for me* can mark them for you, and Claim only lights up when you really have the
  prize.
- **Strict**, the traditional way: nothing glows, you spot it yourself, and a wrong claim is a
  **Bogey!** Everyone sees it, and you're out of that prize for the round.

**Fair claims:**
- Only numbers already called count. A prize won on a number can be **shared** by anyone else who
  has it on that same number.
- After a win, the board waits 5 seconds before the next number, so a second winner has time to
  claim.
- After the next number, the prize is closed. Points are split between those who share it.
- The round ends 5 seconds after the last prize goes, or when all 90 have been called.

**The caller screen:** open *Show the caller on a TV or laptop* from the lobby, or go to
`tambola.local/#tv=CODE` on any screen. It shows:
- a big ball with the number;
- the call in words;
- the last five numbers;
- the 1–90 board lighting up;
- the prizes and winners, and the leaderboard;
- the address and code, so latecomers can find the game for the next round.

Tap the screen once to turn its voice on, since browsers won't speak until you do.

**Voices:** every device can speak, using the voices it already has. A 🔊 button switches it per
device: the host's phone and the caller screen speak by default, and players' phones don't (one
voice is enough). Hindi needs a Hindi voice on that device. Phones have one, and macOS includes
*Lekha*. If there isn't one, the device says so and calls in English.

Also from Ludo: emoji reactions that float up on every screen, rejoining after a refresh (your
seat is kept on your phone), and a list of open games on the start screen. If the host's phone
goes quiet for a minute, the next player becomes the host.

## The thing worth understanding

**Making a strip.** Six tickets that use 1–90 exactly once, with every rule above holding, is a
small puzzle. The board does it in two passes, with a random retry when it paints itself into a
corner:

1. **How many from each column.** Every ticket gets one number from each column, which uses 54.
   Column 1 has 9 numbers, columns 2–8 have 10 and column 9 has 11, so 36 are left. Each goes to
   a random ticket that has fewer than 15 numbers and fewer than 3 in that column.
2. **Which rows.** For each ticket, the fullest columns go first, because they have the least
   choice. Each number goes into the emptiest row that can still take one, so every row ends with
   exactly five.

Over 5,000 strips (30,000 tickets) in the tests, every one was valid, and the columns came out
varied: 123,070 holding one number, 113,860 holding two and 33,070 holding three.

**One set of rules, two servers.** Everything that decides anything is in `tambola.h`:
- the tickets, the draw, the prizes;
- the rooms, players, host hand-over and calling clock;
- the JSON the phones get;
- a single `route()` that answers every request.

`tambola.ino` only hands it requests from the ESP32's web server. `tools/devserver.cpp` hands it
the same requests from a laptop. So the end-to-end tests below run against the board's own rules,
not a copy of them. You can also play on a laptop without flashing anything.

## Endpoints

All take the game code `c` and the player token `t`. Every answer is JSON.

| Path | Body | |
|---|---|---|
| `/rooms` | | open games, for the start screen |
| `/create` | `n` name, `k` tickets | a new game; you're the host |
| `/join` | `n`, `k` | join before it starts (or rejoin with your token) |
| `/state` | `v` ticket version | everything; your tickets only when they changed |
| `/tickets` | `k` | how many tickets, or a new set |
| `/claim` | `p`: early, top, middle, bottom, corners, full, full2 | `won`, `shared`, `taken`, `notyet`, `bogey`, `out` |
| `/emoji` | `e` 0–7 | |
| `/leave` | | |
| `/settings` | `pace` (0, 3–30 s), `strict`, `lang` (0–2), `prizes` (mask) | host |
| `/start`, `/pause`, `/next`, `/end`, `/again` | | host: start, pause/resume, call one, stop, new round |

## Files

```
src/tambola/tambola.ino     the board: WiFi and the web server, nothing else
src/tambola/tambola.h       the whole game: tickets, draw, prizes, rooms, the clock, the JSON
src/tambola/tambola_page.h  the page: start, lobby, tickets and claims, caller screen, results, voices
src/tambola/wifi_setup.h, improv.h, ui.h   home WiFi or its own network (shared with chapters 7–10)
tools/tambola_test.cpp      the rules, tested on a laptop
tools/devserver.cpp         the same game on a laptop, for trying it and for the end-to-end tests
```

## Limits

- **How many phones:** up to 30 players per game, 3 games at once, and anyone can watch. On your
  home WiFi that's fine. On the board's own network, `devkit-tambola` (no router), the ESP32 lets
  in at most **10** devices. The board raises its limit from the default 4 to that.
- **Joining:** only before a round starts. Latecomers can watch and join the next round.
- **No money.** Points and a leaderboard only. Settle the pot however your family does.
- **Voices** depend on the device, as above.

## Running it

Flash **Tambola** from the flasher and give it your WiFi on the same page. Then open
**http://tambola.local** on a phone. Without home WiFi, join `devkit-tambola` and open
`192.168.4.1`.

To try it on a laptop with no board:

```
cd tools && c++ -std=c++17 -I../src/tambola devserver.cpp -o devserver && ./devserver 8090
```

Then open http://localhost:8090, or this computer's address from phones on the same WiFi.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module, on the default partition, with no
warnings from the sketch: **1,027,892 bytes, 78% of 1.31 MB**, and globals at **24% of RAM**. The
largest reply (30 players, 6 tickets, all 90 called) is 3.3 KB.

**The rules, on a laptop** (`tools/tambola_test.cpp`), **41,139 checks**:
- **Tickets:** 5,000 strips (30,000 tickets) all valid, each number once per strip, a player's
  tickets never sharing a number.
- **The draw:** all 90, once each, in a new order every game.
- **Every prize pattern on a known ticket**, including "not yet: the fifth number came later".
- **A room through the same requests the phones send:**
  - names cleaned, including Hindi names never cut mid-letter and markup removed;
  - rejoining keeps your seat, a watcher gets no seat, and only the host can start;
  - Full House can't be switched off;
  - the countdown to the first number and the pace between numbers;
  - a wrong strict claim is a Bogey, and the player is out of that prize;
  - a win, then a share on the same number, then "taken" after the next number;
  - Second Full House waits for the first;
  - pause, resume and manual Next;
  - a new round carries the points over and deals new tickets;
  - host hand-over, leaving, rooms closing, and the room and game limits.
- **2,000 whole games** with 2–26 players and 1–6 tickets each, Friendly and Strict, with random
  mistaken claims. Every game ended, with 1,262 shared prizes and 15,231 Bogeys along the way.
  Every prize was won in every game where no Bogey had put everyone out of it.
- **The biggest room's JSON** is well-formed.

**End to end**, with the real page in a browser against the laptop build of the board's game and
seven automated phones, **11 of 11**:
- A game was started from the page's own button.
- In the host's lobby, choosing Strict and हिंदी and adding a ticket all reached the board.
- Start, pressed in the page, started the game.
- A player marked numbers, claimed Early Five from the page and won it.
- The round ended by itself after Full House and the 5-second share window.
- New round from the page went back to the lobby with scores kept.
- A strict early Full House claim was a Bogey, announced to everyone.
- The caller screen was shown in English and in Hindi.

**Not yet confirmed:**
- the game on the board itself with real phones;
- how many phones polling at once the ESP32's web server handles comfortably on a busy home WiFi
  (ten to fifteen should be fine; thirty is the design limit);
- the voices on Android, iPhone and Windows (macOS has Hindi *Lekha* and English (India) voices).

## Sources

- The ticket rules as printed Tambola/Housie books use them: 3×9, 15 numbers, 5 per row, column
  ranges, strips of six.
- The traditional bingo/housie calls, and Hindi number words 1–90.
- The Web Speech API (`speechSynthesis`) for the voices.
- Chapter 8 (Ludo) for rooms, codes, seats and emoji reactions. Chapters 7–10 for `wifi_setup.h`.
