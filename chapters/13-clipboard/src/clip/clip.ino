// One clipboard for every device: copy on one, paste on another. End-to-end encrypted.
//
// The devices seal everything with a passphrase the board never sees; the board keeps the sealed
// items, tells each device what's new, and serves the page and the computer helper. All its
// rules are in clip.h, tested on a laptop. This file connects them to:
//   - flash (LittleFS): one file per sealed item, and a small index, written a moment after
//     anything changes, so the history survives a restart;
//   - the clock (NTP), so items expire on time even after a restart;
//   - two web servers, both ESP-IDF's, answering through the same handler:
//       https://clip.local (443) for phones and browsers: they need a secure page for the
//         clipboard and for WebCrypto. The certificate is the board's own (cert.h).
//       http://clip.local (80) for the computer helpers: what they send is sealed already, and
//         a secure connection would cost the board ~37 KB of memory each.
//   - the BOOT button: hold it 5 seconds to reset the group (for a forgotten passphrase).
//
// Home WiFi is needed. With no saved network, the board opens devkit-clip to set one up.
//
// Board: ESP32 DevKit V1, nothing attached. Default partition scheme (its 1.4 MB of flash files
// hold the items).

// ESP-IDF's server first: it defines HTTP_ANY too, and Arduino's WebServer (used only by the WiFi
// setup page) needs its own.
#include <esp_https_server.h>
#undef HTTP_ANY
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_random.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <time.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "cert.h"
#include "clip.h"
#include "clip_page.h"
#include "files.h"
#include "serve.h"

const char *HOST = "clip";
const int BOOT_BUTTON = 0;              // the board's own BOOT button

WebServer web(80);                       // only for the WiFi setup page

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nclip: starting");
  cRand = hwRand;
  lock = xSemaphoreCreateMutex();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  if (!LittleFS.begin(true)) Serial.println("clip: flash files could not be mounted");
  store = &flashStore;
  loadIndex();

  const wifisetup::Config cfg = {HOST, "devkit-clip", "devkit-clip", "13", false};
  if (!wifisetup::begin(cfg, web)) return;     // no WiFi yet: the setup page runs from loop()
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  hosts = String(HOST) + ".local " + WiFi.localIP().toString();
  startServers();
  Serial.printf("clip: ready, %lu bytes free, %u items, %lu KB of flash used\n", (unsigned long)ESP.getFreeHeap(),
                itemCount(), (unsigned long)(LittleFS.usedBytes() / 1024));
}

void loop() {
  wifisetup::loop();
  if (!wifisetup::online()) { delay(5); return; }
  const uint32_t now = millis();
  static uint32_t lastTick = 0, lastLog = 0, bootHeld = 0;
  if (now - lastTick >= 250) {
    lastTick = now;
    xSemaphoreTake(lock, portMAX_DELAY);
    tick(now, unixTime());
    xSemaphoreGive(lock);
    saveIndexIfDirty(now);
  }
  // Hold BOOT for 5 seconds: a new group, for when the passphrase is forgotten.
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (!bootHeld) bootHeld = now;
    else if (now - bootHeld > 5000) {
      Serial.println("clip: BOOT held, resetting the group");
      xSemaphoreTake(lock, portMAX_DELAY);
      resetAll();
      xSemaphoreGive(lock);
      saveIndexIfDirty(now, true);
      bootHeld = 0;
      while (digitalRead(BOOT_BUTTON) == LOW) delay(10);
    }
  } else bootHeld = 0;
  if (now - lastLog >= 60000) {
    lastLog = now;
    Serial.printf("clip: %u items, %lu KB sealed, %u devices online, %lu bytes free (largest block %lu)\n", itemCount(),
                  (unsigned long)(usedBytes / 1024), onlineCount(), (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  }
  delay(5);
}
