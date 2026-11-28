// Phone Pad: turn your phone into a build-your-own game controller for your PC. The board is the
// bridge — WiFi to your phone, Bluetooth to the PC, where it looks like a real keyboard + mouse +
// gamepad (no driver, no app). You design the controller on the phone (buttons, d-pad, sticks, a
// trackpad, tilt steering), map each widget to a key or a gamepad control, and play.
//
//   your finger ── WiFi (WebSocket) ──► ESP32 ── Bluetooth HID ──► PC
//
// Pair it once: on the PC, Bluetooth settings → add device → "AirController". Then open the page.
//
// The input→HID logic is in pad.h (tested on a laptop). This file is the Bluetooth HID device, the
// WebSocket that carries the taps, and the flash that holds your saved layouts.
//
// Board: ESP32 DevKit V1, nothing attached. Partition: "No OTA (2MB APP/2MB SPIFFS)".

#include <esp_http_server.h>
#undef HTTP_ANY
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include "ui.h"
#include "improv.h"
#include "wifi_setup.h"
#include "pad.h"
#include "pad_page.h"

const char *HOST = "air";
const char *BT_NAME = "AirController";

WebServer web(80);
static httpd_handle_t wsServer = nullptr;
static SemaphoreHandle_t lock;
static HidState hid;

// ── composite HID: a keyboard (report 1), a mouse (report 2) and a gamepad (report 3) ──
static const uint8_t HID_MAP[] = {
  // keyboard
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
  // mouse
  0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02,
  0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
  0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0,
  // gamepad: 4 axes, a hat, 16 buttons
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x03,
  0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02,
  0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
  0x75, 0x04, 0x95, 0x01, 0x81, 0x01,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02, 0xC0
};

static BLEHIDDevice *ble = nullptr;
static BLECharacteristic *repKbd = nullptr, *repMouse = nullptr, *repGp = nullptr;
static volatile bool btConnected = false, justConnected = false;
static volatile uint32_t msgs = 0;

class SrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer *) { btConnected = true; justConnected = true; Serial.println("pad: PC connected over Bluetooth"); }
  void onDisconnect(BLEServer *) { btConnected = false; Serial.println("pad: PC disconnected"); BLEDevice::startAdvertising(); }
};
class SecCb : public BLESecurityCallbacks {
  bool onSecurityRequest() { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t c) { Serial.println(c.success ? "pad: paired" : "pad: pairing failed"); }
  uint32_t onPassKeyRequest() { return 0; }
  void onPassKeyNotify(uint32_t) {}
  bool onConfirmPIN(uint32_t) { return true; }
};

static void flushReports() {
  if (!btConnected) return;
  uint8_t buf[9];
  if (hid.kbdDirty && repKbd) { int n = hid.keyboardReport(buf); repKbd->setValue(buf, n); repKbd->notify(); }
  if (hid.gpDirty && repGp) { int n = hid.gamepadReport(buf); repGp->setValue(buf, n); repGp->notify(); }
  if (hid.mouseDirty && repMouse) { int n = hid.mouseReport(buf); repMouse->setValue(buf, n); repMouse->notify(); }
}

static void startBluetooth() {
  BLEDevice::init(BT_NAME);
  BLESecurity *sec = new BLESecurity();
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setAuthenticationMode(true, false, true);
  BLEDevice::setSecurityCallbacks(new SecCb());
  BLEServer *s = BLEDevice::createServer();
  s->setCallbacks(new SrvCb());
  ble = new BLEHIDDevice(s);
  ble->manufacturer()->setValue("devkit-diary");
  ble->pnp(0x02, 0xE502, 0xA111, 0x0210);
  ble->hidInfo(0x00, 0x01);
  ble->reportMap((uint8_t *)HID_MAP, sizeof HID_MAP);
  repKbd = ble->inputReport(1);
  repMouse = ble->inputReport(2);
  repGp = ble->inputReport(3);
  ble->setBatteryLevel(100);
  ble->startServices();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C0);                 // generic HID
  adv->addServiceUUID(ble->hidService()->getUUID());
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  Serial.printf("pad: Bluetooth \"%s\" ready — pair it on the PC\n", BT_NAME);
}

// ── WebSocket: the phone's taps arrive here, a few bytes each ──
static esp_err_t wsHandler(httpd_req_t *req) {
  if (req->method == HTTP_GET) { xSemaphoreTake(lock, portMAX_DELAY); hid.reset(); flushReports(); xSemaphoreGive(lock); return ESP_OK; }   // the handshake: start clean
  httpd_ws_frame_t f = {};
  f.type = HTTPD_WS_TYPE_TEXT;
  if (httpd_ws_recv_frame(req, &f, 0) != ESP_OK) return ESP_FAIL;
  if (f.len == 0 || f.len > 480) return ESP_OK;
  static uint8_t buf[512];
  f.payload = buf;
  if (httpd_ws_recv_frame(req, &f, sizeof buf - 1) != ESP_OK) return ESP_FAIL;
  buf[f.len] = 0;
  xSemaphoreTake(lock, portMAX_DELAY);
  hid.applyLine((char *)buf);
  flushReports();
  xSemaphoreGive(lock);
  msgs++;
  return ESP_OK;
}

static void startWs() {
  httpd_config_t c = HTTPD_DEFAULT_CONFIG();
  c.server_port = 81;
  c.ctrl_port = 32780;
  c.lru_purge_enable = true;
  if (httpd_start(&wsServer, &c) != ESP_OK) { Serial.println("pad: WebSocket server did not start"); return; }
  static const httpd_uri_t ws = {"/ws", HTTP_GET, wsHandler, nullptr, true};
  httpd_register_uri_handler(wsServer, &ws);
}

// ── saved layouts, in flash ──
static void sendLayouts() {
  String o = "[";
  File dir = LittleFS.open("/lay");
  if (dir) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      if (n.endsWith(".json")) { if (o.length() > 1) o += ','; o += '"' + uiJson(n.substring(0, n.length() - 5).c_str()) + '"'; }
    }
  }
  o += ']';
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", o);
}
static String layPath(const String &name) {
  String safe;
  for (size_t i = 0; i < name.length() && safe.length() < 32; i++) { char c = name[i]; if (isalnum(c) || c == '-' || c == '_' || c == ' ') safe += c; }
  safe.trim();
  return "/lay/" + safe + ".json";
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\npad: starting");
  lock = xSemaphoreCreateMutex();
  if (!LittleFS.begin(true)) Serial.println("pad: flash files could not be mounted");
  if (!LittleFS.exists("/lay")) LittleFS.mkdir("/lay");

  const wifisetup::Config cfg = {HOST, "devkit-air", "devkit-air", "15", false};
  const bool online = wifisetup::begin(cfg, web);
  startBluetooth();                            // Bluetooth runs even while WiFi is still being set up
  if (!online) return;
  startWs();

  web.on("/", [] { web.sendHeader("Cache-Control", "no-store"); web.send_P(200, "text/html; charset=utf-8", PAD_PAGE); });
  web.on("/status", [] {
    char b[96];
    snprintf(b, sizeof b, "{\"ok\":true,\"paired\":%s,\"msgs\":%lu,\"bt\":\"%s\"}", btConnected ? "true" : "false",
             (unsigned long)msgs, BT_NAME);
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", b);
  });
  web.on("/layouts", sendLayouts);
  web.on("/layout", [] {                       // GET ?name=  returns the JSON; POST name,data saves
    if (web.hasArg("data")) {
      File f = LittleFS.open(layPath(web.arg("name")), "w");
      if (f) { f.print(web.arg("data")); f.close(); web.send(200, "application/json", "{\"ok\":true}"); }
      else web.send(200, "application/json", "{\"ok\":false}");
    } else {
      File f = LittleFS.open(layPath(web.arg("name")), "r");
      if (!f) { web.send(404, "application/json", "{\"ok\":false}"); return; }
      web.streamFile(f, "application/json");
      f.close();
    }
  });
  web.on("/dellayout", [] { LittleFS.remove(layPath(web.arg("name"))); web.send(200, "application/json", "{\"ok\":true}"); });
  web.on("/release", [] {                      // the page calls this when it closes, so nothing sticks
    xSemaphoreTake(lock, portMAX_DELAY); hid.reset(); flushReports(); xSemaphoreGive(lock);
    web.sendHeader("Cache-Control", "no-store"); web.send(200, "application/json", "{\"ok\":true}");
  });
  web.on("/favicon.ico", [] { web.send(204); });
  web.onNotFound([] { web.send(404, "text/plain", "not here"); });
  web.begin();
  Serial.printf("pad: open http://%s.local or http://%s on your phone\n", HOST, WiFi.localIP().toString().c_str());
}

void loop() {
  wifisetup::loop();
  web.handleClient();
  if (justConnected) { justConnected = false; xSemaphoreTake(lock, portMAX_DELAY); hid.reset(); flushReports(); xSemaphoreGive(lock); }
  // Flush any leftover mouse travel (a move bigger than one report can carry).
  if (hid.mouseDirty && btConnected) { xSemaphoreTake(lock, portMAX_DELAY); flushReports(); xSemaphoreGive(lock); }
  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 60000) {
    lastLog = millis();
    Serial.printf("pad: Bluetooth %s, %lu messages, %lu bytes free\n", btConnected ? "connected" : "waiting",
                  (unsigned long)msgs, (unsigned long)ESP.getFreeHeap());
  }
  delay(1);
}
