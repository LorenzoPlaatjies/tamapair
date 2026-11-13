// tp_link — pods talk from anywhere through a private MQTT mailbox. Owner: Lorenzo's agent. Spec: docs/LINK_SPEC.md.
//
// Implements the hooks facelab calls (strong versions override the weak stubs in facelab.ino):
//   tp_begin(offline), tp_send_mood, tp_send_food, tp_partner_mood, tp_food_arrived, tp_setup_ap
// All network work runs in its own FreeRTOS task; the 30 fps frame loop only touches queues/atomics.
//
// Topics: tp/<pairId>/<podId>/{mood,food,status}. podId = this board's MAC (12 hex), so both pods run
// identical firmware. Each pod publishes under its own id and listens to tp/<pairId>/+/... ignoring itself.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_mac.h>
#include "secrets.h"

#ifndef TP_PAIR_ID
#error "firmware/facelab/secrets.h missing: copy secrets.example.h to secrets.h and fill it in"
#endif

static const int PIN_BTN = 9;               // side button: hold 5 s while running = forget Wi-Fi and restart
static const uint32_t KEEPALIVE_S = 10;     // broker fires our "off" will ~15 s after we vanish

struct OutMsg { uint8_t kind; uint8_t val; }; // kind 0 = mood, 1 = food
static QueueHandle_t outQ, foodQ;
static volatile int partnerMood = -1;       // last mood index the partner published (retained)
static volatile bool partnerOnline = false, partnerKnown = false;
static uint8_t offlineMood = 0;
static char podId[13], partnerId[13] = "", apName[20] = "";
static volatile bool inSetup = false;
static volatile int stage = 0; // 1 task up, 2 wifi mode, 3 autoConnect, 4 wifi ok, 5 mailbox loop
static char seenFood[8][8]; static int seenIdx = 0;

static WiFiClientSecure net;
static PubSubClient mqtt(net);

static void topic(char *out, size_t n, const char *pod, const char *what) { snprintf(out, n, "tp/%s/%s/%s", TP_PAIR_ID, pod, what); }

static void onMessage(char *t, byte *p, unsigned int len) {
  // tp/<pair>/<pod>/<what>
  char pod[24] = "", what[12] = "";
  const char *a = strchr(t + 3, '/'); if (!a) return; a++;
  const char *b = strchr(a, '/'); if (!b) return;
  size_t pl = min((size_t)(b - a), sizeof(pod) - 1); memcpy(pod, a, pl); pod[pl] = 0;
  strncpy(what, b + 1, sizeof(what) - 1);
  if (!strcmp(pod, podId)) return;                              // our own echo
  char msg[96]; size_t ml = min((size_t)len, sizeof(msg) - 1); memcpy(msg, p, ml); msg[ml] = 0;
  strncpy(partnerId, pod, sizeof(partnerId) - 1);
  if (!strcmp(what, "status")) {
    partnerOnline = !strcmp(msg, "on"); partnerKnown = true;
    Serial.printf("[link] partner %s is %s\n", pod, partnerOnline ? "online" : "OFFLINE");
  } else if (!strcmp(what, "mood")) {
    const char *m = strstr(msg, "\"m\":"); if (!m) return;
    partnerMood = atoi(m + 4); partnerKnown = true;
    Serial.printf("[link] partner mood %d\n", partnerMood);
  } else if (!strcmp(what, "food")) {
    const char *f = strstr(msg, "\"f\":"), *id = strstr(msg, "\"id\":\""); if (!f) return;
    char fid[8] = ""; if (id) { strncpy(fid, id + 6, 6); fid[6] = 0; }
    for (auto &s : seenFood) if (fid[0] && !strcmp(s, fid)) return; // duplicate delivery (QoS 1): eat once
    if (fid[0]) { strcpy(seenFood[seenIdx], fid); seenIdx = (seenIdx + 1) % 8; }
    uint8_t v = atoi(f + 4); xQueueSend(foodQ, &v, 0);
    Serial.printf("[link] food %u arrived\n", v);
  }
}

static bool mqttConnect() {
  char will[96]; topic(will, sizeof will, podId, "status");
  char cid[32]; snprintf(cid, sizeof cid, "tp-%s-%04x", podId, (unsigned)(esp_random() & 0xffff));
  const char *user = strlen(TP_MQTT_USER) ? TP_MQTT_USER : nullptr, *pass = strlen(TP_MQTT_PASS) ? TP_MQTT_PASS : nullptr;
  if (!mqtt.connect(cid, user, pass, will, 1, true, "off")) { Serial.printf("[link] mqtt connect failed (%d)\n", mqtt.state()); return false; }
  mqtt.publish(will, "on", true);
  char sub[96];
  snprintf(sub, sizeof sub, "tp/%s/+/status", TP_PAIR_ID); mqtt.subscribe(sub, 1);
  snprintf(sub, sizeof sub, "tp/%s/+/mood", TP_PAIR_ID);   mqtt.subscribe(sub, 1);
  snprintf(sub, sizeof sub, "tp/%s/+/food", TP_PAIR_ID);   mqtt.subscribe(sub, 1);
  Serial.println("[link] mailbox connected");
  return true;
}

static void sendOut(const OutMsg &o) {
  char t[96], body[48];
  if (o.kind == 0) { topic(t, sizeof t, podId, "mood"); snprintf(body, sizeof body, "{\"m\":%u}", o.val); mqtt.publish(t, body, true); }
  else {
    static const char AL[] = "abcdefghijkmnpqrstuvwxyz23456789"; char id[7];
    for (int i = 0; i < 6; i++) id[i] = AL[esp_random() % (sizeof(AL) - 1)]; id[6] = 0;
    topic(t, sizeof t, podId, "food"); snprintf(body, sizeof body, "{\"f\":%u,\"id\":\"%s\"}", o.val, id); mqtt.publish(t, body, false);
  }
  Serial.printf("[link] sent %s %u\n", o.kind ? "food" : "mood", o.val);
}

static void checkResetHold() {
  static uint32_t lowSince = 0;
  if (digitalRead(PIN_BTN) == LOW) { if (!lowSince) lowSince = millis(); else if (millis() - lowSince > 5000) {
      Serial.println("[link] button held 5 s: forgetting Wi-Fi"); WiFiManager wm; wm.resetSettings(); { Preferences p; p.begin("tamapair", false); p.putBool("wifiok", false); p.end(); } delay(200); ESP.restart(); } }
  else lowSince = 0;
}

static void netTask(void *) {
  stage = 1;
  // 1) Wi-Fi: saved network, or a setup portal from a phone (tamapair-XXXX)
  WiFi.mode(WIFI_STA); stage = 2;
  WiFiManager wm;
  wm.setDebugOutput(false);                 // never print Wi-Fi details
  wm.setConfigPortalTimeout(0);
  wm.setAPCallback([](WiFiManager *) { inSetup = true; Serial.printf("[link] setup mode: join Wi-Fi \"%s\" from a phone\n", apName); });
  wm.setTitle("tamapair");
  // keep the phone page to one job: pick your Wi-Fi, type the password, save
  std::vector<const char *> menu = {"wifi"}; wm.setMenu(menu);
  wm.setShowInfoErase(false); wm.setShowInfoUpdate(false); wm.setClass("invert");
  wm.setScanDispPerc(true); wm.setMinimumSignalQuality(10);
  // Wi-Fi saved by anything other than us (e.g. the factory demo) is ignored: straight to setup, no 30 s wait
  Preferences p; p.begin("tamapair", false); bool ours = p.getBool("wifiok", false);
  if (!ours && wm.getWiFiIsSaved()) { Serial.println("[link] ignoring Wi-Fi saved by older firmware"); wm.resetSettings(); }
  stage = 3;
  wm.setConnectTimeout(15); wm.setConnectRetries(2);
  bool ok = false;
  while (!ok) ok = wm.getWiFiIsSaved() ? wm.autoConnect(apName) : wm.startConfigPortal(apName); // nothing saved: straight to phone setup
  stage = 4;
  inSetup = false;
  p.putBool("wifiok", true); p.end();
  Serial.println("[link] Wi-Fi connected");

  // 2) mailbox
  if (strlen(TP_MQTT_CA)) net.setCACert(TP_MQTT_CA);
  else { net.setInsecure(); Serial.println("[link] WARNING: broker certificate not checked (dev config)"); }
  mqtt.setServer(TP_MQTT_HOST, TP_MQTT_PORT);
  mqtt.setKeepAlive(KEEPALIVE_S);
  mqtt.setSocketTimeout(5);
  mqtt.setCallback(onMessage);
  uint32_t nextTry = 0; stage = 5;
  for (;;) {
    checkResetHold();
    if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(500)); continue; } // Wi-Fi reconnects on its own
    if (!mqtt.connected()) {
      if (millis() >= nextTry) { if (!mqttConnect()) nextTry = millis() + 3000; }
      vTaskDelay(pdMS_TO_TICKS(50)); continue;
    }
    mqtt.loop();
    OutMsg o; while (xQueueReceive(outQ, &o, 0) == pdTRUE) sendOut(o);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------- hooks facelab calls ----------
void tp_begin(uint8_t offline) {
  offlineMood = offline;
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA); // burned-in address; WiFi.macAddress() is zero before Wi-Fi starts
  snprintf(podId, sizeof podId, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(apName, sizeof apName, "tamapair-%02X%02X", mac[4], mac[5]);
  outQ = xQueueCreate(8, sizeof(OutMsg)); foodQ = xQueueCreate(8, sizeof(uint8_t));
  pinMode(PIN_BTN, INPUT_PULLUP);
  xTaskCreate(netTask, "tp_net", 12288, nullptr, 1, nullptr);
  Serial.printf("[link] pod %s starting\n", podId);
}
void tp_send_mood(uint8_t mood) { OutMsg o = {0, mood}; if (outQ) xQueueSend(outQ, &o, 0); }
void tp_send_food(uint8_t food) { OutMsg o = {1, food}; if (outQ) xQueueSend(outQ, &o, 0); }
bool tp_partner_mood(uint8_t &mood) {
  if (!partnerKnown) return false;
  if (!partnerOnline) { mood = offlineMood; return true; }   // unplugged / out of Wi-Fi -> offline face
  if (partnerMood < 0) return false;
  mood = (uint8_t)partnerMood; return true;
}
bool tp_food_arrived(uint8_t &food) { return foodQ && xQueueReceive(foodQ, &food, 0) == pdTRUE; }
const char *tp_setup_ap() { return inSetup ? apName : nullptr; } // non-null while waiting for Wi-Fi setup from a phone
void tp_print_status() { // serial 'k'
  Serial.printf("[link] stage %d pod %s setup=%d wifi=%d mailbox=%d partner=%s known=%d online=%d mood=%d\n", (int)stage, podId, (int)inSetup,
                WiFi.status() == WL_CONNECTED, mqtt.connected(), partnerId[0] ? partnerId : "-", (int)partnerKnown, (int)partnerOnline, partnerMood);
  if (inSetup) Serial.printf("[link] waiting for setup: join Wi-Fi \"%s\" from a phone\n", apName);
}
