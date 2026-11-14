// tp_near.cpp — how close are the two pods? Direct radio (ESP-NOW) next to the Wi-Fi mailbox (tp_link.cpp).
// Ported from firmware/tamapair/link.cpp (Lorenzo's agent) into hooks facelab calls. Written by Dr. Smith's agent.
//
// Each pod broadcasts a tiny HELLO 5x a second on whatever channel its Wi-Fi is on. How loud the partner's HELLO
// arrives (RSSI) = how close it is: far / near (same room) / touching (side by side). Different Wi-Fi channels = far.
// HELLO also carries: "I'm being tickled", a heart-toss counter, and a shared clock so both pods dance on the same beat.
//
// Hooks (weak stubs in facelab.ino):
//   tp_near_tick()            call every frame
//   tp_near()                 0 far, 1 near, 2 touching (already steadied: needs 0.6 s to go up, 1.5-6 s to go down)
//   tp_beat_ms()              a clock both pods agree on (the pod with the lower MAC leads)
//   tp_near_flags(tickling)   tell the partner I'm being tickled      tp_partner_tickled()  is the partner being tickled?
//   tp_near_heart()           toss a heart across                     tp_heart_arrived()    true once per heart that landed
//   tp_near_print()           serial 'j': state + signal strength (for calibrating the thresholds below)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include "secrets.h"

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint32_t HELLO_MS = 200, LOST_MS = 2500;
// Signal strength in dBm (closer to 0 = closer together). CALIBRATE with both boards: serial 'j' prints the live number.
static const int NEAR_IN = -70, NEAR_OUT = -76;      // same room
static const int TOUCH_IN = -42, TOUCH_OUT = -50;    // side by side
static const uint32_t UP_MS = 600, DOWN_TOUCH_MS = 1500, DOWN_NEAR_MS = 6000; // how long a change must last before we believe it

struct __attribute__((packed)) Hello { char magic[2]; uint8_t type; uint16_t pair; uint8_t flags, heartSeq; uint32_t clock; };
static const uint8_t PKT_HELLO2 = 4;                 // types 1-3 are the old protocol (docs/PROTOCOL.md)

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static bool up = false, iLead = true, dirty = false, myTickle = false;
static uint8_t myMac[6], myHeart = 0;
static uint16_t pairTag = 0;
static uint32_t lastHello = 0, nextInit = 2000;
// written by the radio callback, read by the frame loop (under mux)
static volatile bool have = false;
static float rssiS = -100; static uint32_t seenMs = 0; static uint8_t pFlags = 0, pHeart = 0; static int heartsDue = 0;
static int32_t clockOff = 0;
static int state = 0, pend = 0; static uint32_t pendSince = 0;

static uint16_t tag16(const char *s) { uint32_t h = 2166136261u; for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; } h ^= 0x7a11; h *= 16777619u; return (uint16_t)(h ^ (h >> 16)); }

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < (int)sizeof(Hello)) return;
  Hello h; memcpy(&h, data, sizeof h);
  if (h.magic[0] != 'T' || h.magic[1] != 'P' || h.type != PKT_HELLO2 || h.pair != pairTag) return;   // only our pair
  if (!memcmp(info->src_addr, myMac, 6)) return;
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -100;
  uint32_t now = millis();
  bool theyLead = memcmp(info->src_addr, myMac, 6) < 0;
  portENTER_CRITICAL(&mux);
  bool fresh = !have || now - seenMs > LOST_MS;
  rssiS = fresh ? rssi : rssiS * .7f + rssi * .3f;
  if (!fresh) { uint8_t d = h.heartSeq - pHeart; if (d && d < 4) heartsDue += d; }
  pHeart = h.heartSeq; pFlags = h.flags; seenMs = now; have = true; iLead = !theyLead;
  if (theyLead) { int32_t off = (int32_t)(h.clock - now), d = off - clockOff; clockOff = (fresh || d > 40 || d < -40) ? off : clockOff + d / 4; }
  else clockOff = 0;
  portEXIT_CRITICAL(&mux);
}

uint32_t tp_beat_ms() { return millis() + (uint32_t)clockOff; }

static void hello() {
  Hello h = {{'T', 'P'}, PKT_HELLO2, pairTag, (uint8_t)(myTickle ? 1 : 0), myHeart, tp_beat_ms()};
  esp_now_send(BCAST, (const uint8_t *)&h, sizeof h);
}

void tp_near_tick() {
  uint32_t now = millis();
  if (!up) {   // start once Wi-Fi is running (tp_link.cpp turns it on in its own task)
    if (now < nextInit) return;
    nextInit = now + 1000;
    wifi_mode_t m; if (esp_wifi_get_mode(&m) != ESP_OK || m == WIFI_MODE_NULL) return;
    if (esp_now_init() != ESP_OK) { Serial.println("[near] esp_now_init failed, will retry"); return; }
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t peer = {}; memcpy(peer.peer_addr, BCAST, 6); peer.channel = 0; peer.ifidx = WIFI_IF_STA; peer.encrypt = false;
    esp_now_add_peer(&peer);
    esp_wifi_set_ps(WIFI_PS_NONE);                     // with power save on, the radio naps and misses most HELLOs
    esp_read_mac(myMac, ESP_MAC_WIFI_STA); pairTag = tag16(TP_PAIR_ID);
    up = true; Serial.println("[near] radio up");
  }
  if (dirty || now - lastHello >= HELLO_MS) { if (dirty) hello(); dirty = false; lastHello = now; hello(); }

  bool h; float r; uint32_t seen;
  portENTER_CRITICAL(&mux); h = have; r = rssiS; seen = seenMs; portEXIT_CRITICAL(&mux);
  int want;
  bool lost = !h || now - seen > LOST_MS;
  if (lost) want = 0;
  else if (state == 2) want = r < TOUCH_OUT ? (r < NEAR_OUT ? 0 : 1) : 2;
  else if (r > TOUCH_IN) want = 2;
  else if (state == 1) want = r < NEAR_OUT ? 0 : 1;
  else want = r > NEAR_IN ? 1 : 0;
  if (want == state) { pend = state; return; }
  if (want != pend) { pend = want; pendSince = now; }
  uint32_t dwell = lost ? 0 : want > state ? UP_MS : state == 2 ? DOWN_TOUCH_MS : DOWN_NEAR_MS;
  if (now - pendSince >= dwell) {
    state = want; pend = want;
    Serial.printf("[near] %s (signal %d)\n", state == 0 ? "far" : state == 1 ? "near" : "touching", (int)r);
  }
}

int tp_near() { return state; }
void tp_near_flags(bool tickling) { if (tickling != myTickle) { myTickle = tickling; dirty = true; } }
void tp_near_heart() { myHeart++; dirty = true; }
bool tp_partner_tickled() {
  portENTER_CRITICAL(&mux); bool t = have && (pFlags & 1) && millis() - seenMs < 700; portEXIT_CRITICAL(&mux); return t;
}
bool tp_heart_arrived() {
  portENTER_CRITICAL(&mux); bool got = heartsDue > 0; if (got) heartsDue--; portEXIT_CRITICAL(&mux); return got;
}
void tp_near_print() {
  bool h; float r; uint32_t seen; int32_t off;
  portENTER_CRITICAL(&mux); h = have; r = rssiS; seen = seenMs; off = clockOff; portEXIT_CRITICAL(&mux);
  uint8_t ch = 0; wifi_second_chan_t sc; esp_wifi_get_channel(&ch, &sc);
  Serial.printf("[near] up=%d state=%d heard=%d signal=%d last=%lums ago channel=%d lead=%d clockoff=%ld\n", (int)up, state, (int)h, (int)r,
                h ? (unsigned long)(millis() - seen) : 0UL, (int)ch, (int)iLead, (long)off);
}
