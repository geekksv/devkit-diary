# 14 · No ads on this WiFi

> **Needs:** one ESP32 DevKit V1 on your home WiFi. Nothing installed on your phones or laptops.
> **Status:** run on the board itself, on a home WiFi. It answered 3,000 lookups from a laptop at
> 103 a second (median 43 ms), blocked the ad servers, and blocked none of 2,099 lookups of
> everyday sites. Not yet tried: phones joined to NoAds for a day of real use.

An ad blocker for every phone and laptop, with no app and no settings on them. The board joins
your home WiFi and makes a second network, **NoAds**. Anything that joins NoAds gets the internet
through the board, and the board quietly refuses the ~180,000 ad and tracker addresses on a
well-kept public blocklist. Websites load without their ads, free apps lose their banners, and
trackers can't phone home.

```
phone ──"NoAds"──► ESP32 ──home WiFi──► router ──► internet
                   (says "nowhere" to ad servers)
```

## Using it

1. Flash **No ads** from the web flasher and give it your home WiFi. On first start it downloads
   the blocklist, which takes about three minutes before NoAds appears.
2. Open the dashboard at **http://noads.local** (from your home WiFi), or **http://192.168.4.1**
   (from NoAds). It shows NoAds' password, which the board makes up on first start.
3. On a phone or laptop, join **NoAds** with that password. Browse as usual.

The dashboard shows:
- **Lookups and the share blocked**, plus a 60-minute chart.
- **A live feed:** every lookup, with one-tap Allow or Block.
- **What's blocked most,** and which device asks most.
- **Pause blocking:** 5 minutes, 30 minutes or an hour, for when a site won't work.
- **Always allow and always block lists** of your own, up to 32 names each. A name covers
  everything under it: blocking `tiktok.com` also blocks `www.tiktok.com`.
- **Check a site:** whether a name would be blocked, and why.
- **Settings:**
  - the blocklist: Normal, Light or Classic;
  - NoAds' name and password;
  - which DNS servers allowed lookups go to (Cloudflare 1.1.1.1 and Google 8.8.8.8 to start).

The list updates itself once a week, between 3 and 5 in the morning by your dashboard's clock.
To update, the board restarts in update mode. NoAds is off for about 3 minutes, and phones fall
back to your home WiFi by themselves (see below for why).

### What it can't block

A DNS blocker works by refusing to say where ad *servers* are. So it can't block:
- **YouTube video ads,** which come from the same servers as the videos;
- **Instagram and Facebook sponsored posts,** which are part of the feed itself;
- **Google's "Sponsored" search results.** The default list leaves `googleadservices.com` alone,
  so clicking them still works. The Classic list blocks it.

Some devices skip the board and ask the internet directly:
- **Android:** turn off **Settings → Network → Private DNS**.
- **Chrome:** turn off **Settings → Privacy → Use secure DNS**.
- **iPhone:** turn off **iCloud Private Relay** for this network.
- **Firefox:** nothing to do. It switches its own off when the board tells it to (Mozilla's
  "canary domain", `use-application-dns.net`).

### Speed

NoAds devices share one radio with the board's link to your router: expect 5 to 15 Mbps between
them. That's fine for browsing, social apps, music and normal video, but not for 4K or big
downloads. Up to 8 devices at once.

### Router mode: every device, full speed

If you can change your router's settings, point its **DNS server** at the board's home address,
shown on the dashboard. Every device on your home WiFi is then covered at full speed, without
joining NoAds.
- **Give the board a fixed address** (a "DHCP reservation") so it never moves.
- **Router-wide, the board is part of everyone's internet:** if it's unplugged, nobody can look
  sites up until you put the router's DNS back.

## The thing worth understanding

**DNS is the internet's phone book.** Before loading `ads.example.com`, a phone asks "what's its
address?" The board answers every such question:
- **On the list:** it answers at once with `0.0.0.0`, which means "nowhere", so the ad never
  loads.
- **Not on the list:** it passes the question to a real DNS server and hands the answer back,
  remembered for a while so the next phone asking gets it instantly.
- **Names under a listed one:** it checks the name and every name above it, so `x.ads.example.com`
  is caught by `ads.example.com`, but never a bare `com`.

**180,000 names in 300 KB of memory.** The names as text would take 3.7 MB. So each name becomes a
6-byte **fingerprint** (a hash), and they're sorted and stored in flash:
- **A small index in memory:** the first fingerprint of every 512, about 3 KB.
- **One flash read per check:** the index points to one 3 KB block, read once, then searched.
- **Mix-ups:** two different names sharing a fingerprint is about one in a billion per lookup. The
  tests tried 2 million made-up names, and none was wrongly blocked.

**Sorting 180,000 numbers on a chip with no room to hold them.** While the list downloads, each
fingerprint goes into one of 8 files by its first 3 bits. Then each file is read 16 times, each
time keeping one sixteenth (the next 4 bits): about 1,500 numbers, 12 KB, sorted in memory and
written out. The 128 pieces come out in order, so the whole file is sorted.

Getting this right on the board took several attempts, and each is worth knowing:
- **Growing a list:** step by step, it briefly needs half as much again. That ran out of memory,
  so the sizes are counted during the download and the space is reserved at start.
- **"Free" memory isn't one piece:** 120 KB free doesn't mean a 24 KB block exists. The pieces
  were made small enough to always fit.
- **Changing a file's start copies the whole file:** in flash, it needs room for two copies. So
  the list's header went to the **end**, written last. A list cut short by a power cut never
  ends in a valid one, so it's never mistaken for whole.
- **A secure download wants ~50 KB.** With NoAds routing, DNS and the dashboard all running, the
  board had 100 KB free, but once the download's connection was open, its largest free block
  was **6.6 KB**. Downloads broke off part way. So updates run in a restart of their own: home
  WiFi only, nothing else, all the memory. That costs 3 minutes of NoAds a week, at night, and
  works every time.

## Endpoints

| Path | What it does |
|---|---|
| `GET /state` | Everything the dashboard shows |
| `POST /pause` | `min` minutes (0 resumes) |
| `POST /allow`, `/block` | `name`, and `do=remove` to take one off |
| `POST /lookup` | Would `name` be blocked, and why |
| `POST /settings` | `list`, `ap`, `pass`, `dns1`, `dns2` |
| `POST /update` | Fetch the list now |

DNS on UDP port 53, on both networks.

## Files

- `src/noads/noads.ino`:
  - the two networks and the routing between them (NAPT);
  - the DNS server, a task of its own;
  - the weekly update;
  - the dashboard.
- `src/noads/filter.h`: names, fingerprints, deciding, DNS packets, the answer cache and the
  statistics.
- `src/noads/blocklist.h`: the list file, its index and lookups, and reading list lines in every
  common format.
- `src/noads/builder.h`: downloading text into a sorted list file in little memory.
- `src/noads/noads_page.h`: the dashboard.
- `src/noads/wifi_setup.h`, `improv.h`, `ui.h`: WiFi setup, shared with the other chapters.
- `tools/filter_test.cpp`: all the logic on a laptop. Given real list files, it builds them as the
  board does and checks every name:
  `c++ -std=c++17 -O2 -I../src/noads filter_test.cpp -o t && ./t multi-onlydomains.txt`

**Partition scheme:** "No OTA (2MB APP/2MB SPIFFS)", for room to keep the list and build the next
one. The web flasher sets it.

## Verified, and not

**On a laptop:**
- **The real lists:** HaGeZi Normal (184,283 names) and Light (84,559), and StevenBlack (81,676),
  all built the way the board builds them. Every name is found afterwards, and 0 of 2 million
  made-up names is wrongly blocked.
- **Everyday sites stay allowed:** Google, YouTube, WhatsApp, Instagram, Netflix, Amazon, Flipkart,
  Paytm and HDFC.
- **DNS packets:** blocked answers for A, AAAA and other kinds; the Firefox signal; and 200,000
  random packets, none crashing the parser.
- **The cache:** expiry, other types, server failures not kept, and the least-used going first.
- **Statistics:** the most-blocked list and the per-minute chart.

**On the board:**
- **The update works:** the list downloads (3.7 MB in about 2 minutes) and is built and used. Ad
  servers get `0.0.0.0`; `google.com` and `youtube.com` resolve.
- **Three updates in a row** with no restart.
- **Load:** 3,000 lookups at 103 a second, median 43 ms. 0 of 2,099 everyday lookups was blocked.

**Not yet confirmed:**
- phones on NoAds for a day of real use, and real speeds through it;
- router mode in a real router.

## Sources

- HaGeZi DNS blocklists (Multi Normal and Light, wildcard domains) and StevenBlack's unified hosts
  list.
- RFC 1035 (DNS messages) and Mozilla's canary domain for DNS-over-HTTPS.
- The ESP32 core's NAPT routing, and its built-in certificate bundle for downloading over HTTPS.
