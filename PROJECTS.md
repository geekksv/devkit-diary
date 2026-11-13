# Ideas

Things worth building on a bare ESP32 DevKit V1.

**A list to pick from, not a plan.** Nothing here is promised, the groupings are loose, and it
will grow and change as the work does.

No modules. No breadboard. No sensors, no wires, no shopping list. The only thing outside the
board that any of these touch is a WiFi router, and you already have one.

What happens in each one and why it is worth the evening. No code, no wiring and no library
names here on purpose.

---

## Part I · Things that should not be possible

### 1 · The board can hear electricity

Leave a pin connected to nothing at all and read it anyway. The number does not sit still — it
oscillates, fifty times a second, in step with the mains wiring buried in the walls around you.
The pin is acting as a tiny antenna and the building is humming into it.

A chapter about noise, and about the fact that "connected to nothing" is not the same as
"reading nothing".

> **What you learn:** why a floating input is a bug, and what it is picking up when it is one.

### 2 · A theremin with no instrument

Capacitive touch does not actually require touch. Your hand *approaching* a pin changes the
reading before contact, and the change is proportional to distance. Feed that into the chip's
analog output and you have pitch controlled by a hand moving through the air.

An instrument made of nothing, and the shortest path to showing someone that this board is
stranger than it looks.

> **What you learn:** that a touch sensor is a distance sensor you are using at zero range.

### 3 · Finding things you lost

Nearly everything with a battery is shouting over Bluetooth — earbuds, watches, speakers, your
own phone. The board listens, lists what is nearby, and reports how strong each one is. Walk
around and the number climbs as you get closer. Hot and cold, for objects.

The first chapter that earns its keep in the first week.

> **What you learn:** what is actually broadcasting around you, which is more than you think.

### 4 · Knowing where you are without GPS

The set of WiFi networks visible from your sofa is very nearly unique to your sofa. The board
collects that set and hands it to a lookup service, which answers with coordinates. Position
without a satellite, without a GPS chip, indoors, in seconds.

> **What you learn:** how a phone knows where it is when it cannot see the sky.

### 5 · The computer that runs while asleep

There is a second, much smaller processor inside this chip that keeps running after both main
cores have been powered down. It has its own instruction set and its own memory, and it exists
so a battery device can watch for something interesting while drawing almost nothing.

Programming it feels like finding a room in your house you had not noticed.

> **What you learn:** that "asleep" is a range of states, not a switch.

### 6 · Watching a clock go wrong

Set the board's time from the network, then leave it alone for a week and ask again. It will be
wrong. Not broken — drifted, by an amount you can measure and, with effort, predict, because
the crystal keeping time is sensitive to temperature and to being slightly imperfect.

> **What you learn:** why every networked device re-checks the time, and what it costs not to.

---

## Part II · Things you will actually keep using

### 7 · A remote for the whole machine

The board pairs with a laptop as a Bluetooth keyboard — not a thing pretending to be one, an
actual keyboard as far as the laptop is concerned. Then it serves a page of buttons to your
phone over its own WiFi, so a tap on the phone becomes a keystroke on the laptop: slides, the
media keys, whole typed sentences, a new browser tab opening on a site you pick.

The first tool in the book worth carrying to a room you are teaching in — and the only sketch
that holds both radios open at once.

> **What you learn:** how a device declares what kind of device it is, and that nothing
> downstream verifies the claim.
>
> **Written up as [chapter 2](chapters/02-a-remote-for-the-whole-machine/).**

### 8 · Proof the internet went down

It sits in a corner, tests the connection on a schedule, and writes down every time the line
dies and for how long. After a month you have a record. The next time the provider says there
was no problem, there is something to put in front of them.

Unglamorous, and the one most likely to still be plugged in a year from now.

> **What you learn:** how to measure a thing that only matters when you are not watching.

### 9 · A quiz for a room with no internet

The board makes its own network and serves a live quiz on it. Sixty phones join — no signal
needed, no data used, no accounts, no app. Questions appear, answers come back, scores update
on the projector.

Built specifically for workshops where the venue WiFi cannot be relied on, which is most of
them.

> **What you learn:** that a server does not need the internet, and sixty clients is not many.

### 10 · A chat room that needs nothing

The same foundation turned to the simplest possible use: everyone within range joins the
board's network and types to each other. No router, no internet, no account, no company in the
middle. Nothing said in the room leaves the room.

Useful where there is no signal. Interesting everywhere else.

> **What you learn:** how little infrastructure a working chat system actually requires.
>
> **Written up as [chapter 1](chapters/01-a-chat-room-that-needs-nothing/).**

### 11 · Phones as controllers

The board serves a game to a big screen and hands every phone in the room a controller — just a
web page, nothing to install. Four people playing something hosted entirely on a chip the size
of a stick of gum.

> **What you learn:** how to keep several clients in sync in real time without a backend.

### 12 · Drop a file, grab it on the other device

A file server living in the board's own flash memory. Drag something in from a laptop, pull it
down on a phone. No cable, no cloud, no account, no thirty-second upload to a server in another
country so it can come back to a device two feet away.

> **What you learn:** that the chip has a filesystem, and what it is like to work in megabytes.

### 13 · Extending the WiFi that does not reach

The board joins the weak network at the edge of its range, then broadcasts its own network and
passes traffic between the two. A real repeater, for the room the router never quite reaches.

The hardest chapter here by some distance — the plumbing for it sits well below the friendly
layer everything else uses. Worth the fight.

> **What you learn:** what a router actually does, by having to do it.

### 14 · The last time you need the cable

Up to now every change has meant plugging it in. This chapter teaches the board to accept new
firmware over the air from a browser. After it, the board can be sealed in a box on a shelf and
still be updated.

> **What you learn:** how a device replaces its own software without bricking itself.

---

## Part III · Measuring things you cannot see

### 15 · Which channel is actually free

A survey of every channel and what is sitting on each one. Routers pick their channel
automatically and often pick badly, because they decide once, at boot, in an empty flat, and
never reconsider.

> **What you learn:** why your WiFi is slow at 8pm and fine at 3am.

### 16 · Walking the signal

Carry the board around the building, logging strength as you go. The dead spot behind the
kitchen stops being an argument and becomes a number.

> **What you learn:** how radio actually behaves around walls, water and metal — badly, and not
> symmetrically.

### 17 · A census of the air

Leave it running for twenty-four hours recording every network that appears and disappears.
Neighbours' routers have schedules. Phones hotspot at the same time each morning. The air above
a building has a daily rhythm and you can draw it.

> **What you learn:** that the spectrum around you is busy, shared, and surprisingly legible.

### 18 · What sleep really costs

Measure the current the board draws when it is working, when the radio is idling, when it is
lightly asleep and when it is deeply asleep. Then turn those four numbers into the only
question that matters: how long would this run on a battery?

> **What you learn:** the arithmetic behind every battery-life claim you have ever read.

### 19 · Two cores, honestly

This chip has two processors. Run identical work on one, then split it across both, and time
it. Sometimes it halves. Sometimes it changes nothing. Sometimes it is slower, and
understanding why is the chapter.

> **What you learn:** where parallelism helps and where it is just more complicated.

### 20 · Is it actually random?

The chip has a hardware random number generator. Datasheets say it is good. This chapter takes
several million numbers out of it and runs them through the tests statisticians actually use,
rather than trusting the claim.

> **What you learn:** how to test a thing you are told to take on faith — and that the answer
> depends on whether the radio is switched on.

---

## Part IV · Dead ends, kept on purpose

### 21 · The sensor Espressif deleted

There is a magnetic field sensor inside this chip. A hundred tutorials show you how to use it.
It is gone — removed from the Arduino support in version 3, and struck from the official
documentation entirely in a 2022 product change notice, because it never worked properly.

So the tutorials are still there, confidently teaching a thing the manufacturer has withdrawn.

> **What you learn:** to check the datasheet against the blog post, and which one wins.

### 22 · The thermometer that lies

There is also a temperature sensor. It reads the temperature of the silicon, which is warmer
than the room by an amount that depends on what the chip has been doing. It is not an ambient
thermometer and cannot be turned into one.

> **What you learn:** the difference between a reading and a measurement.

### 23 · Brownout, watchdog, reboot

Three ways this board will restart without being asked: the supply sagged, something stopped
responding and was killed, or the code crashed. Each leaves a different trace in the log, and
knowing which you are looking at is the difference between a fix and a week of guessing.

> **What you learn:** how to read a crash, which is the most useful skill in the book.

---

## Notes on the list

**Left off.** Blinking the onboard LED in morse code, a counter that survives reboots,
generating a waveform. All real, none of them interesting enough.

**What broke the rule.** Streaming audio to the board over Bluetooth works, but you need a
speaker to hear it. One board and nothing else is the whole premise, so it is out.

**Two that are harder than they read.** The repeater needs networking machinery well below the
layer everything else uses. The geolocation one depends on an outside service, and the free one
most people used has shut down.

A list that only contains the easy things is a different and much less useful book.
