#pragma once
//
// The pet, and nothing else — no radio, no web, no storage. Its life (hunger, happiness, energy,
// sleep) and the one rule that makes it a pet rather than a picture: it is in exactly one place.
// Kept apart so the whole thing can be run for simulated days on a laptop (pet_test).
//
// Every computer running the helper checks in with a "beat" about once a second. The board keeps
// the list, decides which computer the pet is on, and moves it when it walks off a screen, when
// its computer goes quiet, or when someone calls it from a phone.

#include <stdint.h>
#include <string.h>

enum { PET_CAT = 0, PET_DOG = 1, PET_BUNNY = 2, PET_FOX = 3, PET_PANDA = 4, PET_KINDS = 5 };
enum { PET_MAX_DEV = 8, PET_NAME = 12, PET_DEV_NAME = 24, PET_ID = 8 };
static const uint32_t PET_GONE_MS = 10000;        // a computer this quiet is treated as gone
static const float PET_WAKE_HOLD_S = 20 * 60;     // woken by hand: no dozing off again for 20 min

// what was last done to the pet, so whichever screen it's on can show it
enum { EV_NONE = 0, EV_FEED, EV_PET, EV_PLAY, EV_SLEEP, EV_WAKE, EV_POKE, EV_ADOPT };
// why an action was refused
enum { ACT_OK = 0, ACT_ASLEEP, ACT_FULL, ACT_TIRED, ACT_NOPET, ACT_UNKNOWN };

struct PetLife {
  bool adopted;
  uint8_t kind;                    // PET_CAT .. PET_PANDA
  char name[PET_NAME + 1];
  float hunger;                    // 0 starving .. 100 full
  float happy;                     // 0 .. 100
  float energy;                    // 0 exhausted .. 100 rested
  bool asleep;
  float hold;                      // seconds left before it may doze off again
};

struct PetDev {
  bool used;
  char id[PET_ID + 1];             // random, chosen by the helper on first run
  char name[PET_DEV_NAME + 1];     // the computer's name, for the phone page
  uint32_t seen;                   // millis() of its last beat
};

struct PetWorld {
  PetLife pet;
  PetDev dev[PET_MAX_DEV];
  int8_t at;                       // which computer the pet is on, -1 nowhere
  uint8_t side;                    // the edge it walks in from there: 0 left, 1 right
  uint32_t arrival;                // bumps on every arrival, so a helper knows to walk it in
  uint32_t evId;                   // bumps on every event
  uint8_t evKind;
};

static inline float petClamp(float v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

// ── life ─────────────────────────────────────────────────────────────────────

static inline void petAdopt(PetWorld &w, uint8_t kind, const char *name) {
  memset(&w.pet, 0, sizeof w.pet);
  w.pet.adopted = true;
  w.pet.kind = kind < PET_KINDS ? kind : PET_CAT;
  strncpy(w.pet.name, name, PET_NAME);
  w.pet.hunger = 80; w.pet.happy = 80; w.pet.energy = 80;
  w.evId++; w.evKind = EV_ADOPT;
}

static inline bool petNight(float hour) { return hour >= 0 && (hour >= 22 || hour < 7); }

// Advance its life by dt seconds. hour is the local time of day (0..24), or -1 if not known.
// Rates: awake it gets hungry over ~6 hours, tired over ~10, lonely over ~5 (twice as fast when
// hungry); asleep it rests back to full in about an hour and a half and gets hungry half as fast.
static inline void petTick(PetWorld &w, float dt, float hour) {
  PetLife &p = w.pet;
  if (!p.adopted || dt <= 0) return;
  if (p.hold > 0) p.hold = p.hold > dt ? p.hold - dt : 0;
  if (p.asleep) {
    p.energy = petClamp(p.energy + dt * (100.0f / 5400));
    p.hunger = petClamp(p.hunger - dt * (100.0f / 43200));
    // up in the morning once rested, or any time it's fully rested during the day
    if (!petNight(hour) && p.energy >= 99) p.asleep = false;
  } else {
    p.hunger = petClamp(p.hunger - dt * (100.0f / 21600));
    p.energy = petClamp(p.energy - dt * (100.0f / 36000));
    p.happy = petClamp(p.happy - dt * (100.0f / 18000) * (p.hunger < 25 ? 2 : 1));
    // dozes off when exhausted, or at night — unless someone has just woken it up
    if (p.hold <= 0 && (p.energy < 15 || (petNight(hour) && p.energy < 90))) p.asleep = true;
  }
}

static inline int petAct(PetWorld &w, const char *a) {
  PetLife &p = w.pet;
  if (!p.adopted) return ACT_NOPET;
  if (!strcmp(a, "feed")) {
    if (p.asleep) return ACT_ASLEEP;
    if (p.hunger > 90) return ACT_FULL;
    p.hunger = petClamp(p.hunger + 35); p.happy = petClamp(p.happy + 5);
    w.evKind = EV_FEED;
  } else if (!strcmp(a, "pet")) {
    p.happy = petClamp(p.happy + (p.asleep ? 3 : 8));
    w.evKind = EV_PET;
  } else if (!strcmp(a, "play")) {
    if (p.asleep) return ACT_ASLEEP;
    if (p.energy < 20) return ACT_TIRED;
    p.happy = petClamp(p.happy + 20); p.energy = petClamp(p.energy - 12); p.hunger = petClamp(p.hunger - 6);
    w.evKind = EV_PLAY;
  } else if (!strcmp(a, "sleep")) {
    p.asleep = true; p.hold = 0;
    w.evKind = EV_SLEEP;
  } else if (!strcmp(a, "wake")) {
    p.asleep = false; p.hold = PET_WAKE_HOLD_S;
    w.evKind = EV_WAKE;
  } else if (!strcmp(a, "poke")) {                 // clicked on a computer screen
    p.happy = petClamp(p.happy + 4);
    w.evKind = EV_POKE;
  } else {
    return ACT_UNKNOWN;
  }
  w.evId++;
  return ACT_OK;
}

static inline const char *petMood(const PetLife &p) {
  if (!p.adopted) return "none";
  if (p.asleep) return "sleeping";
  if (p.hunger < 25) return "hungry";
  if (p.energy < 20) return "tired";
  if (p.happy < 30) return "lonely";
  if (p.happy > 70) return "happy";
  return "content";
}

// ── where it is ──────────────────────────────────────────────────────────────

static inline bool petOnline(const PetWorld &w, int i, uint32_t now) {
  return i >= 0 && i < PET_MAX_DEV && w.dev[i].used && now - w.dev[i].seen < PET_GONE_MS;
}

static inline int petOnlineCount(const PetWorld &w, uint32_t now) {
  int n = 0;
  for (int i = 0; i < PET_MAX_DEV; i++) if (petOnline(w, i, now)) n++;
  return n;
}

static inline void petArrive(PetWorld &w, int i, int side) {
  w.at = i; w.side = side ? 1 : 0; w.arrival++;
}

// A computer checks in. Returns its slot, or -1 if all are taken by computers still online.
// The first computer to check in while the pet is nowhere gets it.
static inline int petBeat(PetWorld &w, const char *id, const char *name, uint32_t now, int side) {
  int slot = -1;
  for (int i = 0; i < PET_MAX_DEV; i++)
    if (w.dev[i].used && !strcmp(w.dev[i].id, id)) { slot = i; break; }
  if (slot < 0) {                                   // new: an empty slot, or the longest-gone one
    uint32_t oldest = 0;
    for (int i = 0; i < PET_MAX_DEV; i++) {
      if (!w.dev[i].used) { slot = i; break; }
      if (!petOnline(w, i, now) && now - w.dev[i].seen >= oldest) { oldest = now - w.dev[i].seen; slot = i; }
    }
    if (slot < 0) return -1;
    if (w.at == slot) w.at = -1;                    // reusing the slot the pet was stranded on
    memset(&w.dev[slot], 0, sizeof w.dev[slot]);
    w.dev[slot].used = true;
    strncpy(w.dev[slot].id, id, PET_ID);
  }
  strncpy(w.dev[slot].name, name, PET_DEV_NAME);
  w.dev[slot].seen = now;
  if (w.at < 0 && w.pet.adopted) petArrive(w, slot, side);
  return slot;
}

// The next computer round from i that is online, other than i itself; -1 if there is none.
static inline int petNextFrom(const PetWorld &w, int i, uint32_t now) {
  for (int k = 1; k < PET_MAX_DEV; k++) {
    const int j = (i + k) % PET_MAX_DEV;
    if (petOnline(w, j, now)) return j;
  }
  return -1;
}

// The pet walked off computer i's screen through `side` (0 left, 1 right). It turns up on the
// next computer walking in from the opposite edge, as if the screens stood side by side — or,
// alone on the network, turns round and walks back in where it left.
static inline int petLeave(PetWorld &w, int i, int side, uint32_t now) {
  if (w.at != i) return w.at;                       // not here any more; nothing to do
  const int next = petNextFrom(w, i, now);
  if (next < 0) petArrive(w, i, side);
  else petArrive(w, next, side ? 0 : 1);
  return w.at;
}

// Called from a phone: bring the pet to computer i.
static inline bool petCall(PetWorld &w, int i, uint32_t now, int side) {
  if (!w.pet.adopted || !petOnline(w, i, now)) return false;
  if (w.at != i) petArrive(w, i, side);
  return true;
}

// The pet's computer went quiet (asleep, closed, off the network): it moves to another one, or
// waits nowhere until a computer comes back.
static inline void petSweep(PetWorld &w, uint32_t now, int side) {
  if (!w.pet.adopted) { w.at = -1; return; }
  if (w.at >= 0 && petOnline(w, w.at, now)) return;
  int to = -1;
  for (int i = 0; i < PET_MAX_DEV; i++) if (petOnline(w, i, now)) { to = i; break; }
  if (to >= 0) petArrive(w, to, side);
  else w.at = -1;
}
