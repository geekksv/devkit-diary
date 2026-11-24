# 13 · One clipboard for every device

> **Needs:** one ESP32 DevKit V1 on your home WiFi, and your computers and phones.
> **Status:** written and tested on a laptop. The encryption matches byte for byte between the
> browser and Python; the board's rules, two real helpers, the page in a browser and this Mac's
> real clipboard were tested against a laptop build of the board (figures below). Not yet run on
> the board itself, or on Windows.

Copy on one computer, paste on another, with nothing to press. Copy on your phone, tap once,
and it's on your laptop. A history of everything you copied, pins for the things you paste every
day, a one-time item for a password or an OTP, and a quick picker on a hotkey.

**The board never sees your clipboard.** Every item is sealed on the device that copied it, with
a passphrase only your devices know. The board stores and passes on scrambled bytes.

## Using it

1. Flash **One clipboard** from the flasher and give it your WiFi on the same page.
2. **On each computer**, run one line (it fetches the helper from the board):
   - Mac or Linux: `curl -fsSL http://clip.local/i | sh`
   - Windows (PowerShell): `irm http://clip.local/i.ps1 | iex`

   The first computer chooses the passphrase. Every other device types it once.
3. **On phones**, open `https://clip.local` and join with the passphrase. The first time, the
   browser says the page is "not private", because the board made its own certificate (as in
   chapter 12).

Then:
- **Computers sync by themselves.** Copy on one, and a second later it's on the clipboard of the
  others, ready to paste.
- **Phones take one tap.** *Paste here* sends this phone's clipboard. *Copy* on any item puts it on
  the phone's clipboard. Phones can't watch the clipboard in the background: that's the browser's
  and the phone's rule, not ours.
- **The quick picker:** the history with search. Arrows and Enter paste the item into the app you
  were in.
  - **Windows:** **Win+Alt+V** (Win+V is Windows' own history, so it's left alone). If that's
    taken, the helper uses the next free one and says which.
  - **Mac and Linux:** give `devkit_clip.py --pick` a keyboard shortcut once, in Shortcuts or
    your desktop's settings. The installer prints the exact line.
- **iPhone:** a Shortcut with one action, *Open URLs* `https://clip.local/#paste`, on the Home
  Screen or Back Tap, opens the page ready to paste.

**On each item:**
- **Copy**, **Pin** (up to 30, never expire), and **Delete everywhere**.
- **Send to** one device: it goes straight onto that computer's clipboard, with a notification.
- **Open on** a computer, for a link: that computer's browser opens it.
- **Open here** for a link, **Call** for a phone number, **Read aloud** for any text.
- A **4–8 digit code** shows as a big OTP card.

**One time only** (in *Type*): for a password or a code. The first other device that takes it
removes it everywhere, and an untaken one goes after 2 minutes.

**Images:** screenshots up to 300 KB, between computers. Phones can view and save them, and
send one with *Paste here* where the browser allows reading images.

**Password managers are respected.** What they copy is marked private (concealed on a Mac,
excluded from clipboard history on Windows, a password hint on Linux), and the helper never sends
it.

**Settings:**
- how long unpinned items are kept: an hour, a day, a week, or until the history fills up;
- pause sharing from any device (it still receives);
- remove a device; clear the history;
- reset the group.

A forgotten passphrase can't be recovered: that's the point. **Hold the board's BOOT button for
5 seconds** to start a new group.

## The thing worth understanding

**Encryption that two very different programs agree on.** A phone seals with the browser's
WebCrypto. A computer seals with Python, whose standard library has hashing but no AES. Both must
produce the same bytes, or the phone can't read what the laptop copied. The format (in
`helper/clipcrypto.py` and `tools/clipcrypto.js`):

```
keys    master = PBKDF2-HMAC-SHA256(passphrase, "devkit-clip/1" + group salt, 200,000 rounds)
        enc = HMAC(master, "enc")   mac = HMAC(master, "mac")   auth = HMAC(master, "auth")
an item 0x01 | iv (16) | AES-256-CTR(enc, iv, plaintext) | HMAC-SHA256(mac, everything before)
```

- **AES-CTR with an HMAC seal**, not AES-GCM. Both sides have HMAC built in, and CTR only needs
  AES's encrypt direction. So the Python AES is about 150 lines: table-driven, encryption only.
  It's checked against FIPS-197 and SP 800-38A, against `openssl`, and against the browser. It
  runs at about 860 KB/s, fast enough for a 300 KB screenshot.
- **The counter wraps the WebCrypto way.** With `length: 64`, only the low 64 bits of the iv
  count, and they wrap without carrying. The Python does the same, and a test puts the wrap in
  the middle of an item.
- **Passphrases are compared after NFC normalisation.** "é" typed as one character and as
  "e + accent" are the same passphrase on every device.

**Joining without telling the board the passphrase.** The first device gives the board only the
*auth* key. To join, a device gets a random challenge and answers with HMAC(auth, challenge). The
board checks it with its own small SHA-256 (`clip_sha.h`, tested on a laptop against the RFC 4231
vectors) and hands out a token. The *enc* and *mac* keys never leave the devices, so the auth key
lets the board recognise your devices, but not read anything. Five wrong answers pause joining
for a minute.

**What the board can see:** device names, item sizes and times, and which device sent what to
whom. Not one word of any item.

**No loops.** When the helper puts an item from another device on this clipboard, it remembers
its fingerprint and the clipboard's change counter. So the clipboard "changing" doesn't send the
item straight back.

**Light on the board** (lessons from chapter 12):
- **Computers talk plain http.** What they send is sealed and sealed again by the HMAC, and a
  secure connection would cost the board ~37 KB of memory each. Only phones use https: browsers
  allow the clipboard and WebCrypto only on secure pages.
- **One request at a time from each device.** "Nothing new" is a reply of a few bytes.
- **Items are files on the board's flash** (about 1 MB for items), written as they arrive. An
  image is bigger than the board's free memory, so it never sits in memory whole.
- **The page and the helper are stored gzipped** to fit the firmware (91%). Browsers unpack the
  page themselves, and the installers unpack the helper.

## Endpoints

Every request but the first four needs the device's token `t`.

| Path | | |
|---|---|---|
| `/hello` | | whether a group exists, and its salt |
| `/setup` | `salt`, `auth` | the first device starts the group |
| `/challenge`, `/join` | `n`, `a`, `name`, `kind` | prove the passphrase, get a token |
| `/state` | `v`, `epoch` | devices, the items this device may see, and what changed since `v`. Small items come inline. |
| `/put` | `f` flags, `to`; the body is the sealed item | |
| `/blob` | `id` | one sealed item; a one-time item is gone after |
| `/pin`, `/del`, `/clear` | `id`, `on` | |
| `/device`, `/forget`, `/settings`, `/reset` | `id`, `name`, `paused` / `keep` / `sure` | |
| `/i`, `/i.ps1`, `/u`, `/u.ps1`, `*.py.gz` | | the installers and the helper |

## Files

```
src/clip/clip.ino        the board: WiFi, the clock, the BOOT button
src/clip/serve.h         flash storage, the index, the two web servers
src/clip/clip.h          the group, joining, devices, items, pins, expiry, budget, saving, JSON
src/clip/clip_sha.h      SHA-256 and HMAC for checking a device's proof
src/clip/cert.h, wifi_setup.h, improv.h, ui.h   shared with chapter 12
src/clip/files.h, clip_page.h   GENERATED by helper/embed.py
helper/devkit_clip.py    the computer helper: clipboard on Windows, Mac and Linux, the picker, the hotkey
helper/clipcrypto.py     the encryption, in plain Python
helper/install.*, uninstall.*   the one-line installers
helper/embed.py          writes files.h and clip_page.h
page/clip.html           the page (its crypto comes from tools/clipcrypto.js)
tools/clipcrypto.js      the encryption, in WebCrypto
tools/crypto_test.py     the two encryptions against the standards, openssl and each other
tools/clip_test.cpp      the board's rules
tools/helper_test.py     two real helpers against the laptop build of the board
tools/devserver.cpp      the board's clipboard on a laptop
```

## Limits

- **Phones need one tap.** Browsers can't read the clipboard in the background.
- **Linux needs** `wl-clipboard` (Wayland) or `xclip` (X11).
- **Sizes:**
  - 50 items of history and 30 pins;
  - about 1 MB on the board in all;
  - 300 KB for one image.

  The oldest unpinned items go first.
- **12 devices** in a group, and one group per board.
- **Phones on https:** about three at once comfortably (37 KB each, as in chapter 12). Computers
  use http and cost almost nothing.
- **Your home network only.** Nothing goes over the internet.

## Running it

Flash, then run the installer lines above. To try it on a laptop without a board:

```
cd tools && c++ -std=c++17 -I../src/clip devserver.cpp -o devserver && ./devserver 8093
```

Open http://localhost:8093 (browsers treat localhost as secure). Point a helper at it with
`DEVKIT_CLIP_HOSTS=localhost:8093 python3 ../helper/devkit_clip.py --setup`.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module, default partition, no warnings
from the sketch: **1,203,334 bytes, 91% of 1.31 MB**, globals **24% of RAM**.

**Encryption** (`tools/crypto_test.py`), **216 checks**:
- FIPS-197 C.3 and SP 800-38A F.5.5, and 37 comparisons against `openssl` with random keys and
  lengths.
- Tampering anywhere in an item is caught, and so is a wrong passphrase. Every seal gets a fresh
  iv, and NFC is applied.
- **24 cases in Chrome:**
  - Passphrases in Hindi, emoji, with decomposed accents, and empty.
  - Items from 0 to 70,000 bytes, the counter wrapping mid-item, and the full 200,000 rounds.
  - The browser and Python seal **identical bytes**, each opens the other's items, both make the
    same proof for the board, the browser's saved keys are Python's, and both refuse a wrong
    passphrase.

**The board's rules** (`tools/clip_test.cpp`, with address and undefined-behaviour sanitizers),
**4,795 checks**:
- SHA-256 and HMAC against FIPS and RFC 4231 vectors.
- The board accepts a proof computed by the real Python helper.
- **Joining:** setup once and only with the board's salt, challenges used once and only within a
  minute, the pause after five wrong answers, and the device limit.
- **Items:**
  - inline or fetched separately;
  - one device only, and one time only (the sender looking doesn't use it up);
  - untaken one-time items going after 2 minutes;
  - empty, too big and paused sends refused;
  - delete, pins (30), clear keeping pins, and a removed device taking its private items with it.
- **History and storage:** history 50 with the oldest going first and their bytes too; pins
  surviving; the 1 MB budget; expiry by day, with undated and pinned items kept, and "forever".
- **Restarts and reset:** devices going offline and coming back; saving, a restart, tokens still
  working, items whose bytes are missing dropped, a damaged record refused; reset giving a new
  epoch.
- **Large and random use:**
  - 50 inline items over several replies, every reply whole JSON within the 12 KB buffer;
  - 60 random rounds of up to 8 devices, where the books always balanced and every device's view
    matched the board's.

**Two real helpers** (`tools/helper_test.py`), with files standing in for their clipboards, **18
checks**:
- Joining:
  - the first computer starts the group and the second joins;
  - a wrong passphrase is refused.
- Text and back:
  - Hindi and emoji text goes A to B, and a reply comes back;
  - **no loop:** two copies made exactly two items;
  - the board's copy contains none of the words.
- Large items: a 150 KB text and a 60 KB image both arrive whole.
- Pausing: a paused computer sends nothing, even after it resumes.
- Targeted items:
  - a one-time code sent to B reached B's clipboard and left the board;
  - a link sent to A was opened by A.
- Leaving and other commands:
  - the picker's list decrypts;
  - status lists the devices;
  - leaving removes the device and its keys.

**The page in Chrome with a helper** (laptop build of the board, stand-in phone clipboard):
- The page joined with the passphrase (WebCrypto, 200,000 rounds), and the board accepted its
  proof.
- It showed the laptop's copy decrypted.
- *Paste here* reached the laptop's clipboard.
- *Copy*, pin, the Pinned tab and search worked.
- A one-time code typed on the page reached the laptop.
- The laptop's 6-digit copy showed as an OTP card, and the device list was right.

**On this Mac's real clipboard** (a short test, with the clipboard saved first and put back after),
**11 checks**:
- Hindi and emoji text, and a PNG image, went onto the real clipboard and came back exactly.
- `pbpaste` saw what the helper wrote, and the change counter moved.
- An item marked concealed (as a password manager marks it) was refused, and later never sent.
- A real copy on the Mac reached a second helper, and that helper's copy landed on the Mac's
  clipboard, with no echo.
- The quick picker window opened against the history without errors.

**Not yet confirmed:**
- **anything on the board itself**;
- **the Windows helper:** the clipboard through `ctypes`, the Win+Alt+V hotkey, the PowerShell
  installer and the gzip unpacking were written to the documented APIs but never run, since this
  was built on a Mac;
- **Linux:** the `wl-clipboard`/`xclip` code hasn't been run;
- **the picker's paste keystroke** (pressing ⌘V or Ctrl+V for you), and using the picker by hand;
- phones other than Chrome on a laptop, and the iPhone Shortcut.

## Sources

- FIPS-197 (AES), NIST SP 800-38A (CTR), RFC 2104 and RFC 4231 (HMAC), RFC 8018 (PBKDF2).
- The Web Crypto API (AES-CTR `length`, PBKDF2, HMAC) and the Async Clipboard API.
- nspasteboard.org (concealed and transient types on macOS). Microsoft's clipboard formats
  `ExcludeClipboardContentFromMonitorProcessing`, `CanIncludeInClipboardHistory` and
  `CanUploadToCloudClipboard`, and `RegisterHotKey`. KDE's `x-kde-passwordManagerHint`.
- Chapter 12 for https on the board and keeping it light; chapters 9 and 10 for the helper and
  its installers.
