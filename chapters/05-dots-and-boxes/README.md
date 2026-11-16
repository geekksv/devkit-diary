# 5 · Dots and boxes

> **Needs:** one ESP32 DevKit V1. Two to four phones per game — up to eight phones on the board.
> **Status:** flashed and played on a phone and a tablet — 4×4, 6×6 and 9×9 boards, the
> one-screen layout and the last-move mark. The rules are also tested on a laptop, including
> five hundred random games played to the end (figures below).

The board makes its own WiFi network and serves dots and boxes to every phone that joins. One
phone creates a room, picks a board — 4×4 boxes for a quick game, 6×6 for the classic, 9×9
for a long one — and gets a four-letter code. Up
to three more join with the code or from the list of open rooms, and the host starts the game.

Players take turns drawing one line between two neighbouring dots. Draw the fourth side of a
box and it is yours, coloured and initialled — **and you move again**. When every line is
drawn, most boxes wins.

---

## What makes it different from the other chapters

Chapter 3's tic-tac-toe is strictly two players taking strict turns. Dots and boxes breaks both:

- **More than two players.** Up to four, sitting round a table that people can leave mid-game.
- **Turns are not alternate.** Whether you move again depends on what your line did — so the
  board cannot know whose turn is next until it has worked out the consequences of the move.

That second rule is the whole game. Without it, dots and boxes is a quiet colouring exercise.
With it, every move late in the game is a decision about how many boxes to hand an opponent,
and the strategy goes deep enough that a mathematician wrote a book on it.

## How it works

The same foundation as chapter 3 — captive portal, rooms with four-letter codes, a token per
seat, the board as the only referee — so a phone only ever says *"I drew line 17"*. Everything
else is decided on the chip.

### Numbering the lines

A board of *n×n* boxes has *(n+1)×(n+1)* dots and **2·n·(n+1)** lines — 40, 84 or 180. The board
numbers them in one list: every horizontal line, row by row, then every vertical line, row by
row. Box *(r, c)* is bounded by:

```
top    = h(r,   c)        =  r·n + c
bottom = h(r+1, c)
left   = v(r,   c)        =  (n+1)·n  +  r·(n+1) + c
right  = v(r,   c+1)
```

So the whole board is two small arrays per room: who drew each line (180 bytes at most) and
who owns each box (81). A room, with four seats and names, is 384 bytes; six rooms are under
2.5 KB of the board's 320 KB.

### A move

When a line comes in, the board checks it is your turn and the line is free, draws it, and then
looks at **only the one or two boxes that line borders** — a line on the edge of the board
borders one box, any other line borders two. Each of those that now has four sides, and was not
already someone's, becomes yours.

Then the rule:

- **closed at least one box →** your turn again;
- **closed nothing →** the turn passes to the next player still at the table.

One line can close **two boxes at once** — two three-sided boxes sharing the missing side — and
scores two.

When the last line is drawn the game is over: the most boxes wins, and a tie for the most is a
draw, not a shared win.

## The thing worth understanding

**One rule — "close a box, go again" — turns a children's game into a strategy game.**

Early on, nobody wants to draw a box's third side, because the next player simply closes it.
So the board fills with lines that are safe — until there are none left. Then the board has
broken into **chains**: corridors of boxes where drawing any one line lets the next player
close the whole corridor, box after box, going again each time.

The obvious play when you are handed a chain is to take all of it. The strong play is often
to take all of it **except the last two**, and finish your turn with the line that gives those
two away — leaving your opponent two boxes and then the obligation to open the *next* chain
for you. Two boxes sacrificed to keep control of everything after.

This is the *long chain rule* and the *double-dealing move*, worked out in full by Elwyn
Berlekamp. The board knows none of it — it only enforces "close a box, go again" — and all of
it follows from that.

> **What you learn:** how much strategy one simple rule can create, and how a referee that
> enforces only the rules still produces the whole game.

## Turns round a table

With up to four players, "the other player" becomes "the next player who is still here":

- The turn passes clockwise by seat, **skipping empty seats**.
- **A player who leaves** — or whose phone goes quiet for 45 seconds — loses their seat, but
  **their boxes stay theirs** on the board. If it was their turn, it passes on.
- **If only one player is left**, the game ends and they have won it.
- **The host** is whoever holds the lowest seat. If the creator leaves before the start, the
  next player in becomes the host.
- **Nobody joins mid-game.** A room that is playing is not in the open list, and a code is
  refused until the game ends. Between games, new players can join.
- **Play again** starts a new game with whoever is seated, and the first move rotates one seat
  round the table each time.

## The clock

Each move has **thirty seconds**, kept by the board. When it runs out, the board draws a
random free line for whoever is up.

Late in the game a random line almost always hands the next player a chain, so letting the
clock run out is never a strategy — which is what a clock is for. The check runs from `loop()`
every 200 ms as well as on every request, so it fires on time even if every phone has gone
quiet. If the random line happens to close a box, the same player is up again with a fresh
thirty seconds: the rule is the rule, whoever drew the line.

## What the phone shows

- The four seats, each in its colour — mint, pink, amber, violet — with boxes this game and a
  star for games won. The player to move is ringed in their colour, with the clock below.
- The board, drawn as SVG from the same numbering the board uses. Free lines are faint dotted
  guides with a wide invisible tap target, so a thumb does not have to hit a thin line.
- **The last move stays marked** until the next one: a wide band in the mover's colour around a
  white core, and a white outline on any box it just won. A new move pulses twice to catch the
  eye, then holds still. It took three tries on real phones:
  - first, the line **blinked three times and stopped** — easy to miss if you looked away, and
    invisible on any phone with *reduce motion* switched on, because the shared stylesheet turns
    every animation off there;
  - then a mark that **stayed, and pulsed forever** — impossible to miss, and too much to look at
    for a whole game;
  - now it **stays, and is still** — the mark itself carries the information, so it works with
    animations off, and the two pulses are only there to say *this just changed*.

### One screen, no scrolling

A game you have to scroll to see is a game where you miss your turn. So the game screen is a
fixed column exactly the height of the screen: a slim bar with the room code, the players, a
line with the status and the clock — and the board takes **all the space that is left**, scaled
to fit whichever way is tighter. The page header is hidden during a game.

- On a **phone**, the width is what runs out, so the board is as wide as the screen and the
  height left over goes to the players: three or four players get two chips to a row, so names
  stay readable instead of being cut to an initial.
- On a **tablet**, the board grows until the height runs out.
- **Lines, dots and tap targets scale with the board size.** A 9×9 board on a phone gives each
  box about 36 pixels; the lines are drawn thicker and the invisible tap target around each one
  wider, so the smallest board on the smallest screen is still played with a thumb.

This was checked by rendering the real page in Chrome at phone (390×844 and 360×740) and tablet
(820×1180) sizes, with 4×4, 6×6 and 9×9 games of two to four players in progress.
- A line of status: *Your turn*, *You closed a box — go again!*, *Asha closed two boxes and
  goes again*, and *Time ran out — the board drew a line for Ravi*.

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the page; anything else redirects here |
| `/rooms` | GET | — | rooms that can still be joined, with size and player count |
| `/create` | POST | `n`, `s` | new room with a side of `s` = 4, 6 or 9 boxes → `{code, token, seat}` |
| `/join` | POST | `c`, `n`, `t` | take a free seat; with a known token, the same seat back |
| `/state` | GET | `c`, `t` | the room as this seat sees it — and the heartbeat |
| `/start` | POST | `c`, `t` | the host starts, with at least two seated |
| `/move` | POST | `c`, `t`, `e` | draw line `e` |
| `/again` | POST | `c`, `t` | after a game, start the next one |
| `/leave` | POST | `c`, `t` | give up the seat |

Lines and boxes travel as strings, one character each — `.` for nobody's, or the seat number
that drew or owns it — so even a 9×9 room is about 530 bytes of JSON.

## Limits

- **Eight phones.** The DHCP ceiling again: two four-player games, or four two-player ones.
- **No spectators.** A full or playing room turns a fifth phone away.
- **9×9 at most.** 180 lines fit the arrays as sized. Bigger boards are a constant away, but
  past 9×9 a box on a phone gets smaller than a fingertip.
- **A 9×9 game is long** — 180 lines. At thirty seconds a move it cannot stall, but it can take
  a while; that is what 4×4 is for.
- **A restart forgets every game.** Rooms live in RAM.

## Where it could go next

1. **Play the board.** A solo seat against the chip. A good dots and boxes player is mostly
   "never draw a third side, and count the chains at the end" — a small rule set the board can
   run between moves.
2. **Spectators**, for a projector at the front of a room.
3. **Teams.** Four players as two pairs, sharing a colour and a score.
4. **Show the chains.** When no safe lines are left, outline each chain and its length, so the
   strategy above can be seen rather than explained.

## Running it

Open `src/boxes/boxes.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash. The default
partition is fine. `ui.h` must sit beside the `.ino`.

1. One phone joins the WiFi **devkit-boxes**, opens `http://192.168.4.1`, enters a name, picks
   4×4, 6×6 or 9×9 and **creates a room**.
2. Up to three more join the same WiFi and tap the room in the list, or type its code.
3. The host taps **start the game**. The host moves first.

Serial at 115200 logs phones joining and leaving, every room created and joined, every move
(and whether it closed a box), and every time the clock ran out.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module on the default partition, with
no warnings from the sketch: **961,672 bytes — 73% of 1.31 MB**, and global variables at **15%
of RAM**.

On a laptop, every endpoint was driven through a stand-in web server:

- **Geometry** — on all three sizes, every box has four distinct lines and every line borders
  exactly the boxes that list it; and the page's drawing was checked against the board's
  numbering for all 532 box sides.
- **Rules** — a closed box going to the right player with an extra turn, one line closing two
  boxes and scoring two, a taken line and an out-of-turn move refused.
- **Five hundred random games**, every size with two, three and four players, played to the
  last line: every box owned and complete at the end, scores adding up to the board, and the
  turn staying with the mover exactly when a box closed. Eighteen ended level.
- **The table** — start needing the host and two players, no joining mid-game but joining
  between games, a reload landing back in the same seat, a leaver skipped with their boxes
  kept, the last player standing winning, *play again* rotating the opener, and the clock
  drawing for the right player at thirty seconds.

**Confirmed on hardware:** games on a phone and a tablet over the board's WiFi — creating and
joining rooms, the 4×4, 6×6 and 9×9 boards, the one-screen layout with no scrolling, closing
boxes and moving again, and the last-move mark.

**Not yet confirmed on hardware:** four players at once (so far two devices), and the clock
running out on a live game.

## Sources

- E. Berlekamp, *The Dots and Boxes Game: Sophisticated Child's Play*, A K Peters, 2000 — chains,
  the long chain rule and the double-dealing move.
- The ESP32 Arduino core's bundled `WebServer`, `DNSServer` and `esp_random()`.
- Chapter 3 of this book, for rooms, seats, the redirect and the clock.
