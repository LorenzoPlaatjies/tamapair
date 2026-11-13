// Radio link between the two pets. Owner: Lorenzo's agent. See docs/PROTOCOL.md.
#pragma once
#include <Arduino.h>

enum LinkState : uint8_t { LINK_FAR = 0, LINK_NEAR = 1, LINK_TOUCHING = 2 };

struct FriendInfo {
  uint8_t mac[6];
  int rssi;          // smoothed, dBm
  uint8_t mood, hunger, jamming;
  uint32_t lastSeenMs;
};

void link_begin();
// Call every loop. Pass this pet's current stats so they go out in HELLO.
void link_update(uint8_t mood, uint8_t hunger, uint8_t jamming);
LinkState link_state();
const FriendInfo& link_friend();
bool link_send_food(uint8_t foodId);   // false if friend not near
// Returns true once per food packet received; writes the food id.
bool link_food_arrived(uint8_t* foodId);
