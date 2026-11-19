# 8 · Ludo

> **Needs:** one ESP32 DevKit V1, and two to four phones to play (plus any number to watch).
> **Status:** compiles against the core, and the whole rule engine is tested on a laptop —
> including twenty thousand random games played to the end — with the board rendered and checked
> in a browser (figures below). Not yet flashed.

The game everyone knows, refereed by the board. One phone creates a room and gets a four-letter
code; up to three more join, and anyone else can **watch**. The board is the only thing that
rolls the die and the only thing that decides a move is legal, so a phone only ever says
**"roll"** or **"move token 2."**

It runs on your **home WiFi** (`http://ludo.local`), or makes its own network **`devkit-ludo`**
as a fallback — so you can play on the sofa with everyone's internet intact, or anywhere at all
with no WiFi around.

Two modes: **classic** (four tokens each) and **quick** (two tokens, a game in a few minutes).
Plus a **turn clock** so nobody stalls the table, **reaction emojis** to fling across the board,
and a **spectator view** for a laptop or TV.

---

## What makes it different from the other chapters

Tic-tac-toe and dots-and-boxes are small and exact — a few cells, a turn each. Ludo is a real
board game: four players, four tokens apiece, a die, captures, safe squares, bonus turns, and a
long path home. The rules are fiddly enough that the only way to trust them is to **play the
game tens of thousands of times and check it never breaks** — which is exactly what happens off
the board, before any of it touches a radio.

## The rules the board enforces

Everything below lives in `ludo.h`, with no radio or web anywhere near it:

- **A six to come out.** A token sits in its base until you roll a 6, which brings it onto your
  coloured start square.
- **Round and home.** A token travels the shared 52-square ring, then up its own five-square
  home column to the centre. You need the **exact** number to finish — an overshoot is not a
  legal move.
- **Capture.** Land on a square holding a lone opponent token (and it isn't a safe star) and
  that token goes all the way back to its base.
- **Safe squares.** The eight starred squares — each player's start and a star partway along —
  can't be captured. And **two of your own tokens on one square** are a blockade: safe, and an
  opponent can't land there.
- **Bonus turns.** Roll a 6, make a capture, or send a token home, and you roll again — but
  **three sixes in a row** forfeits the whole turn.
- **Win** by getting all your tokens home.

### How a token's journey is counted

Each token carries a single number, its **progress** from 0 to 57:

```
0            in its base
1 .. 51      on the shared ring; the real square is (start + progress - 1) mod 52
52 .. 56     the five squares of its own home column
57           home — finished
```

That one number is the whole model. Two tokens collide only when they map to the same **ring**
square; a base, a home column and the centre are private, so no cross-player comparison is ever
needed there. The page draws the board from exactly these numbers, using the same ring mapping,
so what you see and what the referee computes can never drift apart.

## The thing worth understanding

**A board game is only fair if one referee owns the dice and the rules.**

The die is rolled on the board, from its hardware random generator — a phone can't pick its own
number. Every move is checked against the rules on the board — a phone can't move a token
further than the die allows, out of base without a six, or onto a safe square to capture.
A phone holds no game state at all; it just draws what the board sends and sends back "roll" or
"move 2." So the game can't be cheated by a edited page or a forged request, because nothing
a phone claims is trusted — only what the referee computes.

> **What you learn:** why every online board game is an *authoritative server* with thin
> clients, and how much of "the rules" is really "the server refusing illegal requests."

## The turn clock

Each turn has **25 seconds**, kept by the board. Run it out and the board plays for you: it
rolls, and if a move is needed it makes a sensible one — prefer finishing a token, then a
capture, then the furthest-along token. It's the same idea as the clocks in the other games,
and here it earns its keep, because the classic way a Ludo game dies is one person taking
forever while everyone drifts off.

## Reactions, and watching

- **Emojis.** A row of reactions — 😂 🔥 😮 😎 😭 👏 🎲 💀 — that pop over the thrower's corner
  and sit by their name for a moment. It's the small thing that makes an online game feel like
  people are at the table.
- **Spectators.** Anyone can open a room and **watch** without taking a seat — the board hands
  a viewer a read-only token. Put the room on a laptop or TV and the whole table watches the
  board while four phones play it.

## Leaving, reconnecting, playing again

- A player whose phone goes quiet for a minute, or who leaves, **drops out of the game** — their
  tokens freeze where they fell and their turns are skipped. If fewer than two remain, the game
  ends. (Reconnecting before that keeps your seat; the page remembers your room and token.)
- **Play again** restarts with whoever is seated, and the opener rotates each game.

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the page |
| `/rooms` | GET | — | rooms you can join or watch |
| `/create` | POST | `n`, `m` | new room, `m`=0 classic / 1 quick → `{code, token, seat}` |
| `/join` | POST | `c`, `n`, `t` | take a seat; with a known token, the same seat back |
| `/watch` | POST | `c` | a read-only viewer token |
| `/state` | GET | `c`, `t` | the whole game, from this seat's (or a viewer's) point of view |
| `/start` | POST | `c`, `t` | the host, with two or more seated |
| `/roll` | POST | `c`, `t` | roll the die (and auto-play the move if there's only one) |
| `/move` | POST | `c`, `t`, `k` | move token `k` |
| `/again` | POST | `c`, `t` | after a game, start the next |
| `/emoji` | POST | `c`, `t`, `e` | fling reaction `e` |
| `/leave` | POST | `c`, `t` | give up the seat |

## Two networks

Like the file drop, Ludo uses `wifi_setup.h` so it runs on your home WiFi **or** its own network:

| | Home WiFi (`ludo.local`) | Its own network (`devkit-ludo`) |
|---|---|---|
| Setting up | once, from the flasher's WiFi card — or `/setup` on its own network | none |
| Internet while playing | kept | lost, while you're on `devkit-ludo` |
| How many can connect | more (the router serves them) | up to 8 phones (the DHCP ceiling) |

## Limits

- **Four players and four rooms.** Plenty for a chip this size; the serial line prints the room
  size on boot.
- **Spectators share the connection count.** On the board's own network, players + watchers add
  up to the 8-phone ceiling; on home WiFi, more.
- **No blockade-passing rule.** You can't *land* on an enemy blockade, but you can pass over it —
  a common simplification; the full "a blockade blocks the road" rule is left out.
- **A restart forgets every game.** Rooms live in RAM.

## Running it

**From the flasher:** choose *Ludo*, *Flash*, then either use the WiFi card for your home network
or skip it and join `devkit-ludo`.

**From the Arduino IDE:** open `src/ludo/ludo.ino`, choose **ESP32 Dev Module**, default
partition; every `.h` sits beside the `.ino`.

Then open the page on two to four phones, create a room, and start. Put it on a laptop or TV with
**watch** to follow along.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module on the default partition, with no
warnings from the sketch: **1,010,000 bytes — 77% of 1.31 MB**, globals at **15% of RAM**.

**The rule engine**, on a laptop, is where the real checking is. First the rules one at a time:
a six needed to leave base, a capture sending an opponent home, a safe star and a two-token
blockade both surviving, the exact number needed to finish, the bonus turn on a six / capture /
finish, three sixes forfeiting the turn, and the win. Then **twenty thousand random full games**
— two, three and four players, classic and quick — every one played to a winner, with the token
count conserved on every single move, no game ever looping without end (the longest ran 632
turns), and every winner's tokens all genuinely home. Captures happened 81,000 times across
those games.

**The board**, rendered in a browser from a mid-game state: the four bases, the ring with its
coloured starts and safe stars, the home columns, the pinwheel centre, and the tokens placed from
the same progress numbers the engine uses — all correct, with the movable token ringed. The
page's script passed its syntax check.

**Not yet confirmed on hardware:** all of it on a real board and real phones — four players at
once, the die on the board's own random generator, the turn clock playing for a slow player, the
emojis and the spectator view, and the fallback to `devkit-ludo`.

## Sources

- The ESP32 Arduino core's `WebServer` and `esp_random()`.
- Chapters 3 and 5 for rooms and the turn clock; chapter 7 for the dual-network `wifi_setup.h`.
