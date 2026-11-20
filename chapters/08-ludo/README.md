# 8 · Ludo

> **Needs:** one ESP32 DevKit V1, and two to four phones to play (plus any number to watch).
> **Status:** flashed and played on a real board with real phones. The rule engine is tested on a
> laptop — twenty thousand random games played to the end — and the page is checked in a browser
> at phone and desktop sizes.

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

Each turn has **25 seconds**, kept by the board, and rolling gives you a fresh 25 to choose.
Run it out and the board plays for you: it rolls if you hadn't, then makes a sensible move —
prefer finishing a token, then a capture, then the furthest-along token. It's the same idea as
the clocks in the other games, and here it earns its keep, because the classic way a Ludo game
dies is one person taking forever while everyone drifts off.

On the page the clock is a bar along the bottom of the active player's panel, with the seconds
beside their score. It's white, turns amber under ten seconds and red under five — deliberately
never a player's colour, so it can't blur into the panel it sits on.

## The obvious move plays itself

Most rolls in Ludo don't leave a real decision: every token in base and you roll a six, or one
token out and the rest still waiting for a six, or two tokens stacked on one square. The board
counts the **genuinely different** moves a roll allows — tokens standing together are
interchangeable, so moving any of them is the same move — and when there is only one, it plays it
itself about a second after the roll, once the die has landed and you've seen the number. When
there is a real choice, it's yours.

On a laptop, the counting was checked case by case (all in base on a six, one token out, a six
with one out — a real choice — a stacked pair, two apart, the exact last roll home) and then
across twenty thousand more full games with every one-choice turn auto-played: all finished,
every move legal. About a third of all rolls turn out to be one-choice, so that's a third less
tapping.

## The page

- **A panel for each player, around the board** — two above, two below, each with the player's
  colour, name and how many tokens are home. The panel whose turn it is lights up, with a thin bar
  along its bottom counting down the turn clock.
- **The die sits in your own panel.** When it's your turn it breathes; tap it and it tumbles for a
  moment before landing on the number the board rolled. The number stays up while you choose a
  token, so you always see what you got. A roll with no legal move lingers briefly, so nobody
  misses it.
- **A dot where each token would land.** Tap the token or the dot.
- **Captures are announced** — "Ria sent your token home" — and your phone buzzes briefly and the
  tab title changes when your turn comes round.
- **Tokens hop square by square** along the track, around corners, instead of sliding across the
  board. Tokens you can move get a ring; two on one square sit side by side; finished tokens line up
  in their colour's triangle in the centre.
- **Two players sit across the board** — red and yellow, or green and blue — never side by side,
  so each has a clear half. The second to join takes the colour opposite the host, and if two
  players end up next to each other anyway, the board moves one across when the game starts.
- **Only the seated colours have pieces.** In a two-player game the other two bases stay empty.

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
| `/state` | GET | `c`, `t` | the whole game, from this seat's (or a viewer's) point of view, with the last roll |
| `/start` | POST | `c`, `t` | the host, with two or more seated |
| `/roll` | POST | `c`, `t` | roll the die; with one real choice the board plays it a second later, otherwise you tap a token |
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
warnings from the sketch: **1,017,876 bytes — 77% of 1.31 MB**, globals at **15% of RAM**.

**The rule engine**, on a laptop, is where the real checking is. First the rules one at a time:
a six needed to leave base, a capture sending an opponent home, a safe star and a two-token
blockade both surviving, the exact number needed to finish, the bonus turn on a six / capture /
finish, three sixes forfeiting the turn, and the win. Then **twenty thousand random full games**
— two, three and four players, classic and quick — every one played to a winner, with the token
count conserved on every single move, no game ever looping without end (the longest ran 632
turns), and every winner's tokens all genuinely home. Captures happened 81,000 times across
those games.

**The page**, rendered in a browser at phone and desktop widths from lobby, mid-game, two-player,
late-game and win states: the ring, starts, stars, home columns, the turn arrows and the centre
all where they belong, tokens placed from the same progress numbers the engine uses, stacked and
finished tokens laid out cleanly, and no sideways overflow. The die animation and the
square-by-square hop were checked by driving one state into the next.

**On hardware:** flashed and played to a win on a real board and phones. Still worth a longer
session: four phones at once, the turn clock playing for a slow player, and the spectator view on
a TV.

## Sources

- The ESP32 Arduino core's `WebServer` and `esp_random()`.
- Chapters 3 and 5 for rooms and the turn clock; chapter 7 for the dual-network `wifi_setup.h`.
