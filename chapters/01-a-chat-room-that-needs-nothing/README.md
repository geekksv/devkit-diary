# 1 · A chat room that needs nothing

> **Needs:** one ESP32 DevKit V1. Nothing else — not even a router.
> **Status:** sketch complete, page logic tested off-hardware, first flash pending.

The board becomes the WiFi network. Phones join it and type to each other. No router, no
internet, no account — and nothing said in the room is written anywhere, so pulling the power
ends the conversation.

Joining the network opens the chat by itself, without anyone being told an address.

---

## How it works

**`WIFI_AP` mode** makes the board the access point rather than a client. **`WebServer`** serves
the chat as a web page, so there is nothing to install on any phone. And a **`DNSServer`
answering every lookup with `192.168.4.1`** catches the captive-portal probe each OS sends on
joining an unfamiliar network — so the phone decides it is behind a login page and opens the
chat unprompted.

## Identity

The first version sent a name with every message. That let anyone rename themselves
mid-conversation, or type someone else's name and speak as them — the design trusted the sender
to say who it was.

Identity now lives on the board. You pick a name once; the board checks it is unused, stores it,
and returns a random 32-bit token from `esp_random()`. **Every message carries the token and
never a name**, and the board stamps the name from its own table. No request a client can send
posts under another name, because the protocol has no field for one.

A valid token also short-circuits `/join`, returning the name you already hold — so a hand-made
rename request bounces, not just the UI. The token lives in `localStorage`, so a reload keeps
you in; a reboot invalidates it and bounces the page back to the join screen.

## Capacity

| | Value | Set by |
|---|---|---|
| Phones at once | **8** | `MAX_STATIONS` → `softAP()` |
| *Default if not passed* | *4* | 5th positional arg of `WiFi.softAP()` |
| DHCP leases | 8 | `CONFIG_LWIP_DHCPS_MAX_STATION_NUM` |
| Names held | 24 | `MAX_USERS` |
| Messages kept | 40 | `MAX_MSGS` ring buffer |
| Name / message length | 15 / 140 | `NAME_LEN`, `MAX_LEN` |
| Name released after | 120 s idle | `IDLE_MS` |

Two notes on that table:

**`WiFi.softAP(ssid, pass)` caps the room at four phones.** `max_connection` is the fifth
positional argument, it defaults to 4, and nothing warns you — the fifth person simply cannot
join. This sketch passes 8.

**Eight is the ceiling worth asking for.** The core ships with a DHCP pool of 8, so a ninth
phone associates and then waits for an address that never arrives — a network that looks
connected and broken, which is worse than being told the room is full.

The roster is deliberately larger than the radio limit: a name is held for `IDLE_MS` after its
phone stops polling, so closing a browser briefly does not hand your name to someone else.

## Range

ESP32-WROOM-32, PCB trace antenna, +20 dBm maximum transmit power (adjustable via
`WiFi.setTxPower()`). What that gives you depends on the room, so the page measures it instead:
`esp_wifi_ap_get_sta_list()` reports each associated phone's RSSI, shown as one bar per radio,
weakest called out.

| RSSI | |
|---|---|
| better than −55 dBm | strong — same room |
| −55 to −67 | good — through a wall |
| −67 to −78 | weak but working |
| below −78 | about to drop |

Worth knowing: a phone at the edge does not disconnect cleanly. It stays associated, polling
times out, and the page reports *lost the board* while the WiFi icon still shows connected.

⚠️ The bars are radios, not people. An HTTP request carries no MAC address, so labelling them
with names would be guesswork.

## Endpoints

| | Method | Takes | Returns |
|---|---|---|---|
| `/` | GET | — | the page — also served for any unknown path, so every portal probe works |
| `/join` | POST | `n`, optional `t` | `{ok, token, name}` or `{ok:false, err}` — `taken` · `empty` · `full` |
| `/msgs` | GET | `since`, `t` | messages after `since`, roster, signals; also the liveness ping |
| `/say` | POST | `t`, `x` | `{ok}` — no name parameter exists |
| `/leave` | POST | `t` | `{ok}` — frees the name immediately |

Each phone polls `/msgs` every 900 ms. `since` is a monotonic counter, so a response carries
only unseen messages — usually none. `WebServer` services one request at a time, which is the
limit that gives out first if the station count is raised.

## Limits

- **Not private between people in the room** — everyone on the network reads everything. It is
  private from outside the room, which is a different claim.
- **No authentication.** The token prevents impersonating a *present* name, not strangers
  joining. Set `AP_PASS` to 8+ characters if that matters.
- **Forgets everything on reboot.** 40 messages in RAM, no flash, no history.

## Running it

Open `src/local-chat/local-chat.ino` in the Arduino IDE, choose **ESP32 Dev Module**, flash.
`ui.h` must sit beside the `.ino`. Serial at 115200 prints the network name and address; join
**`devkit-chat`** and the page should open by itself, or visit `http://192.168.4.1`.

## Verified, and not

The capacity and DHCP limits, the `wifi_sta_info_t.rssi` field, and the fact that
`esp_random()` needs `<esp_random.h>` explicitly — it is not reachable through `Arduino.h` on
core 3.x — were read from the installed core (3.3.11). The page's logic is tested against a stub
board. It has not been flashed: the captive portal popping on current Android and iOS, and the
point at which polling stutters, are both still expectations.

## Sources

- [ESP32 Wi-Fi API guide — SoftAP and `max_connection`](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html)
- [`esp_wifi_ap_get_sta_list()` and `wifi_sta_info_t`](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html)
- [ESP32-WROOM-32 datasheet — antenna, +20 dBm](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf)
