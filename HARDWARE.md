# The board

Everything in this book runs on one ESP32 DevKit V1 — the common thirty-pin board carrying an
ESP32-WROOM-32 module, a USB-to-serial chip, a regulator and two buttons. Nothing is attached
to it at any point.

This page is the reference the chapters lean on: what is actually inside, and which of it is
usable.

---

## What is in the module

| | |
|---|---|
| Processor | Two 32-bit cores, up to 240 MHz, running FreeRTOS |
| Plus | A third, much smaller low-power coprocessor that runs while the main cores sleep |
| Memory | 520 KB on-chip SRAM, 4 MB flash on a typical board |
| Radio | WiFi 802.11 b/g/n, and Bluetooth — both Classic and Low Energy |
| Analog in | 18 channels of 12-bit ADC |
| Analog out | Two 8-bit DACs, on GPIO 25 and 26 |
| Touch | Ten capacitive touch pins, which also work as wake sources |
| Timekeeping | An RTC that survives deep sleep, plus memory that survives with it |
| Other | Hardware random number generator, PWM on nearly any pin, onboard LED on GPIO 2 |

Two cores and a radio that does both WiFi and Bluetooth is a lot of machine for the price, and
most of this book is about reaching the parts of it that project tutorials skip.

---

## Things worth knowing before you start

**Not every pin is yours.** Some are strapping pins, read at boot to decide how the chip
starts; holding one the wrong way stops the board booting. Some are wired to the flash chip and
using them crashes it. The chapters say which pins they use and why.

**ADC2 stops working when WiFi is on.** The second analog block is shared with the radio. Half
the "my sensor reads zero" questions on the forums are this. Anything analog in this book
stays on ADC1.

**Deep sleep forgets everything** except what you deliberately put in RTC memory. A variable is
not a place to keep something across a sleep.

**The USB port may not be enough.** Transmitting over WiFi draws a sharp current spike. A thin
cable or a weak port causes brownout resets that look exactly like a software crash — see
chapter 24, which exists because of this.

**`A0` is not a pin number here.** Board labels, GPIO numbers and silkscreen do not always
agree. Go by GPIO number.

---

## Two things that do not work, whatever the tutorials say

### The hall sensor

The ESP32 has a magnetic field sensor on the die, and a great many tutorials show you how to
read it.

It is gone. The function was removed from the Arduino core in version 3.0, and Espressif struck
the sensor from the ESP32 documentation entirely in a product change notice dated December
2022, on the grounds that it does not work properly.

You can still reach it by pinning the board package to version 2.x. The honest answer is not
to build anything on it. Chapter 22 is about this.

- [arduino-esp32 2.x → 3.0 migration guide](https://randomnerdtutorials.com/esp32-migrating-version-2-to-3-arduino/)
- [PCN20221202 — Remove Hall Sensor from ESP32 Series of Documentation](https://documentation.espressif.com/en/PCN20221202%20Remove%20Hall%20Sensor%20from%20ESP32%20Series%20of%20Documentation.html)

### The internal thermometer

There is a temperature sensor, and it reads the temperature of the silicon — which is warmer
than the room by an amount that varies with what the chip has been doing for the last minute.
It is useful for noticing that the chip is overheating. It is not an ambient thermometer and no
amount of offsetting makes it into one. Chapter 23.

---

## Toolchain

Pinned here so the book can be read against the right versions later. A book that does not say
what it was written against rots quietly.

| | |
|---|---|
| Board | ESP32 DevKit V1 (ESP32-WROOM-32), 30-pin |
| Core | arduino-esp32 3.x — *fill in the exact version when chapter 1 is built* |
| Editor | *fill in* |
| Serial | 115200 baud unless a chapter says otherwise |

Chapters that need something outside the Arduino core say so in their first paragraph. At the
time of writing that is only chapter 14, which needs networking machinery that lives a layer
below.
