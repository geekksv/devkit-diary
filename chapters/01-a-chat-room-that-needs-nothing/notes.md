# Build log — chapter 1

Dated entries, written while building. Failures stay in.

---

## 2026-11-11 — the first version worked and was wrong

Got a chat serving from the board's own AP in one sitting. AP mode, a `WebServer`, a 40-entry
ring buffer of messages, and a `DNSServer` answering every lookup with `192.168.4.1` so the
phone's captive-portal probe lands on the chat. That last part took one line and is the only
reason the thing feels finished — the page just appears when you join the network.

Then I put it in front of two phones and the design fell over.

The page had a name box next to the message box. Both went up with every message. Which means:

- the name could change between messages, so nobody had a stable identity
- you could type **someone else's name** and talk as them

Neither is a coding mistake. The code did what it was told. The design trusted the sender to
declare who they were, and a sender will say anything.

The tempting fix was to hide the name field after the first message. I nearly did it. It is
not a fix — it is the same trust with the evidence removed. `curl` can post whatever it likes
and the board would believe it.

### What replaced it

Identity moved to the board:

- a join screen, name picked once
- the board refuses a name someone present already has, case-insensitively
- the board returns a random 32-bit token from `esp_random()`
- `/say` takes the **token**, never a name, and the board stamps the name from its own table

There is now no request a client can send that posts under another name, because the protocol
has no field for one. The page is no longer load-bearing.

Three details that only showed up once it was built:

**Rejoining with a token you already hold** has to return the name you *have*, not the name you
*asked for*. My first attempt compared the two and fell through to allocating a second slot
when they differed — which meant a crafted request could effectively rename you and leak a
roster slot. Now a valid token short-circuits everything and the requested name is ignored.

**A reload must not cost you your name.** Token goes in `localStorage` and is re-validated on
load. Without that, every accidental refresh burns a name for two minutes.

**The board restarting has to be visible.** All tokens die with it, so the page now detects a
rejected token and bounces back to the join screen saying *"the board restarted, so names were
released"* — rather than silently failing to send.

Added a live roster, shown on the join screen too, so you can see which names are taken before
picking one.

### A bug that would not have compiled

`esp_random()` is **not** reachable through `Arduino.h` on arduino-esp32 3.x. It used to come
in via `esp_system.h`; it does not any more — the declaration lives in `esp_random.h` and the
core's own bundled examples include it explicitly. Found it by reading the installed headers
rather than by compiling, which is luck. Needs:

```cpp
#include <esp_random.h>
#include <strings.h>   // strcasecmp
```

Worth remembering generally: on this core, assume nothing from IDF is transitively included.

### How far this is actually verified

Being honest about what has and has not happened, because the whole point of these logs is
that they are not a press release:

- **Verified:** the page's JavaScript runs against a stub board and a stub DOM — join, name
  lock, duplicate refusal, reload, rename attempt, board restart, leave. 21 assertions, all
  passing. Every element id the script reaches resolves, and every endpoint it calls is one
  the firmware serves.
- **Verified by reading:** `esp_random` and `strcasecmp` includes, against the installed core
  (3.3.11). Braces and parens balance; the raw string literal is matched.
- **Not verified:** it has not been flashed. No compile, no hardware, no second phone.

So the honest status is *written, not yet proven*. Open questions for the first flash:

- Does the captive portal actually pop on this Android version, and on iOS? The probe URLs
  differ and I am answering all of them, but "should" is not "does".
- How many phones before 900 ms polling makes the board stutter? Guessing it is well under the
  AP's station limit, and that polling is what gives out first.
- `String` concatenation builds the JSON response. A client that has just joined pulls up to
  40 messages at once, which is a few kilobytes assembled in RAM on a board that has 520 KB
  and two WiFi stacks running. Fine in theory. Theory is why this section exists.
- Does a name actually get released after the 2-minute idle window, or does a phone that is
  asleep keep polling often enough to hold it forever?

Next: flash it, then put it in front of more than two phones and write down what breaks.
