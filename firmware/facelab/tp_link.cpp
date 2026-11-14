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
#include <WebServer.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_mac.h>
#include "secrets.h"
#include "tp_portal.h"

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
static volatile bool inSetup = false, mqttUp = false;
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
  if (len == 0) {                                               // retained message cleared (pod retired / simulator cleared): forget it
    if (!strcmp(pod, partnerId)) { partnerKnown = partnerOnline = false; partnerMood = -1; partnerId[0] = 0; Serial.println("[link] partner forgotten"); }
    return;
  }
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

static void forgetWifi() { Preferences p; p.begin("tamapair", false); p.remove("ssid"); p.remove("pass"); p.end(); }

static void checkResetHold() {
  static uint32_t lowSince = 0;
  if (digitalRead(PIN_BTN) == LOW) { if (!lowSince) lowSince = millis(); else if (millis() - lowSince > 5000) {
      Serial.println("[link] button held 5 s: forgetting Wi-Fi"); forgetWifi(); delay(200); ESP.restart(); } }
  else lowSince = 0;
}

// ---------- Wi-Fi: saved network, else our own cute setup page on the phone (tp_portal.h) ----------
// The Wi-Fi name + password live only in this pod's flash (namespace "tamapair", keys ssid/pass). Never printed.
static String savedSsid, savedPass;

// 0 = connected, 1 = wrong password / refused, 2 = network not found
static int tryJoin(const char *ssid, const char *pass, uint32_t ms, void (*idle)() = nullptr) {
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis(); bool sawNoNet = false;
  while (millis() - t0 < ms) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) return 0;
    if (st == WL_NO_SSID_AVAIL) sawNoNet = true;
    if (idle) idle(); else checkResetHold();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  WiFi.disconnect(false, false);              // stop trying; keeps the setup network up
  return sawNoNet ? 2 : 1;
}

static WebServer *web; static DNSServer *dns;
static volatile int joinState = 0;            // 0 idle, 1 trying, 2 ok, 3 wrong password, 4 network not found
static String joinS, joinP; static volatile bool joinPending = false;
static uint32_t lastHit = 0;

static void jsonStr(String &o, const String &v) {
  o += '"';
  for (char c : v) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if ((uint8_t)c >= 0x20) o += c; }
  o += '"';
}
static void portalPump() { dns->processNextRequest(); web->handleClient(); checkResetHold(); }

static bool runPortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.scanNetworks(true);                     // look around while the phone joins
  WiFi.softAP(apName);
  IPAddress ip = WiFi.softAPIP();
  dns = new DNSServer(); dns->setErrorReplyCode(DNSReplyCode::NoError); dns->start(53, "*", ip);
  web = new WebServer(80);
  web->on("/", [] { lastHit = millis(); web->sendHeader("Cache-Control", "no-store"); web->send_P(200, "text/html", TP_PORTAL_HTML); });
  web->on("/nets", [] {
    lastHit = millis();
    if (web->hasArg("again") && WiFi.scanComplete() != WIFI_SCAN_RUNNING) { WiFi.scanDelete(); WiFi.scanNetworks(true); }
    int n = WiFi.scanComplete();
    String o = "{\"busy\":"; o += (n == WIFI_SCAN_RUNNING) ? "true" : "false"; o += ",\"saved\":"; jsonStr(o, savedSsid); o += ",\"n\":[";
    // strongest first, no duplicates, no hidden networks, at most 10
    int idx[40], k = 0;
    for (int i = 0; i < n && i < 40; i++) if (WiFi.SSID(i).length()) idx[k++] = i;
    for (int i = 0; i < k; i++) for (int j = i + 1; j < k; j++) if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    int shown = 0;
    for (int i = 0; i < k && shown < 10; i++) {
      bool dup = false; for (int j = 0; j < i; j++) if (WiFi.SSID(idx[j]) == WiFi.SSID(idx[i])) dup = true;
      if (dup) continue; if (shown++) o += ','; jsonStr(o, WiFi.SSID(idx[i]));
    }
    o += "]}"; web->send(200, "application/json", o);
  });
  web->on("/join", HTTP_POST, [] {
    lastHit = millis();
    joinS = web->arg("s"); joinP = web->arg("p"); joinS.trim();
    if (!joinS.length() || joinS.length() > 32 || joinP.length() > 63) { web->send(400, "application/json", "{\"st\":\"fail\"}"); return; }
    joinState = 1; joinPending = true; web->send(200, "application/json", "{\"st\":\"trying\"}");
  });
  web->on("/status", [] {
    lastHit = millis();
    static const char *ST[] = {"idle", "trying", "ok", "fail", "nonet"};
    String o = "{\"st\":\""; o += ST[joinState]; o += "\"}"; web->send(200, "application/json", o);
  });
  // phones probe these to spot a sign-in network; send everything to our page so it pops up by itself
  web->onNotFound([] { web->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/"); web->send(302, "text/plain", ""); });
  web->begin();
  inSetup = true; lastHit = millis();
  Serial.printf("[link] setup mode: join Wi-Fi \"%s\" from a phone\n", apName);

  bool ok = false;
  for (;;) {
    portalPump();
    if (joinPending) {
      joinPending = false;
      Serial.println("[link] trying the Wi-Fi from the phone");
      for (int i = 0; i < 20; i++) { portalPump(); vTaskDelay(pdMS_TO_TICKS(10)); } // let the "trying" reply go out
      int r = tryJoin(joinS.c_str(), joinP.c_str(), 15000, portalPump);
      joinState = r == 0 ? 2 : r == 1 ? 3 : 4;
      if (r == 0) {
        Preferences p; p.begin("tamapair", false); p.putString("ssid", joinS); p.putString("pass", joinP); p.end();
        savedSsid = joinS; savedPass = joinP; ok = true;
        uint32_t t0 = millis(); while (millis() - t0 < 4000) { portalPump(); vTaskDelay(pdMS_TO_TICKS(10)); } // let the phone see "yay"
        break;
      }
      Serial.println(r == 1 ? "[link] that didn't work (password?)" : "[link] can't see that network");
    }
    // nobody on the setup page for 2 min but we know a network (e.g. the router was just rebooting): try it again
    if (savedSsid.length() && millis() - lastHit > 120000) {
      lastHit = millis();
      if (tryJoin(savedSsid.c_str(), savedPass.c_str(), 15000, portalPump) == 0) { ok = true; break; }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  web->stop(); dns->stop(); delete web; delete dns; web = nullptr; dns = nullptr;
  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
  inSetup = false;
  return ok;
}

static void netTask(void *) {
  stage = 1;
  // 1) Wi-Fi: the network saved by our setup page, else open the setup page (tamapair-XXXX)
  WiFi.persistent(false);                   // we keep the network ourselves; ignore anything older firmware saved
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); stage = 2;
  { Preferences p; p.begin("tamapair", true); savedSsid = p.getString("ssid", ""); savedPass = p.getString("pass", ""); p.end(); }
  stage = 3;
  bool ok = savedSsid.length() && tryJoin(savedSsid.c_str(), savedPass.c_str(), 20000) == 0;
  if (!ok) runPortal();
  stage = 4;
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
    if (WiFi.status() != WL_CONNECTED) { mqttUp = false; vTaskDelay(pdMS_TO_TICKS(500)); continue; } // Wi-Fi reconnects on its own
    mqttUp = mqtt.connected();
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
int tp_link_state() { // for the settings screen: 0 no Wi-Fi yet, 1 connecting, 2 online, 3 online + partner on
  if (inSetup || stage < 4) return inSetup ? 0 : 1;
  if (WiFi.status() != WL_CONNECTED || !mqttUp) return 1;
  return partnerOnline ? 3 : 2;
}
const char *tp_setup_ap() { return inSetup ? apName : nullptr; } // non-null while waiting for Wi-Fi setup from a phone
void tp_print_status() { // serial 'k'
  Serial.printf("[link] stage %d pod %s setup=%d wifi=%d mailbox=%d partner=%s known=%d online=%d mood=%d\n", (int)stage, podId, (int)inSetup,
                WiFi.status() == WL_CONNECTED, mqtt.connected(), partnerId[0] ? partnerId : "-", (int)partnerKnown, (int)partnerOnline, partnerMood);
  if (inSetup) Serial.printf("[link] waiting for setup: join Wi-Fi \"%s\" from a phone\n", apName);
}
