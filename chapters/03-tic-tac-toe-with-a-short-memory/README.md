# 3 · Tic-tac-toe with a short memory

> **Needs:** one ESP32 DevKit V1. Two phones per game — up to eight phones, four games at once.
> **Status:** flashed and played. A phone and a tablet create and join rooms over the board's
> WiFi. The first flash found two bugs — a page that could not reach the board, and a board
> that froze on "create a room" — both written up below. The move clock is newer than that
> test and still to be confirmed on hardware.

The board makes its own WiFi network and serves a game of tic-tac-toe to every phone that
joins. One phone creates a room and gets a four-letter code; a second phone types the code, or
taps the room in the list of games waiting for an opponent. Then they play — on two screens,
refereed by a chip with no internet connection and nothing attached to it.

There are two games in the box. **Classic** is the one everybody knows. **Vanishing** gives
each player only three marks: place a fourth and your oldest one is wiped off the board. There
are no draws, and every move is on a fifteen-second clock. It sounds like a small change. It is
a different game.

---

## What makes it different from the other chapters

Chapter 1's chat room is a shared log: everyone appends, everyone reads, and the order is the
only rule. Nobody can make an *illegal* message.

A game is the opposite. Almost every request a phone can send is wrong most of the time — not
your turn, cell taken, game over, not even in this room. So this is the first chapter where the
board stops being a noticeboard and becomes a **referee**: it holds the only real copy of every
board, and its answer to a move is often *no*.

It is also the first chapter with **rooms** — several independent sessions running side by
side on one server, each with its own state, its own players and its own clock.

## How it works

The same captive-portal trio as chapter 1 — `WIFI_AP`, `WebServer`, `DNSServer` — so a phone
that joins `devkit-xo` usually opens the page by itself.

Behind the page, eight `Room` slots in a plain array. Each one holds:

| Field | What it is |
|---|---|
| `code` | four letters, from an alphabet with no `I` and no `O` |
| `tok[2]` | a random token per seat — seat 0 plays X, seat 1 plays O |
| `cell[9]` | the board: empty, X or O |
| `at[9]` | the move number each mark was placed on |
| `ply`, `starter` | moves made this round, and who opened it — together they *are* the turn |
| `result`, `line` | playing / winner / draw, and which of the eight lines won |
| `turnAt`, `timedOut` | when the current turn began, and who last ran out of time |
| `wins`, `draws` | the running score for as long as both stay |
| `ver` | bumps on every change |

A room is about 100 bytes. All eight together are less than a kilobyte — nothing in this
chapter comes near the memory limit, which is worth noticing after chapter 2 ran out of flash.

## The thing worth understanding

**The phones are not trusted, and do not need to be.**

A phone never says "I placed an X on cell 4". It says "the holder of token `9f3a…` tapped cell
4 in room `KQTX`", and the board works out everything else: which seat that token holds, whether
that seat's turn it is, whether the cell is empty, whether the game is still running. Only then
does the board change, and the reply is the new board as the board sees it.

So the page can be edited, the request can be forged, the phone can be someone else's — none of
it lets you move for your opponent, move twice, or overwrite a mark, because none of those
decisions are made on the phone. The turn is not even stored: it is `(starter + ply) % 2`,
computed fresh from moves the board itself accepted.

This is how every online game that cares about cheating is built — an *authoritative server*
with thin clients — and it is the same idea as chapter 1's token, one step further. There the
token stopped you posting under someone else's name. Here it stops you playing someone else's
turn.

> **What you learn:** where the rules have to live in a multiplayer system, and why it is never
> on the client.

## The vanishing variant

Three rules, and the board enforces all of them:

- **Three marks each.** A player can have at most three marks on the board at any moment.
- **Your fourth wipes your first.** Placing a fourth mark removes your oldest one in the same
  move. Every move after that removes your oldest remaining mark, so the count never goes
  above three.
- **No draws.** Six marks at most on nine cells: the board never fills, so it cannot end level.
  Play goes on until someone lines up three.

The board does not keep a queue for this. Every mark records the move number it was placed on
(`at[]`), and the mark that goes is simply the one with the smallest number among the mover's
marks — once they have three. That is the whole variant: one comparison, in `doomed()`.

**The order inside one move matters**, and it is the one decision in the rules worth arguing
about:

1. the mover's oldest mark is removed,
2. the new mark lands,
3. the board checks for three in a row.

So a line that *needed* the vanished mark does not count. You cannot win with a mark you no
longer have, and — because the removed mark is still on the board while you choose — you
cannot place onto the cell you are about to lose.

### Showing what is about to go

The game is only fair if nobody is surprised by it, so the page marks the mark that will vanish:

- **The mover's** next-to-go mark — the one that disappears the moment they tap — gets a
  pulsing red dashed outline and a small **next** tag, and the line under the board says it in
  words: *your X in the top-left vanishes when you move*.
- **The other player's** next-to-go mark is faded, without the alarm. It matters one move
  later, and you can plan for the hole it will leave.

Both phones show both, because watching your opponent's mark about to disappear is half the
strategy.

### The clock

No draws has a cost: two careful players can circle forever. So each move in vanishing has a
**fifteen-second clock**, counted by the board, not the phone.

- The clock starts when the second player sits down, and restarts after every move.
- `/state` carries the milliseconds left. The page counts down on its own between polls so the
  bar moves smoothly, and resynchronises every 0.7 s — the board's number is the real one.
- When it reaches zero, the board **plays a random free cell for whoever was too slow**, and
  both phones say so. A random move rather than a lost round, because the usual reason is a
  phone that dimmed its screen for a moment, and losing a game to that is harsher than a stall
  deserves. A random move in this variant is usually a bad one — that is punishment enough.
- The check runs in `loop()` every 200 ms as well as on every request, so the clock runs out on
  time even if both phones have stopped asking.

Classic has no clock. Nine moves at most, and it ends on its own.

## Rooms, seats, and leaving

- **Create** takes a name and a variant, finds a free slot, picks a code no other room is
  using, and seats you as X.
- **Join** takes a code and a name and puts you in the empty seat. The lobby also lists every
  room with one player waiting, so in a room full of people nobody has to read a code aloud.
- **Every poll is a heartbeat.** The page asks for the room's state every 700 ms, and that
  request is also what keeps your seat. A seat whose phone has been silent for **45 seconds**
  is released.
- **A seat emptying ends the round.** If your opponent leaves or their phone dies, the board is
  cleared and the score reset — a half-played game against whoever sits down next is not the
  same game. The room goes back to *waiting* and reappears in the lobby list.
- **A reload is not leaving.** The page keeps your code and token on the phone, and on reload
  asks the board for the same seat back. Locking the phone for half a minute costs nothing.
- **The loser of the toss does not lose it forever.** X opens the first round; after that the
  opener alternates with every *play again*.

## Polling, honestly

There is no push here. Both phones ask, every 700 ms, "what does the room look like now?", and
the board answers with the whole room — about 300 bytes of JSON. With eight phones that is
roughly eleven requests a second, which a single-threaded `WebServer` handles without strain;
chapter 1 runs a similar rate.

The cost is latency: your opponent sees your move up to 0.7 s after you made it. For
tic-tac-toe that is fine — nobody plays it fast enough to notice. For anything twitchier it
would not be, and that is the case for WebSockets, which the core does not ship a server for.
(The mover sees their own mark immediately: the reply to a move *is* the new state.)

## Endpoints

| | Method | Takes | Does |
|---|---|---|---|
| `/` | GET | — | the page; also served for any unknown path, so portal probes work |
| `/rooms` | GET | — | rooms with one player waiting, plus free slots and connected phones |
| `/create` | POST | `n`, `m` | new room, variant `0` classic / `1` vanishing → `{code, token, seat}` |
| `/join` | POST | `c`, `n`, `t` | take the empty seat; with a known token, hand the same seat back |
| `/state` | GET | `c`, `t` | the room as this seat sees it, with the clock — and the heartbeat |
| `/move` | POST | `c`, `t`, `i` | play cell `i` (0–8), answer with the new state or a reason |
| `/again` | POST | `c`, `t` | once a round is over, start the next one |
| `/leave` | POST | `c`, `t` | give up the seat |

A refused move answers `{ok:false, err}` with one of `alone`, `over`, `turn`, `cell`, `taken`.
The page never shows these — its buttons are already disabled whenever a move would be refused
— but a hand-made request gets told why.

## Limits

- **Eight phones.** The DHCP ceiling from chapter 1 again: four games at once, or fewer games
  and a couple of people watching the lobby.
- **No spectators.** A third phone trying a full room is turned away. Watching is a small
  change — a read-only seat — and is listed below as the first improvement.
- **No clock in classic.** Only vanishing has one, so a player can sit on a classic move for as
  long as they like. The 45-second seat timeout only catches a phone that has *gone*.
- **A restart forgets every game.** Rooms live in RAM. The page notices the board no longer
  knows its token and returns to the lobby with a message.
- **No encryption.** Anyone on `devkit-xo` can see the traffic. It is tic-tac-toe.

## Where it could go next

Roughly in order of effort:

1. **Spectators.** A third kind of seat that gets `/state` but cannot move. Put the board on a
   projector and run a tournament from it.
2. **Play the board.** A solo seat against the chip. Classic tic-tac-toe is small enough for a
   full minimax search on every move — 9! positions at most, far fewer in practice — so the
   board would never lose. Vanishing has cycles and no full board, so it needs a depth limit
   and a scoring function instead, which is the interesting version.
3. **Ultimate tic-tac-toe.** Nine boards in a grid, where your move decides which board your
   opponent must play in next. Same room code, same referee, 81 cells instead of 9.
4. **Bigger boards.** 4×4 or 5×5, four in a row. `LINES` becomes generated rather than written
   out.
5. **Push instead of poll.** A WebSocket server — not in the core, so it would break the
   book's *nothing to install* rule. Worth it only if latency starts to matter.
6. **Remember the champions.** A leaderboard by name in NVS (`Preferences`), so it survives a
   restart. Names are not accounts, so it is a bit of fun rather than a ranking.
7. **A second board as a bridge.** Two ESP32s linked by ESP-NOW, each hosting its own WiFi, so
   a game can be played between two rooms out of each other's WiFi range.

## Running it

Open `src/xo/xo.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash. The default
partition is fine. `ui.h` must sit beside the `.ino`.

1. Phone one joins the WiFi **devkit-xo**; the page should open by itself, or visit
   `http://192.168.4.1`. Enter a name, pick a variant, **create a room**.
2. Phone two joins the same WiFi, enters a name, and taps the room in the list — or types its
   code.
3. X moves first. After each round, either player can tap **play again**.

Serial at 115200 prints the address, the free heap and the size of one room, and then a line
for every phone that joins, gets an address or leaves, and every request that changes a game.
When something does not work, that log says which step never happened.

## What the first flash found

Every test on the laptop passed, and the board still failed twice the first time two real
devices touched it. Both are kept, because neither would have been found any other way.

### The page that lived at Google's address

The phone joined, the page appeared, and **create a room** answered *the board did not
answer*. The serial log showed the phone's connectivity probe — `GET /generate_204` — and
then nothing: the button's request never arrived.

The first version served the game *as the answer to that probe*, the way chapter 1 does. So the
browser believed the page lived at `connectivitycheck.gstatic.com`, and every request the page
made went to that name. The phone looked the name up through its own secure DNS — not the
board's catch-all — got Google's real address, and sent the request there, over a WiFi with no
internet. The board never saw it.

The fix is the standard captive-portal move: anything not asked of `192.168.4.1/` is answered
with a **redirect** to it. The page now lives at a bare IP address, and no name lookup is
involved anywhere.

> Chapter 1 has the same pattern and has worked on every phone it met — but it was never
> tested on one with Private DNS switched on. It probably has this bug too.

### The board that froze on "create a room"

With the redirect in, the log showed `POST /create … -> create requested` — and then
silence. No answer, no further polls, and a second device could not even load the page. The
board had not restarted (its clock kept counting), so something in the handler was looping.

It was the room code. `newCode()` keeps drawing codes until it finds one no room is using.
The first version marked the room *used* and then generated the code **straight into that
room's own `code` field** — so the uniqueness check found this very room holding this very
code, every time, and drew again forever. One loop on one core, and the whole web server stops.

The fix is to draw the code into a separate buffer before the slot is marked used. The laptop
tests had covered the rules but called the rules directly, never `handleCreate()`; they now
drive every endpoint through a stand-in web server, and creating two rooms in a row is the
first thing they check.

> **What you learn:** a test that skips the one function you did not suspect proves nothing
> about it — and an infinite loop on a microcontroller looks exactly like a network fault.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module on the default partition, with
no warnings from the sketch: **958,000 bytes — 73% of 1.31 MB** of program space, and global
variables at **14% of RAM**. Half of chapter 2's binary, because there is only one radio stack
linked in — the default partition is fine here.

Off the board, every endpoint was driven through a stand-in web server on a laptop: creating
rooms back to back, the open list, joining by code in either case, a third player turned away,
a reload landing back in the same seat, a won game and *play again*, all eight slots filling
and one freeing on *leave*, a vanished mark that would have completed the line (it does not
count), three hundred vanishing moves with never more than three marks each and no draw, and
the clock — nothing at 14.9 s, the board moving for the right player at 15.

**Confirmed on hardware:** a phone and a tablet on the board's WiFi, the redirect landing the
page on `192.168.4.1`, and rooms created and joined.

**Not yet confirmed on hardware:** the move clock and the *next* marker, which were added after
that test; the seat timeout; and the claim that eight phones polling at 700 ms is comfortable.

## Sources

- The ESP32 Arduino core's bundled `WebServer`, `DNSServer` and `esp_random()`.
- Chapter 1 of this book, for the DHCP ceiling and the token idea this chapter extends.
