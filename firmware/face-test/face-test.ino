// Face test: swipe left/right through the retro faces, tap = giggle.
// Board: Waveshare ESP32-C6-Touch-LCD-1.9 — 170x320 ST7789 LCD used sideways, CST816 touch, TCA9554 expander.
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "face.h"

// pins (Waveshare's ESP32-C6-LCD-1.9 demo + our I2C scan)
#define LCD_SCK 5
#define LCD_MOSI 4
#define LCD_CS 7
#define LCD_DC 6
#define LCD_RST 14
#define LCD_BL 15       // backlight is ON when LOW
#define TP_SDA 18
#define TP_SCL 8
#define TOUCH 0x15      // CST816

Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, true /* panel needs colour inversion */, 170, 320, 35, 0, 35, 0);

// one finger: true while down, raw panel coordinates (portrait)
static bool touchRead(int &x, int &y) {
  uint8_t d[8] = {0};
  Wire.beginTransmission(TOUCH); Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TOUCH, (uint8_t)8) != 8) return false;
  for (int i = 0; i < 8; i++) d[i] = Wire.read();
  if (d[1] == 0 || d[1] > 5) return false;
  x = ((d[2] & 0x0F) << 8) | d[3]; y = ((d[4] & 0x0F) << 8) | d[5];
  return true;
}

uint8_t current = F_IDLE;
uint32_t nextBlink, nextBob, giggleUntil = 0, beat = 0;
bool bob = false;

void show(bool blink = false) { face_draw(giggleUntil ? F_GIGGLE : current, blink, bob); }

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("[1] boot");
  if (!gfx->begin()) Serial.println("screen failed");
  gfx->setRotation(1);                       // sideways: 320 x 170
  gfx->fillScreen(0);
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, LOW);
  Serial.printf("[2] screen %d x %d\n", gfx->width(), gfx->height());

  Wire.begin(TP_SDA, TP_SCL); Wire.setTimeOut(20);
  Wire.beginTransmission(TOUCH); Serial.printf("[3] touch %s\n", Wire.endTransmission() == 0 ? "found" : "MISSING");

  if (!face_begin(gfx, gfx->width(), gfx->height())) Serial.println("face canvas failed");
  uint32_t t0 = millis(); show();
  Serial.printf("[4] face drawn in %lu ms\n", millis() - t0);
  nextBlink = millis() + 3000; nextBob = millis() + 1200;
}

bool down = false; int sx, sy, lx, ly;

void loop() {
  int x, y;
  uint32_t t = millis();
  if (t > beat) { beat = t + 3000; Serial.printf("alive face=%s\n", face_name(current)); }

  bool now = touchRead(x, y);
  if (now && !down) { down = true; sx = lx = x; sy = ly = y; }
  else if (now) { lx = x; ly = y; }
  else if (down) {
    down = false;
    // screen is sideways, so a left/right swipe moves along the panel's long (y) side
    int along = ly - sy, across = lx - sx;
    if (abs(along) > 40 && abs(along) > abs(across)) {
      current = along > 0 ? (current + 1) % F_COUNT : (current + F_COUNT - 1) % F_COUNT;
      giggleUntil = 0; show();
      Serial.printf("swipe %+d -> %s\n", along, face_name(current));
    } else if (abs(along) < 20 && abs(across) < 20) {
      giggleUntil = t + 1200; show();
      Serial.println("tap -> giggle");
    }
  }

  if (giggleUntil && t > giggleUntil) { giggleUntil = 0; show(); }
  if (t > nextBob) { bob = !bob; show(); nextBob = t + 1200; }
  if (t > nextBlink && face_can_blink(current) && !giggleUntil) {
    show(true); delay(140); show();
    nextBlink = t + 2500 + random(3000);
  }
  delay(15);
}
