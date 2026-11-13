// tamapair — main loop. Keep this file thin; logic lives in link.* / pet.* / face.*
#include "link.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("tamapair booting");
  link_begin();
}

void loop() {
  link_update(/*mood*/ 128, /*hunger*/ 50, /*jamming*/ 0);

  uint8_t food;
  if (link_food_arrived(&food)) Serial.printf("[pet] yum! got food %u\n", food);

  static uint32_t t = 0;
  if (millis() - t > 1000) {
    t = millis();
    Serial.printf("[pet] friend rssi=%d state=%d\n", link_friend().rssi, link_state());
  }
  delay(10);
}
