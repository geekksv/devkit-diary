# 12 · Talk across languages

> **Needs:** one ESP32 DevKit V1, the phones in the room, and home WiFi with internet. A TV or
> laptop for captions is nice, not needed.
> **Status:** written and tested on a laptop: the room rules through the same code, and the real
> page in five browsers at once with real translation (figures below). The microphone test
> (step 1) passed on the board itself, on a Mac and an Android phone. The full chapter is not yet
> run on the board.

Everyone in a room speaks and reads in **their own language**. Grandma speaks Hindi, the guest
reads English, a cousin reads Tamil, and the TV shows French captions, all at once. Each phone
listens with its own speech recognition, translates, and reads the others aloud in its own
voice. The board keeps the room together.

## Using it

1. Open **https://talk.local** on a phone. The first time, the phone warns that the page is
   "not private": the board made its own certificate (below). Android: *Advanced* → *Proceed*.
   iPhone: *Show details* → *visit this website*.
2. Type your name, pick **the language you speak**, and **Start a room**. You get a four-letter
   code. Everyone else joins with it, or taps the room in the list, each in their own language.
3. **Tap the mic and speak**, or type. Everyone sees what you said in their language, with your
   words underneath, and their phone reads it aloud (the speaker button turns that off per phone).

**Also in the room:**
- **Quick phrases:** Yes, No, Thank you, Please speak slowly, Where is the bathroom?, and more.
  One tap, and everyone gets it in their language.
- **Keep listening (captions):** the phone listens until you stop it. Everyone sees what is being
  said as it is said (the grey "speaking now" line), then each sentence in their own language.
  For a talk, a prayer meeting, or a relative who is hard of hearing.
- **Show captions on a TV:** open `http://talk.local/#tv=CODE` on a TV or laptop. It shows the last
  few sentences, large, in the language picked on that screen. Tap once to have them read aloud.
- **Bigger text**, **change my language**, **save the conversation** (a text file with every
  message and its translation), and **clear for everyone** (the host).

**One phone, two people:** the screen splits in two, the top half upside down for the person
across the table. Each half has its own language and its own mic, and each sees the other's words
in their own language, read aloud.

**The page** follows the phone's light or dark setting, with line icons drawn into the page itself
(no emoji, nothing loaded from the internet).

**Tools** (no room needed): **Dictate** (speak, get text to copy), **Read aloud** (paste text,
pick a language and speed), and **Translate** (type in any language).

**25 languages:** English, Hindi, Bengali, Tamil, Telugu, Marathi, Gujarati, Kannada, Malayalam,
Punjabi, Urdu, Odia, Assamese, Nepali, Spanish, French, German, Italian, Portuguese, Russian,
Arabic, Turkish, Chinese, Japanese and Korean. Adding one is one line in the page.

## The thing worth understanding

**Phones only allow the microphone on https pages.** A board on a home network has no
certificate authority to vouch for it, so it **makes its own certificate** on its first boot
(`cert.h`):
- An ECDSA P-256 key, made in well under a second, kept in flash, and different on every board.
  No key is in the code.
- A certificate naming `talk.local` and the board's IP address (browsers read only those names),
  marked for web servers and valid for under 825 days (iPhones refuse anything else).

Phones can't check that certificate against anyone, so they warn once. After that, the page is
a secure page like any other, and the microphone works. The board makes a new certificate if its
IP address changes, so expect the warning again then.

**Who does the work.** The board has 520 KB of memory and no microphone, so it can't listen or
translate. Phones can:
- **listening:** the phone's own speech recognition, the same as its keyboard's mic button;
- **translating:** straight from the phone, through Google (the address Google's own Chrome
  extension uses), with **MyMemory** as the fallback;
- **speaking:** the phone's own voices.

The board keeps the room (`talk.h`): who is in it and in which language, every message, the live
lines, and **every translation a phone makes**. The first phone reading Hindi translates a
message and hands the result back; every other Hindi phone, and a Hindi caption screen, gets
it from the board.

**Why the board doesn't translate.** The first version did, and step 1 showed the cost: one
secure connection out to Google takes about **55 KB** of the board's memory while it runs. With a
Mac and an Android phone both connected over https, free memory fell to **48 KB**. Each phone's
https connection needs about 37 KB too. Moving translation to the phones leaves all of that for
the phones, and a room can hold any number of languages.

**Two doors.** The same page is on `https://talk.local` (port 443) for phones that speak, and on
`http://talk.local` (port 80) for screens that only read, such as the TV. Reading screens get no
warning and cost the board much less memory. Both doors answer through the same `route()`.

## Endpoints

All take the room code `c`. Those marked *seat* also need your seat token `t`. Every answer is JSON.

| Path | Body | |
|---|---|---|
| `/rooms` | | open rooms, for the start screen |
| `/create` | `n` name, `l` language | a new room; you host |
| `/join` | `n`, `l` (`t` to come back) | a seat; coming back with a new `l` changes your language |
| `/state` | `v` version, `tl` translation language | people, and messages changed since `v`, with the `tl` translation if the board has one. `more`: come straight back |
| `/say` *seat* | `x` text, `l` its language, `k` 0 spoken / 1 typed / 2 phrase | |
| `/tr` | `id`, `tl`, `x` | a phone's translation of a message; the first for each language is kept |
| `/live` *seat* | `x` words so far, `on` listening | the grey "speaking now" line |
| `/me` *seat* | `n`, `l` | change name or language |
| `/leave` *seat*, `/clear` *seat*, host only | | |

## Files

```
src/talk/talk.ino       the board: WiFi, the http and https servers, nothing else
src/talk/talk.h         the rooms: people, messages, shared translations, live lines, memory budget, JSON
src/talk/talk_page.h    the page: rooms, mic, translation, voices, captions, pass mode, tools, TV screen
src/talk/cert.h         the board's own certificate
src/talk/serve.h        one lock and one reply buffer for the two servers
src/talk/wifi_setup.h, improv.h, ui.h   joining home WiFi (shared with chapters 7–11)
tools/talk_test.cpp     the room rules, tested on a laptop
tools/devserver.cpp     the same rooms on a laptop, for trying it and for the end-to-end tests
tools/mictest/          step 1: the microphone and translation test that came first
```

## Limits

- **Internet is needed.** Phones need it to listen (most send audio to Google or Apple) and to
  translate. Without home WiFi, the board only offers `devkit-talk` to set WiFi up.
- **Secure connections:** the board keeps up to **3** https connections open, about 37 KB each.
  Up to three phones speaking at once is comfortable. A fourth closes the quietest connection,
  which reconnects on its next poll. Screens that only read should use `http://`.
- **Rooms:** 2 at once, 10 seats each (anyone can watch without one). A room keeps about 16 KB of
  text, roughly the last 60 sentences with their translations, and the oldest go first.
- **Browsers:** listening works in Chrome (Android, computers), Edge, and Safari on iPhone, **not in
  Firefox**. The page says so and still lets you type, read and hear.
- **Voices** depend on the phone. Without a voice for a language, the phone shows the text and
  says so, and doesn't read it in the wrong voice.
- **Translation** uses free services with no account. Google's address is unofficial and could
  change. MyMemory is the fallback and allows about 5,000 characters a day.
- **Privacy:** the speech services hear your voice and the translators see the words. The board
  keeps messages in memory only and forgets them when it restarts.

## Running it

Flash **Talk across languages** from the flasher and give it your WiFi on the same page. Then
open **https://talk.local** on a phone (or the https address printed on the flasher page). For a
TV, open `http://talk.local/#tv=CODE`.

To try it on a laptop with no board:

```
cd tools && c++ -std=c++17 -I../src/talk devserver.cpp -o devserver && ./devserver 8091
```

Then open http://localhost:8091. Browsers treat `localhost` as secure, so the mic works there.

## Verified, and not

Compiled against arduino-esp32 **3.3.11** for ESP32 Dev Module, on the default partition, with no
warnings from the sketch: **1,156,640 bytes, 88% of 1.31 MB**, and globals at **22% of RAM**.

**Step 1 on the board itself** (`tools/mictest`):
- On a Mac in Chrome and an Android phone in Chrome, the board's own certificate made a secure
  page, speech recognition was there, and the microphone was allowed.
- Translation on the board took 1.0 to 1.4 s. Free memory was 139 KB with one phone and 102 KB
  with two, and fell by about 55 KB during a translation. That is why the phones translate now.
- `translate.googleapis.com …client=gtx` answered this home network with Google's "unusual
  traffic" page. The Chrome extension's address answered normally, so that is the one used.

**The room rules, on a laptop** (`tools/talk_test.cpp`, with address and undefined-behaviour
sanitizers), **2,259 checks**:
- **Text:** controls, spaces, markup in names, broken UTF-8, and Hindi, Tamil and emoji never cut
  mid-letter at any length.
- **A room through the same requests the phones send:**
  - rejoining keeps your seat, and can change your language;
  - a translation is sent only to phones reading that language, the first one wins, and at most
    8 are kept per message;
  - the speaker's own translation is accepted along with the message;
  - live lines come and go, and saying something clears yours;
  - clearing tells phones to start again;
  - host hand-over, leaving, the last one out closing the room, room and seat limits, a seat
    away 10 minutes going to someone new, idle rooms closing.
- **The worst reply:** ten people with live lines of quotes and long messages of quotes. 9 KB of
  the 12 KB buffer, whole JSON, and the rest comes in the next reply.
- **300 random rooms**, 2–10 people and a caption screen, random messages, translations, live
  lines and nonsense requests:
  - every reply was whole JSON;
  - the memory budget held after every step;
  - every phone that kept up matched one that started from nothing.

**End to end**, with the real page in five headless Chrome "phones" against the laptop build of
the board's rooms, a stand-in microphone (nothing can speak into a headless browser), and real
translation from the page:
- Asha spoke Hindi; Ben read "How are you? The weather is very nice today."
- Ben typed English; Asha read "मैं ठीक हूं धन्यवाद। मौसम अच्छा है।"
- With *keep listening* on, Ben saw Asha's words as she spoke them, then the sentence in English,
  and Asha's mic started itself again.
- Ben's quick phrase "Thank you" reached Asha as "धन्यवाद".
- The caption screen showed every sentence in French.
- Pass mode: "मुझे एक गिलास पानी चाहिए।" became "I would like a glass of water." on the other half,
  and the reply came back in Hindi.
- Tools: "Good morning, where is the station?" became Tamil. Dictation kept two sentences.
- The host cleared the room, and Ben's screen emptied.

**Not yet confirmed:**
- the full chapter on the board with real phones;
- iPhone Safari (only Mac and Android were tried in step 1);
- how three phones speaking over https at once feel on the board.

## Sources

- The Web Speech API (`SpeechRecognition`, `speechSynthesis`) and when browsers allow the microphone.
- RFC 5280 for certificate fields, and Apple's requirements for TLS server certificates (SAN,
  serverAuth, 825 days).
- Mbed TLS (in ESP-IDF) for making keys and certificates. ESP-IDF's `esp_https_server`.
- Google's and MyMemory's free translation addresses, as tested above.
- Chapters 8 and 11 for rooms, codes and seats. Chapters 7–11 for `wifi_setup.h`.
