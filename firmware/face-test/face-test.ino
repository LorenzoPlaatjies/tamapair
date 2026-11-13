// Face test: swipe left/right through the retro faces, tap = giggle.
// Board: Waveshare ESP32-C6-Touch-AMOLED-1.8 (368x448, SH8601, FT3168 touch).
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "face.h"

// pins (from Waveshare's pin_config.h)
#define LCD_CS 5
#define LCD_SCLK 0
#define LCD_D0 1
#define LCD_D1 2
#define LCD_D2 3
#define LCD_D3 4
#define LCD_W 368
#define LCD_H 448
#define I2C_SDA 8
#define I2C_SCL 7
#define EXPANDER 0x20   // TCA9554: pins 4+5 power the screen/touch
#define TOUCH 0x38      // FT3168

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_SH8601 *gfx = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, LCD_W, LCD_H);
Arduino_Canvas *label;

static uint8_t reg(uint8_t dev, uint8_t r) {
  Wire.beginTransmission(dev); Wire.write(r); Wire.endTransmission(false);
  Wire.requestFrom(dev, (uint8_t)1); return Wire.available() ? Wire.read() : 0;
}
static void setReg(uint8_t dev, uint8_t r, uint8_t v) {
  Wire.beginTransmission(dev); Wire.write(r); Wire.write(v); Wire.endTransmission();
}

// returns true while a finger is down, with its position
static bool touchRead(int &x, int &y) {
  Wire.beginTransmission(TOUCH); Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)TOUCH, (uint8_t)5);
  if (Wire.available() < 5) return false;
  uint8_t n = Wire.read() & 0x0F, xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (n == 0 || n > 2) return false;
  x = ((xh & 0x0F) << 8) | xl; y = ((yh & 0x0F) << 8) | yl;
  return true;
}

uint8_t current = F_IDLE;
uint32_t nextBlink, nextBob, giggleUntil = 0;
bool bob = false;

void drawLabel() {
  label->fillScreen(0);
  label->setTextColor(0x9CD4); label->setTextSize(3);
  char buf[40]; snprintf(buf, sizeof buf, "%s  %d/%d", face_name(current), current + 1, F_COUNT);
  int16_t bx, by; uint16_t bw, bh; label->getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
  label->setCursor((360 - bw) / 2, 6); label->print(buf);
  gfx->draw16bitRGBBitmap(4, 380, label->getFramebuffer(), 360, 36);
}

void show(bool blink = false) {
  uint8_t id = millis() < giggleUntil ? F_GIGGLE : current;
  face_draw(id, blink, bob);
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("[1] boot");
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);
  Serial.print("[2] i2c devices:");
  for (uint8_t a = 1; a < 127; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a); }
  Serial.println();
  Serial.printf("[2] expander cfg=0x%02X out=0x%02X\n", reg(EXPANDER, 0x03), reg(EXPANDER, 0x01));
  setReg(EXPANDER, 0x03, reg(EXPANDER, 0x03) & ~0x30);   // pins 4,5 → outputs
  setReg(EXPANDER, 0x01, reg(EXPANDER, 0x01) | 0x30);    // …and on
  delay(50);
  Serial.println("[3] screen begin");
  if (!gfx->begin()) Serial.println("screen failed");
  Serial.println("[4] screen ok");
  gfx->fillScreen(0);
  gfx->setBrightness(200);
  label = new Arduino_Canvas(360, 36, nullptr);
  label->begin(GFX_SKIP_OUTPUT_BEGIN);
  if (!face_begin(gfx, 4, 82)) Serial.println("face canvas failed");
  Serial.println("[5] first face");
  uint32_t t0 = millis(); show(); drawLabel();
  Serial.printf("[6] face drawn in %lu ms\n", millis() - t0);
  nextBlink = millis() + 3000; nextBob = millis() + 1200;
  Serial.printf("touch id 0x%02X\n", reg(TOUCH, 0xA8));
}

bool down = false; int sx, sy, lx, ly; uint32_t downAt;

uint32_t beat = 0;
void loop() {
  int x, y;
  if (millis() > beat) { beat = millis() + 2000; Serial.printf("alive face=%s touchStatus=0x%02X\n", face_name(current), reg(TOUCH, 0x02)); }
  bool now = touchRead(x, y);
  if (now && !down) { down = true; sx = lx = x; sy = ly = y; downAt = millis(); }
  else if (now) { lx = x; ly = y; }
  else if (!now && down) {
    down = false;
    int dx = lx - sx, dy = ly - sy;
    if (abs(dx) > 50 && abs(dx) > abs(dy)) {            // swipe
      current = dx < 0 ? (current + 1) % F_COUNT : (current + F_COUNT - 1) % F_COUNT;
      giggleUntil = 0; show(); drawLabel();
    } else if (abs(dx) < 25 && abs(dy) < 25) {           // tap
      giggleUntil = millis() + 1200; show();
    }
    Serial.printf("touch end dx=%d dy=%d face=%s\n", dx, dy, face_name(current));
  }

  uint32_t t = millis();
  if (giggleUntil && t > giggleUntil) { giggleUntil = 0; show(); }
  if (t > nextBob) { bob = !bob; show(); nextBob = t + 1200; }
  if (t > nextBlink && face_can_blink(current) && !giggleUntil) {
    show(true); delay(140); show();
    nextBlink = t + 2500 + random(3000);
  }
  delay(15);
}
