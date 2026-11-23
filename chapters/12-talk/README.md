# 12 · Talk across languages

> **Needs:** one ESP32 DevKit V1 and the phones in the room, on home WiFi with internet.
> **Status:** being built. Step 1, the microphone test, is on the flasher.

Everyone in a room speaks and reads in their own language. Phones listen and speak with their
own speech recognition and voices; the board runs the room, translates through a free online
service, and shows live captions on any screen.

## Step 1: the microphone test

Phones only allow the microphone on **https** pages, and the whole chapter depends on that
working with a board on a home network. So this comes first.

`tools/mictest/` makes the board's own certificate (an ECDSA P-256 key, made on the first boot
and kept in flash, different on every board), serves the same test page on
`https://talk.local` and `http://talk.local`, and reports what the phone allows:

- whether the page counts as secure, and whether speech recognition exists in this browser;
- whether the microphone was allowed;
- what the board's translation returned, which service, and how long it took;
- the phone's voices, and the board's free memory with two secure connections open.

**Try it:** flash **Talk: microphone test** from the flasher, open `https://talk.local` on a
phone, accept the "not private" warning once (Android: *Advanced*, *Proceed*; iPhone: *Show
details*, *visit this website*), choose your languages and tap the mic.

## Found so far

- **Translation:** the well-known `translate.googleapis.com …client=gtx` address answered this
  home network with Google's "Sorry, unusual traffic" page. The address Google's own Chrome
  extension uses (`clients5.google.com/translate_a/t?client=dict-chrome-ex`) answers normally
  and detects the language itself, so that is used, with **MyMemory** as the fallback.
  `tools/mictest/translate.h` reads both, checked against live answers in Hindi, Tamil, Bengali,
  Urdu, French, Japanese and Chinese, quotes and emoji, Google's error page and MyMemory's
  daily-limit answer.
- **Size:** 1,165,232 bytes, 88% of the default partition, with the https server, the https
  client and the page.
