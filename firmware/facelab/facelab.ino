// facelab — the face lab (tools/face-lab) running on the real board.
// Board: Waveshare ESP32-C6-Touch-LCD-1.9, ST7789V2, used landscape 320x170. See docs/BOARD.md.
// Tap = laugh, swipe left/right = next/previous mood. Same timing as docs/FACE_STYLE.md.
#include <Arduino_GFX_Library.h>
#include <Wire.h>

// ---------- hardware ----------
#define PIN_BL 15            // backlight, active LOW
#define PIN_SD_CS 20         // SD shares the SPI bus: keep it deselected
#define I2C_SDA 18
#define I2C_SCL 8
#define TOUCH_ADDR 0x15

Arduino_DataBus *bus = new Arduino_HWSPI(6 /*DC*/, 7 /*CS*/, 5 /*SCK*/, 4 /*MOSI*/);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 14 /*RST*/, 1 /*landscape*/, true /*IPS: colours inverted otherwise*/, 170, 320, 35, 0, 35, 0);

// ---------- pixel grid ----------
static const int W = 80, H = 42, S = 4, OY = 1, SW = 320, SH = 170;
static uint8_t buf[W * H], tmp[W * H];
static uint16_t fb[SW * SH];
static const uint16_t COL[] = {0x0000, 0xF7DF, 0xFB73, 0x6E9F, 0x8C53}; // off, white, pink, blue, grey

static inline void px(int x, int y, uint8_t c) { if (x >= 0 && y >= 0 && x < W && y < H) buf[y * W + x] = c; }
static void rect(int x, int y, int w, int h, uint8_t c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c); }
static void rrect(float cx, float cy, float w, float h, float r, uint8_t c) {
  float x0 = cx - w / 2, y0 = cy - h / 2; r = fminf(r, fminf(w / 2, h / 2));
  for (int y = (int)floorf(y0); y < y0 + h; y++) for (int x = (int)floorf(x0); x < x0 + w; x++) {
    float X = x + .5f, Y = y + .5f;
    float dx = fmaxf(fmaxf(x0 + r - X, 0), X - (x0 + w - r)), dy = fmaxf(fmaxf(y0 + r - Y, 0), Y - (y0 + h - r));
    if (dx * dx + dy * dy <= r * r) px(x, y, c);
  }
}
static void ell(float cx, float cy, float rx, float ry, uint8_t c) {
  for (int y = (int)floorf(cy - ry); y <= cy + ry; y++) for (int x = (int)floorf(cx - rx); x <= cx + rx; x++) {
    float a = (x + .5f - cx) / rx, b = (y + .5f - cy) / ry; if (a * a + b * b <= 1) px(x, y, c);
  }
}
static void ring(float cx, float cy, float r, float th, uint8_t c) {
  for (int y = (int)floorf(cy - r); y <= cy + r; y++) for (int x = (int)floorf(cx - r); x <= cx + r; x++) {
    float d = hypotf(x + .5f - cx, y + .5f - cy); if (d <= r && d >= r - th) px(x, y, c);
  }
}
static void line(float x0, float y0, float x1, float y1, int th, uint8_t c) {
  int n = (int)ceilf(hypotf(x1 - x0, y1 - y0) * 2) + 1;
  for (int i = 0; i <= n; i++) { float t = (float)i / n; rect(lroundf(x0 + (x1 - x0) * t - th / 2.f), lroundf(y0 + (y1 - y0) * t - th / 2.f), th, th, c); }
}
static void arc(float cx, float cy, float r, float a0, float a1, int th, uint8_t c) {
  int n = (int)ceilf(r * fabsf(a1 - a0) * 2) + 2;
  for (int i = 0; i <= n; i++) { float a = a0 + (a1 - a0) * i / n; rect(lroundf(cx + cosf(a) * r - th / 2.f), lroundf(cy + sinf(a) * r - th / 2.f), th, th, c); }
}
static void heart(float cx, float cy, float s, uint8_t c) {
  for (float y = -s * 1.3f; y <= s * 1.3f; y++) for (float x = -s * 1.3f; x <= s * 1.3f; x++) {
    float X = x / s, Y = -y / s + .25f, q = X * X + Y * Y - 1;
    if (q * q * q - X * X * Y * Y * Y <= 0) px(lroundf(cx + x), lroundf(cy + y), c);
  }
}
static void zglyph(int x, int y, uint8_t c) { // 3x5 "z"
  const char *g = "111001010100111";
  for (int j = 0; j < 5; j++) for (int i = 0; i < 3; i++) if (g[j * 3 + i] == '1') px(x + i, y + j, c);
}

// ---------- face geometry (docs/FACE_STYLE.md) ----------
static const float CX = 40, EY = 19, EL = 21, ER = 59, EW = 13, EH = 19, MY = 28, P = PI;
static void smile() { arc(CX, MY - 3, 4, P * .18f, P * .82f, 2, 1); }
static void happyEyes() { arc(EL, EY + 4, 7, P * 1.1f, P * 1.9f, 3, 1); arc(ER, EY + 4, 7, P * 1.1f, P * 1.9f, 3, 1); }
static int jit(int f, int a = 1) { return ((f >> 1) % 2) ? a : -a; }

enum Enter { E_SQUASH, E_ZOOM, E_OFF };
struct Mood { const char *name; bool blink, glance; float shake; bool drift; Enter enter; void (*draw)(int f); };

static void dIdle(int) { rrect(EL, EY, EW, EH, 4, 1); rrect(ER, EY, EW, EH, 4, 1); smile(); }
static void dHappy(int) { happyEyes(); ell(CX, MY, 4, 3, 1); rect(CX - 4, MY - 3, 9, 3, 0); }
static void dLaugh(int) {
  line(EL - 7, EY - 7, EL + 5, EY, 3, 1); line(EL - 7, EY + 7, EL + 5, EY, 3, 1);
  line(ER + 7, EY - 7, ER - 5, EY, 3, 1); line(ER + 7, EY + 7, ER - 5, EY, 3, 1);
  ell(CX, MY + 1, 7, 6, 1); rect(CX - 8, MY - 5, 17, 6, 0); ell(CX, MY + 4, 4, 2, 2);
}
static void dYum(int f) { happyEyes(); smile(); int w = jit(f); ell(CX + w, MY + 3, 3, 3, 2); px(CX + w, MY + 2, 1); }
static void dLove(int f) { float b = (f % 15) < 3 ? 1.25f : 1; heart(EL, EY, 6 * b, 2); heart(ER, EY, 6 * b, 2); smile(); }
static void dSleepy(int f) {
  int b = lroundf(sinf(f / 30.f * 1.6f));
  rect(EL - 7, EY + 4 + b, 14, 2, 1); rect(ER - 7, EY + 4 + b, 14, 2, 1); ell(CX, MY + 1, 2, 2, 1);
  int z = (f / 8) % 4; for (int i = 0; i <= z && i < 3; i++) zglyph(66 + i * 4, 15 - i * 4, 4);
}
static void dSad(int f) {
  rrect(EL, EY + 2, EW, EH - 3, 5, 1); rrect(ER, EY + 2, EW, EH - 3, 5, 1);
  for (int i = 0; i < 7; i++) { rect(EL - 7, EY - 7 + i, 7 - i, 1, 0); rect(ER + i, EY - 7 + i, 7 - i, 1, 0); }
  arc(CX, MY + 4, 4, P * 1.2f, P * 1.8f, 2, 1); ell(EL - 3, EY + 9 + (f % 16), 1.3f, 1.8f, 3);
}
static void dGrumpy(int f) {
  rrect(EL, EY + 2, EW, EH - 3, 5, 1); rrect(ER, EY + 2, EW, EH - 3, 5, 1);
  for (int i = 0; i < 7; i++) { rect(EL + i, EY - 7 + i, 7 - i, 1, 0); rect(ER - 7, EY - 7 + i, 7 - i, 1, 0); }
  line(CX - 7, MY, CX - 4, MY - 2, 2, 1); line(CX - 4, MY - 2, CX, MY, 2, 1); line(CX, MY, CX + 4, MY - 2, 2, 1); line(CX + 4, MY - 2, CX + 7, MY, 2, 1);
  if ((f >> 2) % 2) { rect(71, 8, 2, 5, 2); rect(69, 9, 6, 2, 2); }
}
static void dSurprised(int) { ring(EL, EY, 8, 3, 1); ring(ER, EY, 8, 3, 1); rect(EL - 1, EY - 1, 3, 3, 1); rect(ER - 1, EY - 1, 3, 3, 1); ell(CX, MY + 2, 2, 3, 1); }
static void dDizzy(int f) {
  float t = f / 30.f;
  for (int e = 0; e < 2; e++) { float cx = e ? ER : EL; int dir = e ? -1 : 1;
    for (float a = 0; a < P * 4; a += .12f) { float r = 1 + a * .62f, aa = dir * (a + t * 7); px(lroundf(cx + cosf(aa) * r), lroundf(EY + sinf(aa) * r), 1); } }
  for (int i = 0; i < 6; i++) line(CX - 9 + i * 3, MY + (i % 2 ? -1 : 1), CX - 6 + i * 3, MY + (i % 2 ? 1 : -1), 2, 1);
}
static void dMunch(int f) {
  happyEyes(); bool open = (f >> 2) % 2;
  if (open) { rrect(CX, MY + 1, 10, 6, 2, 1); rect(CX - 3, MY + 1, 6, 2, 2); px(48 + (f % 6), MY - 2 - (f % 5), 4); px(31 - (f % 5), MY - 1 - (f % 4), 4); }
  else rect(CX - 5, MY, 10, 2, 1);
}
static void dHype(int) { for (int y = 0; y < 34; y++) { int w = 12 + lroundf(y * .15f); rect(3, EY - 16 + y, w, 1, 1); rect(77 - w, EY - 16 + y, w, 1, 1); } smile(); }
static void dOffline(int f) {
  for (int e = 0; e < 2; e++) { float ex = e ? ER : EL; rrect(ex, EY, EW, EH, 4, 4); rrect(ex, EY, EW - 4, EH - 4, 2, 0); }
  rect(CX - 4, MY - 1, 8, 2, 4);
  int g = 2 + ((f >> 4) % 2);
  rect(46, 36, 9, 2, 4); rect(55, 35, 3, 4, 4); rect(58 + g, 35, 3, 4, 4); rect(61 + g, 36, 1, 1, 4); rect(61 + g, 38, 1, 1, 4); rect(62 + g, 36, 5, 2, 4);
  if ((f % 40) < 3) { px(57 + g, 33, 3); px(58 + g, 40, 3); }
}

// Pink cheeks under the eyes (Dr. Smith's pick). Only fills empty pixels, so it never covers eyes or mouth.
static const bool BLUSH = true;
static void blush() {   // two 10x3 pills with clipped corners, one row below the eyes
  for (int e = 0; e < 2; e++) for (int y = 0; y < 3; y++) for (int x = 0; x < 10; x++) {
    if ((y != 1) && (x == 0 || x == 9)) continue;
    int X = e ? 59 + x : 11 + x, Y = 30 + y;
    if (!buf[Y * W + X]) buf[Y * W + X] = 2;
  }
}

static const Mood MOODS[] = {
  {"idle", true, true, 0, false, E_SQUASH, dIdle},
  {"happy", false, false, 0, false, E_SQUASH, dHappy},
  {"laugh", false, false, 1, false, E_SQUASH, dLaugh},
  {"yum", false, false, 0, false, E_SQUASH, dYum},
  {"love", false, false, 0, false, E_SQUASH, dLove},
  {"sleepy", false, false, 0, false, E_SQUASH, dSleepy},
  {"sad", true, false, 0, false, E_SQUASH, dSad},
  {"grumpy", true, false, .5f, false, E_SQUASH, dGrumpy},
  {"surprised", true, false, 0, false, E_ZOOM, dSurprised},
  {"dizzy", false, false, 0, true, E_SQUASH, dDizzy},
  {"munch", false, false, 0, false, E_SQUASH, dMunch},
  {"hype", false, false, 1, false, E_ZOOM, dHype},
  {"offline", false, false, 0, false, E_OFF, dOffline},
};
static const int NMOODS = sizeof(MOODS) / sizeof(MOODS[0]);
static const int LAUGH = 2;

// ---------- animation state ----------
enum Phase { OUT, GLITCH, IN, HOLD };
static int mood = 0, prevMood = 0, myMood = 0, pf = 0, f = 0;
static Phase phase = HOLD;
static int blinkAt = 60, blinkF = -1, glanceAt = 150, glanceF = -1, glanceDir = 1;
static float lookX = 0;
static int reactUntil = -1, beforeReact = 0;

// ---------- touch static ripple: TV-static droplet bursting out from the finger ----------
struct Ripple { float x, y; int a; bool on; };
static Ripple ripples[4];
static const int RIPPLE_LIFE = 18;
static void addRipple(float x, float y) {
  int slot = 0; for (int i = 0; i < 4; i++) { if (!ripples[i].on) { slot = i; break; } if (ripples[i].a > ripples[slot].a) slot = i; }
  ripples[slot] = {x, y, 0, true};
}
static inline uint8_t staticPx() { return random(100) < 85 ? 1 : 3; }
static void applyRipples() {
  uint8_t row[W];
  for (auto &r : ripples) {
    if (!r.on) continue;
    int a = r.a; float R = a * 3.2f, band = 3 + a * .25f, dens = .9f * (1 - (float)a / RIPPLE_LIFE);
    if (a < 2) for (int i = 0; i < W * H / 25; i++) buf[random(W * H)] = staticPx();         // whole screen crackles
    if (a < 10) {                                                                              // splash: shove rows outward
      float reach = R + 8; int cx = lroundf(r.x);
      for (int y = max(0, (int)floorf(r.y - reach)); y < min(H, (int)ceilf(r.y + reach)); y++) {
        float k = 1 - fabsf(y - r.y) / reach; int s = lroundf((9 - a * .8f) * k + random(3) - 1); if (s <= 0) continue;
        memcpy(row, buf + y * W, W);
        for (int x = 0; x < W; x++) { int from = x > cx ? x - s : x < cx ? x + s : x; buf[y * W + x] = (from >= 0 && from < W) ? row[from] : 0; }
      }
    }
    float lim = R + band + 1;                                                                  // the static ring
    for (int y = max(0, (int)floorf(r.y - lim)); y < min(H, (int)ceilf(r.y + lim)); y++)
      for (int x = max(0, (int)floorf(r.x - lim)); x < min(W, (int)ceilf(r.x + lim)); x++) {
        float d = hypotf(x + .5f - r.x, y + .5f - r.y);
        if (fabsf(d - R) < band && random(1000) < dens * 1000) buf[y * W + x] = staticPx();
      }
    if (a < 4) { float cr = 3 + a * 2.5f;                                                      // flash under the finger
      for (int y = floorf(r.y - cr); y <= r.y + cr; y++) for (int x = floorf(r.x - cr); x <= r.x + cr; x++)
        if (hypotf(x - r.x, y - r.y) <= cr && random(100) < 65) px(x, y, random(100) < 80 ? 1 : 3); }
  }
}

static int inLen(int m) { return MOODS[m].enter == E_ZOOM ? 5 : MOODS[m].enter == E_OFF ? 14 : 4; }
static void setMood(int m) { if (m == mood && phase == HOLD) return; prevMood = mood; mood = m; phase = OUT; pf = 0; }

static void tick() {
  f++; pf++;
  if (phase == OUT && pf >= 4) { phase = GLITCH; pf = 0; }
  else if (phase == GLITCH && pf >= 1) { phase = IN; pf = 0; }
  else if (phase == IN && pf >= inLen(mood)) { phase = HOLD; pf = 0; }
  const Mood &M = MOODS[mood];
  if (phase == HOLD) {
    if (M.blink && f >= blinkAt && blinkF < 0) blinkF = 0;
    if (blinkF >= 0 && ++blinkF > 4) { blinkF = -1; blinkAt = f + 60 + random(90); }
    if (M.glance && f >= glanceAt && glanceF < 0) { glanceF = 0; glanceDir = random(2) ? 1 : -1; }
    if (glanceF >= 0) {
      glanceF++;
      if (glanceF <= 8) { float e = 1 - powf(1 - glanceF / 8.f, 3); lookX = glanceDir * 5 * e; }
      else if (glanceF <= 33) lookX = glanceDir * 5;
      else if (glanceF <= 35) lookX = glanceDir * 5 * (1 - (glanceF - 33) / 2.f);
      else { lookX = 0; glanceF = -1; glanceAt = f + 120 + random(120); }
    }
  }
  if (reactUntil > 0 && f >= reactUntil) { reactUntil = -1; setMood(beforeReact); }
  for (auto &r : ripples) if (r.on && ++r.a >= RIPPLE_LIFE) r.on = false;
}

static void xf(float &sx, float &sy, float &dx, float &dy) {
  const Mood &M = MOODS[mood]; sx = sy = 1; dx = lookX; dy = 0;
  if (phase == OUT) { float t = pf / 4.f; sy = 1 - t * .92f; sx = 1 + t * .12f; }
  if (phase == IN) {
    if (M.enter == E_ZOOM) { static const float seq[] = {.35f, .8f, 1.3f, 1.12f, 1}; sx = sy = seq[min(pf, 4)]; }
    else { static const float a[] = {.15f, .7f, 1.25f, 1.05f}, b[] = {1.12f, 1.04f, .94f, 1}; sy = a[min(pf, 3)]; sx = b[min(pf, 3)]; }
  }
  if (phase == HOLD && blinkF >= 0) { static const float s[] = {1, .45f, .08f, .08f, .45f}; sy = blinkF < 5 ? s[blinkF] : 1; }
  if (phase == HOLD && M.shake > 0) { dx += jit(f) * M.shake; dy += jit(f + 1) * M.shake; }
  if (phase == HOLD && M.drift) dx += lroundf(sinf(f / 9.f) * 3);
}

static void renderFrame() {
  int dm = phase == OUT ? prevMood : mood;
  memset(buf, 0, sizeof buf); MOODS[dm].draw(f);
  if (BLUSH && MOODS[dm].enter != E_OFF) blush();
  memcpy(tmp, buf, sizeof buf); memset(buf, 0, sizeof buf);
  float sx, sy, dx, dy; xf(sx, sy, dx, dy);
  const float ox = CX, oy = EY + 2;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    int X = lroundf((x - ox - dx) / sx + ox), Y = lroundf((y - oy - dy) / sy + oy);
    if (X >= 0 && Y >= 0 && X < W && Y < H && tmp[Y * W + X]) buf[y * W + x] = tmp[Y * W + X];
  }
  if (phase == IN && MOODS[mood].enter == E_OFF && pf < 11) { // old-TV switch-off
    memset(buf, 0, sizeof buf);
    if (pf < 5) { int w = lroundf(76 * (1 - pf / 5.f)) + 2; rect(CX - w / 2, EY, w, 2, 1); }
    else if (pf % 2 || pf < 8) rect(CX - 1, EY, 2, 2, pf < 8 ? 1 : 4);
  }
  if (phase == GLITCH) {
    uint8_t row[W];
    for (int y = 0; y < H; y += 2 + random(5)) {
      int sh = random(13) - 6, h = 1 + random(4);
      for (int j = 0; j < h && y + j < H; j++) { memcpy(row, buf + (y + j) * W, W); for (int x = 0; x < W; x++) buf[(y + j) * W + x] = row[(x - sh + W) % W]; }
    }
    for (int i = 0; i < W * H / 20; i++) buf[random(W * H)] = random(100) < 85 ? 1 : 3;
  }
  applyRipples();
  // grid -> 320x170 RGB565 framebuffer, one push per frame
  memset(fb, 0, sizeof fb);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    uint8_t v = buf[y * W + x]; if (!v) continue;
    uint16_t c = COL[v];
    for (int j = 0; j < S; j++) { uint16_t *p = fb + (y * S + OY + j) * SW + x * S; p[0] = p[1] = p[2] = p[3] = c; }
  }
  gfx->draw16bitRGBBitmap(0, 0, fb, SW, SH);
}

// ---------- touch (CST816 family @0x15) ----------
static bool touchOk = false;
static bool readTouch(int &x, int &y) {
  Wire.beginTransmission(TOUCH_ADDR); Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, 7) != 7) return false;
  uint8_t b[7]; for (int i = 0; i < 7; i++) b[i] = Wire.read();
  if (!b[2]) return false;
  int tx = ((b[3] & 0x0F) << 8) | b[4], ty = ((b[5] & 0x0F) << 8) | b[6]; // portrait 170x320
  x = ty; y = 169 - tx;                                                    // -> landscape
  return true;
}
static bool down = false; static int downX = 0, lastX = 0; static uint32_t downAt = 0;
static void handleTouch() {
  if (!touchOk) return;
  int x, y; bool t = readTouch(x, y);
  if (t && !down) { down = true; downX = lastX = x; downAt = millis(); addRipple(x / (float)S, (y - OY) / (float)S); }
  else if (t) lastX = x;
  else if (!t && down) {
    down = false; int dx = lastX - downX;
    if (abs(dx) > 40) { myMood = (myMood + (dx < 0 ? 1 : -1) + NMOODS) % NMOODS; setMood(myMood); Serial.printf("[face] swipe -> %s\n", MOODS[myMood].name); }
    else { // every tap sparks: if already laughing, re-fire the glitch burst + pop instead of doing nothing
      beforeReact = myMood;
      if (mood == LAUGH) { prevMood = LAUGH; phase = GLITCH; pf = 0; } else setMood(LAUGH);
      reactUntil = f + 60; Serial.println("[face] tap -> laugh");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);
  if (!gfx->begin()) Serial.println("[face] gfx->begin() failed");
  gfx->fillScreen(0x0000);
  pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, LOW); // LOW = backlight on
  Wire.begin(I2C_SDA, I2C_SCL);
  for (int i = 0; i < 10 && !touchOk; i++) { Wire.beginTransmission(TOUCH_ADDR); Wire.write(0x00); Wire.write(0x00); touchOk = Wire.endTransmission() == 0; if (!touchOk) delay(100); }
  Serial.printf("[face] up. touch %s\n", touchOk ? "ok" : "not found");
  randomSeed(esp_random());
}

void loop() {
  static uint32_t next = 0; uint32_t now = millis();
  if (now < next) { delay(1); return; }
  next = now + 33; // 30 fps
  handleTouch();
  tick();
  renderFrame();
}
