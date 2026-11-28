# 21 · Reaction

> **Needs:** one ESP32 DevKit V1. A phone for the scoreboard is nice, not needed.
> **Status:** written and tested on a laptop. The rules (35 checks), and the real page in a
> browser against a laptop build of the board's game. Not yet run on the board itself.

How fast are you? Press BOOT. After a random wait the blue LED lights up: press BOOT again as
fast as you can. The board times you to the millisecond, and **http://react.local** shows your
time, your last five, and a top ten that the board keeps through restarts.

## Playing

1. Open **http://react.local**, type your name and tap **Start**. Or just press **BOOT** on the
   board.
2. **Watch the board, not the screen.** The page follows a moment behind; the LED is the real
   signal.
3. When the blue LED lights, press **BOOT**.
4. Your time appears. Press BOOT again for the next round.

- **Too soon:** pressing before the light is a false start. The LED flickers and nothing counts.
- **A guess:** under 100 ms is faster than anyone can see and press, so it counts as a false start
  too.
- **Missed:** no press within 3 seconds.

Most people land between 200 and 300 ms. Under 200 is very good.

## The thing worth understanding

**Why the page can't do the timing.** A phone asks the board for news a few times a second, over
WiFi. Each answer is 20 to 100 ms late, and never by the same amount. If the page showed the
light and timed your tap, the WiFi would add more to your time than your own reaction does.
So the board lights its own LED and reads its own button, both in the same loop. That loop runs
thousands of times a second, so a press is seen within a millisecond.

**Debouncing without cheating.** A button's contacts bounce for a few milliseconds when pressed.
The board waits until the button has been down for 15 ms before it believes the press. But it
records the moment the button *first* went down, so the waiting never adds to anyone's time.

**A random wait you can't learn.** The light comes 1.5 to 5 seconds after the start, drawn from
the chip's hardware random number generator, so you can't count it out.

## Endpoints

| Path | What it does |
|---|---|
| `GET /state?v=N` | The round, your time, your last five, the top ten. `{"same":true}` if nothing changed since version N |
| `POST /start` | Starts a round (`name` optional) |
| `POST /name` | Sets the player's name |
| `POST /clear` | Clears the top ten (`sure=yes`) |

## Files

- `src/reaction/reaction.ino`: the LED, the button, WiFi and the web server.
- `src/reaction/reaction.h`: the rules, testable on a laptop.
- `src/reaction/reaction_page.h`: the page.
- `src/reaction/wifi_setup.h`, `improv.h`, `ui.h`: WiFi setup, shared with chapters 7 to 20.
- `tools/reaction_test.cpp`: the rules on a laptop:
  `c++ -std=c++17 -I../src/reaction reaction_test.cpp -o t && ./t`

## Running it

Flash **Reaction** from the web flasher, or compile `src/reaction` in the Arduino IDE (ESP32 Dev
Module). Give it your WiFi when asked. Without WiFi it makes its own network, `devkit-react`, and
the game on the board works with no phone at all.

## Verified, and not

**On a laptop:**
- The wait is always 1.5 to 5 s, and spread out.
- Times, false starts, guesses under 100 ms, misses after 3 s.
- The top ten stays sorted and capped, and equal times keep the earlier one ahead.
- Names are cleaned, and long Hindi names are cut without breaking a letter.
- The page in Chrome: start, the light, a press, the time in the top ten, a false start, and
  clearing.

**Not yet confirmed:** the game on the board itself.
