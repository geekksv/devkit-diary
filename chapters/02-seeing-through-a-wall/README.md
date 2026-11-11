# 2 · Seeing through a wall with WiFi

> **Needs:** one ESP32 DevKit V1, and a WiFi router that already exists in the room.
> **Status:** in progress — written before building, as every chapter here is.

---

## What this does

The board tells you when someone moves in a room it cannot see into, using no sensor of any
kind.

Not a motion sensor. Not a camera. Not an infrared detector. The board has nothing attached to
it, and it still knows.

## Why that works

Your router is already flooding the room with radio. That signal does not travel in one clean
line to the board — it bounces off walls, furniture, the floor, and arrives as a sum of
dozens of paths that have each taken a different route and a slightly different amount of
time.

A body is mostly water, and water absorbs and reflects at these frequencies. When someone
moves through the room, every one of those paths changes a little. The sum arriving at the
board changes with them.

What makes this usable is that WiFi does not transmit on a single frequency. The channel is
divided into many narrow subcarriers, and the radio already measures what happened to each one
separately, because it needs that to decode anything at all. That per-subcarrier measurement
is called Channel State Information, and it is far richer than the single signal-strength
number most projects use.

Signal strength tells you *how much* arrived. CSI tells you *how the room shaped it*.

So the chip is already computing a description of the room, thousands of times a second,
purely as a side effect of staying connected. This chapter does nothing more than ask for it.

## What it will not do

**It will not identify anyone.** It detects that the room changed, not who changed it. A cat
and a person both register; so does a door swinging.

**It needs tuning.** The threshold separating "someone moved" from "the radio is noisy" is not
universal. It depends on the room, the distance to the router, and where the board is pointing.
Espressif's own reference implementation says as much.

**Antenna position matters more than it should.** The module's antenna is directional in ways
that are easy to forget because nothing about the board looks directional. Moving it twenty
centimetres can change the result more than moving the person being detected.

These are the three reasons this one is taking the longest, and the reason it is not the
chapter that got finished first.

## The idea being tested

> A single board, with no sensor, can reliably distinguish an empty room from an occupied one
> — and the distinction survives being moved to a different room without rewriting anything.

The second half is the real test. Getting it working once, in one position, is a demo. Getting
it to survive being carried into a different room is the thing worth writing down.

---

## Build log

See **[notes.md](notes.md)** — the real log, written while building, including what failed.

## Sources

- [ESP-CSI — Espressif's own framework](https://github.com/espressif/esp-csi)
- [esp-radar component, including the note that the default movement threshold often needs changing](https://components.espressif.com/components/espressif/esp-radar)
- [ESP-WIFI-CSI detects humans with WiFi signals only, no sensor needed — cnx-software](https://www.cnx-software.com/2022/08/08/esp-wifi-csi-detects-humans-with-wifi-signals-only-no-sensor-needed/)
- [ESPectre, a port of the same idea to another board](https://learn.adafruit.com/espectre-human-detector-for-feather)
