#include "link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint32_t HELLO_EVERY_MS = 200;
static const uint32_t LOST_AFTER_MS = 2000;
// RSSI thresholds — calibrate on real boards.
static const int RSSI_NEAR = -75;
static const int RSSI_TOUCH = -50;
static const int HYST = 4;

enum : uint8_t { PKT_HELLO = 1, PKT_FOOD = 2, PKT_ACK = 3 };

struct __attribute__((packed)) Packet {
  char magic[2];
  uint8_t type;
  uint8_t a, b, c;
};

static FriendInfo fr = {};
static volatile bool haveFriend = false;
static volatile bool foodPending = false;
static volatile uint8_t foodPendingId = 0;
static LinkState state = LINK_FAR;
static uint32_t lastHello = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < (int)sizeof(Packet)) return;
  const Packet* p = (const Packet*)data;
  if (p->magic[0] != 'T' || p->magic[1] != 'P') return;
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -100;

  portENTER_CRITICAL_ISR(&mux);
  memcpy(fr.mac, info->src_addr, 6);
  fr.lastSeenMs = millis();
  if (p->type == PKT_HELLO) {
    fr.rssi = haveFriend ? (fr.rssi * 3 + rssi) / 4 : rssi;  // smooth
    fr.mood = p->a; fr.hunger = p->b; fr.jamming = p->c;
    haveFriend = true;
  } else if (p->type == PKT_FOOD) {
    foodPendingId = p->a;
    foodPending = true;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

static void sendPkt(uint8_t type, uint8_t a, uint8_t b = 0, uint8_t c = 0) {
  Packet p = {{'T', 'P'}, type, a, b, c};
  esp_now_send(BCAST, (const uint8_t*)&p, sizeof(p));
}

void link_begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("[link] esp_now_init failed"); return; }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  Serial.printf("[link] up, me=%s\n", WiFi.macAddress().c_str());
}

void link_update(uint8_t mood, uint8_t hunger, uint8_t jamming) {
  uint32_t now = millis();
  if (now - lastHello >= HELLO_EVERY_MS) {
    lastHello = now;
    sendPkt(PKT_HELLO, mood, hunger, jamming);
  }
  int rssi; uint32_t seen; bool have;
  portENTER_CRITICAL(&mux);
  rssi = fr.rssi; seen = fr.lastSeenMs; have = haveFriend;
  portEXIT_CRITICAL(&mux);

  LinkState s = state;
  if (!have || now - seen > LOST_AFTER_MS) s = LINK_FAR;
  else if (state == LINK_TOUCHING) s = rssi < RSSI_TOUCH - HYST ? (rssi < RSSI_NEAR ? LINK_FAR : LINK_NEAR) : LINK_TOUCHING;
  else if (rssi > RSSI_TOUCH) s = LINK_TOUCHING;
  else if (state == LINK_NEAR) s = rssi < RSSI_NEAR - HYST ? LINK_FAR : LINK_NEAR;
  else s = rssi > RSSI_NEAR ? LINK_NEAR : LINK_FAR;

  if (s != state) Serial.printf("[link] %s (rssi %d)\n", s == 0 ? "FAR" : s == 1 ? "NEAR" : "TOUCHING", rssi);
  state = s;
}

LinkState link_state() { return state; }
const FriendInfo& link_friend() { return fr; }

bool link_send_food(uint8_t foodId) {
  if (state == LINK_FAR) return false;
  sendPkt(PKT_FOOD, foodId);
  return true;
}

bool link_food_arrived(uint8_t* foodId) {
  bool got = false;
  portENTER_CRITICAL(&mux);
  if (foodPending) { *foodId = foodPendingId; foodPending = false; got = true; }
  portEXIT_CRITICAL(&mux);
  return got;
}
