// facelab — the face lab (tools/face-lab) running on the real board.
// Board: Waveshare ESP32-C6-Touch-LCD-1.9, ST7789V2, used landscape 320x170. See docs/BOARD.md.
// HOME: swipe = pick my mood, stay 3 s = send it, tap = spark. Button = menu (HOME / FEED / SETTINGS).
// Controls spec: docs/SCREENS.md -> Controls. Same face timing as docs/FACE_STYLE.md.
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "qrcode.h"   // ESP-IDF QR maker (Wi-Fi setup screen)
#include <Preferences.h>
static Preferences prefs;

// ---------- hardware ----------
#define PIN_BL 15            // backlight, active LOW
#define PIN_SD_CS 20         // SD shares the SPI bus: keep it deselected
#define I2C_SDA 18
#define I2C_SCL 8
#define TOUCH_ADDR 0x15

// ESP32SPI talks to the SPI hardware directly (HWSPI went through the Arduino SPI layer: 30 ms per frame)
Arduino_DataBus *bus = new Arduino_ESP32SPI(6 /*DC*/, 7 /*CS*/, 5 /*SCK*/, 4 /*MOSI*/, GFX_NOT_DEFINED /*MISO*/, FSPI, false /*not shared*/);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 14 /*RST*/, 1 /*landscape*/, true /*IPS: colours inverted otherwise*/, 170, 320, 35, 0, 35, 0);

static uint32_t statMax = 0, statSum = 0, statN = 0, statPush = 0;
// ---------- pixel grid ----------
static const int W = 80, H = 42, S = 4, OY = 1, SW = 320, SH = 170;
static uint8_t buf[W * H], tmp[W * H];
static uint16_t fb[SW * SH];
static uint16_t COL[] = {0x0000, 0xF7DF, 0xFB73, 0x6E9F, 0x8C53, 0xFB73}; // off, white, pink, blue, grey, 5 = partner's colour (settings)

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
static bool blushOn = true; // set in the settings screen (side button), saved across reboots
// more settings (saved): partner's tint colour + strength, how often their mood shows, screen brightness
static int tintC = 0, tintK = 0, swapS = 1, bright = 2;
static const char *TINT_NAMES[] = {"PINK", "PEACH", "BLUE", "LILAC", "MINT"};
static const uint8_t TINT_RGB[][3] = {{255, 110, 156}, {255, 150, 90}, {108, 190, 255}, {185, 130, 255}, {90, 230, 170}};
static const float TINT_K[] = {.13f, .22f, .34f}; static const char *TINTK_NAMES[] = {"FAINT", "SOFT", "BOLD"};
static const int SWAP_S[] = {3, 5, 8, 0};         static const char *SWAP_NAMES[] = {"3S", "5S", "8S", "OFF"};
static const uint8_t BRIGHT[] = {45, 130, 255};   static const char *BRIGHT_NAMES[] = {"LOW", "MID", "HIGH"};
static uint16_t rgb565(int r, int g, int b) { return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); }
static uint16_t tintCol(float k) { const uint8_t *c = TINT_RGB[tintC]; return rgb565(c[0] * k, c[1] * k, c[2] * k); }
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
static int swipeDir = 0; // -1/+1 while a swipe transition runs (sideways tear), 0 = normal squash

// ---------- poke: a light static ripple from the finger + a push that shoves the face away, then springs back ----------
struct Poke { float x, y; int a; bool on; bool crackle; };
static Poke poke = {0, 0, 0, false, false};
static const int POKE_LIFE = 11;
static float pushX = 0, pushY = 0, velX = 0, velY = 0;
static void addPoke(float x, float y) {
  poke = {x, y, 0, true, !poke.on};             // whole-screen crackle only on a fresh tap, not on rapid repeats                       // one at a time: a new tap restarts it (no pile-up)
  float dx = CX - x, dy = (EY + 2) - y, d = sqrtf(dx * dx + dy * dy); if (d < 1) { dx = 0; dy = 1; d = 1; }
  velX += dx / d * 4.4f; velY += dy / d * 2.6f; // shove away from the finger
  velX = constrain(velX, -5.f, 5.f); velY = constrain(velY, -3.f, 3.f); // rapid taps can't stack into a huge fling
}
static void tickPoke() {
  velX += -.28f * pushX; velY += -.28f * pushY; velX *= .74f; velY *= .74f; // damped spring back to centre
  pushX = constrain(pushX + velX, -7.f, 7.f); pushY = constrain(pushY + velY, -4.f, 4.f);
  if (fabsf(pushX) < .05f && fabsf(velX) < .05f) pushX = velX = 0;
  if (fabsf(pushY) < .05f && fabsf(velY) < .05f) pushY = velY = 0;
  if (poke.on && ++poke.a >= POKE_LIFE) poke.on = false;
}
static void applyPoke() {
  if (!poke.on) return;
  int a = poke.a; float R = a * 2.4f, band = 2.4f, dens = .9f * (1 - (float)a / POKE_LIFE);
  if (a < 2 && poke.crackle) for (int i = 0; i < W * H / (a ? 40 : 22); i++) buf[random(W * H)] = random(100) < 85 ? 1 : 3; // crackle across the screen
  if (a < 2 && poke.crackle) { // the whole picture jolts: random rows tear sideways a little
    uint8_t row[W];
    for (int y = 0; y < H; y++) { if (random(100) >= 30) continue; int sh = random(5) - 2; if (!sh) continue;
      memcpy(row, buf + y * W, W); for (int x = 0; x < W; x++) buf[y * W + x] = row[(x - sh + W) % W]; }
  }
  if (a < 5) { // shove of the rows at the finger, outward
    uint8_t row[W]; int cx = lroundf(poke.x), s = 5 - a;
    for (int y = max(0, (int)poke.y - 7); y < min(H, (int)poke.y + 8); y++) {
      memcpy(row, buf + y * W, W);
      for (int x = 0; x < W; x++) { int from = x > cx ? x - s : x < cx ? x + s : x; buf[y * W + x] = (from >= 0 && from < W) ? row[from] : 0; }
    }
  }
  float lo = R - band, hi = R + band, lo2 = lo > 0 ? lo * lo : 0, hi2 = hi * hi; int lim = (int)ceilf(hi) + 1;
  for (int y = max(0, (int)poke.y - lim); y < min(H, (int)poke.y + lim + 1); y++)
    for (int x = max(0, (int)poke.x - lim); x < min(W, (int)poke.x + lim + 1); x++) {
      float ddx = x + .5f - poke.x, ddy = y + .5f - poke.y, d2 = ddx * ddx + ddy * ddy;
      if (d2 >= lo2 && d2 <= hi2 && random(100) < dens * 100) buf[y * W + x] = random(100) < 85 ? 1 : 3;
    }
}

static int inLen(int m) { return MOODS[m].enter == E_ZOOM ? 5 : MOODS[m].enter == E_OFF ? 14 : 4; }
static bool softT = false; // soft dissolve (mood swap with the partner) instead of squash + glitch
static void setMood(int m) { if (m == mood && phase == HOLD) return; prevMood = mood; mood = m; phase = OUT; pf = 0; swipeDir = 0; softT = false; }
static void swipeTo(int m, int dir) { setMood(m); swipeDir = dir; }
static void softTo(int m) { if (m == mood && phase == HOLD) return; setMood(m); softT = true; }

static void tick() {
  f++; pf++;
  if (phase == OUT && pf >= 4) { phase = GLITCH; pf = 0; }
  else if (phase == GLITCH && pf >= 1) { phase = IN; pf = 0; }
  else if (phase == IN && pf >= inLen(mood)) { phase = HOLD; pf = 0; swipeDir = 0; softT = false; }
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
  tickPoke();
}

static void xf(float &sx, float &sy, float &dx, float &dy) {
  const Mood &M = MOODS[mood]; sx = sy = 1; dx = lookX; dy = 0;
  if (phase == OUT && !softT) { float t = pf / 4.f; sy = 1 - t * .92f; sx = 1 + t * .12f; }
  if (phase == IN && !softT) {
    if (M.enter == E_ZOOM) { static const float seq[] = {.35f, .8f, 1.3f, 1.12f, 1}; sx = sy = seq[min(pf, 4)]; }
    else { static const float a[] = {.15f, .7f, 1.25f, 1.05f}, b[] = {1.12f, 1.04f, .94f, 1}; sy = a[min(pf, 3)]; sx = b[min(pf, 3)]; }
  }
  if (swipeDir && phase == OUT) { static const float o[] = {2, 7, 15, 28}; sx = 1.06f; sy = .94f; dx = lookX + swipeDir * o[min(pf, 3)]; }
  if (swipeDir && phase == IN) { static const float o[] = {-30, -14, -4, 2}; sx = sy = 1; dx = swipeDir * o[min(pf, 3)]; }
  if (phase == HOLD && blinkF >= 0) { static const float s[] = {1, .45f, .08f, .08f, .45f}; sy = blinkF < 5 ? s[blinkF] : 1; }
  if (phase == HOLD && M.shake > 0) { dx += jit(f) * M.shake; dy += jit(f + 1) * M.shake; }
  if (phase == HOLD && M.drift) dx += lroundf(sinf(f / 9.f) * 3);
  dx += pushX; dy += pushY; // poke push
}

// ---------- link hooks: Lorenzo's agent implements these (radio / Wi-Fi). Defaults do nothing. ----------
__attribute__((weak)) void tp_send_mood(uint8_t) {}
__attribute__((weak)) void tp_send_food(uint8_t) {}
__attribute__((weak)) bool tp_partner_mood(uint8_t &) { return false; } // true + mood index when partner's mood is known
__attribute__((weak)) bool tp_food_arrived(uint8_t &) { return false; } // true once per food the partner sent
__attribute__((weak)) void tp_begin(uint8_t /*offline mood index*/) {}   // starts the link (tp_link.cpp)
__attribute__((weak)) const char *tp_setup_ap() { return nullptr; }
__attribute__((weak)) void tp_print_status() {}       // serial 'k': link details
__attribute__((weak)) int tp_link_state() { return -1; } // -1 no link, 0 no Wi-Fi yet, 1 connecting, 2 online, 3 online + partner on

// ---------- screens ----------
enum Screen { S_HOME, S_MENU, S_FEED, S_SETTINGS };
static Screen screen = S_HOME;
static int menuSel = 0, foodSel = 0, menuPress = -1, menuPickF = -1; // menuPress = card under the finger, menuPickF = "opening" anim
static int screenGlitch = 0; // frames of static when switching screens
static int enterF = 0;       // frames since the screen opened (things pop in)
static bool setupHidden = false; // side button on the Wi-Fi QR screen = play offline for now (setup keeps waiting; Settings brings it back)
static const int MUNCH = 10, OFFLINE = NMOODS - 1, SWIPE_MOODS = NMOODS - 1; // offline is never picked by hand
static void go(int s) { screen = (Screen)s; screenGlitch = 1; enterF = 0; menuPress = menuPickF = -1; Serial.printf("[face] screen %d\n", (int)s); }

// tiny 3x5 font
static void glyph(char ch, int x, int y, uint8_t c) {
  const char *g = nullptr;
  switch (ch) {
    case 'A': g = "010101111101101"; break; case 'B': g = "110101110101110"; break;
    case 'C': g = "111100100100111"; break; case 'D': g = "110101101101110"; break;
    case 'E': g = "111100110100111"; break; case 'F': g = "111100110100100"; break;
    case 'G': g = "111100101101111"; break; case 'H': g = "101101111101101"; break;
    case 'I': g = "111010010010111"; break; case 'K': g = "101101110101101"; break;
    case 'L': g = "100100100100111"; break; case 'M': g = "101111111101101"; break;
    case 'N': g = "110101101101101"; break; case 'O': g = "111101101101111"; break;
    case 'R': g = "110101110101101"; break; case 'S': g = "111100111001111"; break;
    case 'T': g = "111010010010010"; break; case 'U': g = "101101101101111"; break;
    case 'Y': g = "101101010010010"; break; case 'P': g = "110101110100100"; break;
    case 'J': g = "001001001101111"; break; case 'Q': g = "111101101111001"; break;
    case 'V': g = "101101101101010"; break; case 'X': g = "101101010101101"; break;
    case 'Z': g = "111001010100111"; break; case '-': g = "000000111000000"; break;
    case '0': g = "111101101101111"; break; case '1': g = "010110010010111"; break;
    case '2': g = "111001111100111"; break; case '3': g = "111001111001111"; break;
    case '4': g = "101101111001001"; break; case '5': g = "111100111001111"; break;
    case '6': g = "111100111101111"; break; case '7': g = "111001001001001"; break;
    case '8': g = "111101111101111"; break; case '9': g = "111101111001111"; break;
  }
  if (g) for (int j = 0; j < 5; j++) for (int i = 0; i < 3; i++) if (g[j * 3 + i] == '1') px(x + i, y + j, c);
}
static int glyphW(char ch) { return ch == 'M' || ch == 'W' ? 5 : ch == ' ' ? 1 : 3; }
static void label(const char *s, int x, int y, uint8_t c) {
  for (; *s; s++) {
    if (*s == 'M' || *s == 'W') { const char *m = *s == 'M' ? "1000111011101011000110001" : "1000110001101011101110001"; for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) if (m[j * 5 + i] == '1') px(x + i, y + j, c); }
    else glyph(*s, x, y, c);
    x += glyphW(*s) + 1;
  }
}
static void labelC(const char *s, int y, uint8_t c) { int w = -1; for (const char *q = s; *q; q++) w += glyphW(*q) + 1; label(s, (W - w) / 2, y, c); }

// ---------- foods (feed carousel); k = size, 1 = big ----------
static void fDango(float cx, float cy, float k) {
  line(cx, cy - 12 * k, cx, cy + 12 * k, 1, 4);
  ell(cx, cy - 7 * k, 4.2f * k, 4.2f * k, 2); ell(cx, cy, 4.2f * k, 4.2f * k, 1); ell(cx, cy + 7 * k, 4.2f * k, 4.2f * k, 3);
}
static void fRice(float cx, float cy, float k) {
  for (float y = -9 * k; y <= 7 * k; y++) { float hw = (2 + (y + 9 * k) * .62f); rrect(cx, cy + y, hw * 2, 1.2f, 0, 1); }
  rect(lroundf(cx - 4 * k), lroundf(cy + 2 * k), lroundf(8 * k), lroundf(5 * k) + 1, 4);
}
static void fCake(float cx, float cy, float k) {
  rrect(cx, cy + 3 * k, 17 * k, 10 * k, 1, 1);
  rrect(cx, cy - 3 * k, 17 * k, 4 * k, 1, 2);
  for (int i = -1; i <= 1; i++) rect(lroundf(cx + i * 5 * k), lroundf(cy - 1 * k), 1, lroundf(2 * k) + 1, 2);
  ell(cx, cy - 8 * k, 2.2f * k, 2.2f * k, 2);
}
static void fCandy(float cx, float cy, float k) {
  ell(cx, cy, 5.5f * k, 4.5f * k, 2);
  for (int s2 = -1; s2 <= 1; s2 += 2) for (float i = 0; i < 5 * k; i++) rect(lroundf(cx + s2 * (5 * k + i)), lroundf(cy - i * .8f), 1, lroundf(i * 1.6f) + 1, 1);
  line(cx - 2 * k, cy - 3 * k, cx + 1 * k, cy + 3 * k, 1, 1);
}
struct Food { const char *name; void (*draw)(float, float, float); };
static const Food FOODS[] = { {"DANGO", fDango}, {"RICE", fRice}, {"CAKE", fCake}, {"CANDY", fCandy} };
static const int NFOODS = sizeof(FOODS) / sizeof(FOODS[0]);
static int foodSentF = -1, foodPopF = -1;  // fly-off after sending, then the next one pops back in
static float foodPos = 0; static int foodTarget = 0, dragDX = 0; // carousel slides (eases) and follows the finger while dragging
static const float SLOT = 29;              // grid px between foods
static float popK(int t) { static const float s[] = {.2f, .6f, 1.15f, 1.05f, 1}; return t < 0 ? 0 : t < 5 ? s[t] : 1; }

static void drawFeed() {
  memset(buf, 0, sizeof buf);
  foodPos += (foodTarget - foodPos) * .32f; if (fabsf(foodTarget - foodPos) < .01f) foodPos = foodTarget;
  float view = foodPos - dragDX / (SLOT * S);
  int near = ((int)lroundf(view) % NFOODS + NFOODS) % NFOODS;
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < NFOODS; i++) {   // side foods first, the middle one on top
    float o = fmodf(i - view, NFOODS); if (o < -NFOODS / 2.f) o += NFOODS; if (o >= NFOODS / 2.f) o -= NFOODS;
    float a = fabsf(o); if (a > 1.7f || (a < .5f) != (pass == 1)) continue;
    if (i == foodSel && foodSentF >= 0) continue;                          // it's flying away
    float k = (1 - min(a, 1.f) * .45f) * popK(enterF - (int)lroundf(a) * 2);
    if (i == foodSel && foodPopF >= 0) k *= popK(foodPopF);
    if (k > .1f) FOODS[i].draw(40 + o * SLOT, 17 + a * 1.5f, k);
  }
  if (foodSentF >= 0) {                                                    // off it goes, with two little hearts
    float t = foodSentF / 14.f, e = t * t;
    FOODS[foodSel].draw(40 + e * 34, 17 - e * 30, 1 - t * .6f);
    heart(50 + t * 18, 22 - t * 20, 1.8f, 2); if (foodSentF > 3) heart(58 + t * 14, 26 - t * 18, 1.4f, 2);
  }
  for (int i = 0; i < NFOODS; i++) rect(40 - NFOODS * 2 + i * 4 + 1, 40, 2, 1, i == near ? 2 : 4); // page dots
  labelC(foodSentF >= 0 ? "SENT" : FOODS[near].name, 33, foodSentF >= 0 ? 2 : 1);
}

// ---------- menu: three cards ----------
static void gear(float cx, float cy, uint8_t c, float k = 1) {
  ring(cx, cy, 6 * k, max(1.f, 3 * k), c);
  for (int i = 0; i < 8; i++) { float a = i * P / 4 + f * .02f; rect(lroundf(cx + cosf(a) * 7 * k) - 1, lroundf(cy + sinf(a) * 7 * k) - 1, 2, 2, c); }
}
static float markX = 13;
static void drawMenu() {
  memset(buf, 0, sizeof buf);
  static const char *NAMES[] = {"HOME", "FEED", "SETTINGS"};
  static const uint8_t CC[] = {1, 2, 3};                                  // every card in its own colour: white face, pink food, blue gear
  for (int i = 0; i < 3; i++) {
    float k = popK(enterF - i * 2), cx = 13 + i * 27, cy = 15;          // cards pop in one after another
    bool sel = i == menuSel;
    if (sel && menuPickF < 0 && (f / 10) % 2) cy -= 1;                 // chosen card bobs
    if (i == menuPress) k *= .86f;                                      // pressed under the finger
    if (sel && menuPickF >= 0) { static const float pk[] = {.84f, .92f, 1.1f, 1.22f, 1.3f, 1.34f}; k *= pk[min(menuPickF, 5)]; }
    else if (menuPickF >= 0) k *= max(0.f, 1 - menuPickF * .2f);        // the others shrink away
    if (k < .15f) continue;
    uint8_t c = CC[i];
    rrect(cx, cy, 24 * k, 26 * k, 4 * k, c); rrect(cx, cy, 24 * k - 4, 26 * k - 4, max(1.f, 3 * k), 0);
    if (i == 0) { rrect(cx - 4 * k, cy - 2 * k, 4 * k, 6 * k, 2 * k, c); rrect(cx + 4 * k, cy - 2 * k, 4 * k, 6 * k, 2 * k, c);
                  arc(cx, cy + 2 * k, 2 * k, P * .2f, P * .8f, 1, c);
                  if (blushOn) { rect(lroundf(cx - 8 * k), lroundf(cy + 3 * k), 3, 1, 2); rect(lroundf(cx + 6 * k), lroundf(cy + 3 * k), 3, 1, 2); } }
    if (i == 1) fDango(cx, cy, .62f * k);
    if (i == 2) gear(cx, cy, c, k);
  }
  markX += (13 + menuSel * 27 - markX) * .4f;                           // pink marker slides under the chosen card
  if (menuPickF < 0) rect(lroundf(markX) - 3, 30, 7, 1, 2);
  labelC(NAMES[menuSel], 34, 1);
}

// ---------- settings: tap a row to change it (saved). Swipe = back to the menu ----------
static int rowFlash[5] = {0}, previewUntil = 0;
static const int ROW_Y0 = 2, ROW_H = 7, NROWS = 5;
static void labelR(const char *s, int xr, int y, uint8_t c) { int w = -1; for (const char *q = s; *q; q++) w += glyphW(*q) + 1; label(s, xr - w, y, c); }
static void applyBright() { ledcWrite(PIN_BL, 255 - BRIGHT[bright]); }   // backlight is active LOW
static void changeSetting(int r) {
  rowFlash[r] = 8;
  switch (r) {
    case 0: blushOn = !blushOn; prefs.putBool("blush", blushOn); break;
    case 1: swapS = (swapS + 1) % 4; prefs.putUChar("swap", swapS); break;
    case 2: tintC = (tintC + 1) % 5; prefs.putUChar("tintc", tintC); COL[5] = tintCol(1); previewUntil = f + 45; break;
    case 3: tintK = (tintK + 1) % 3; prefs.putUChar("tintk", tintK); previewUntil = f + 45; break;
    case 4: bright = (bright + 1) % 3; prefs.putUChar("bright", bright); applyBright(); break;
  }
  Serial.printf("[face] settings: blush %d swap %s colour %s tint %s bright %s\n", blushOn, SWAP_NAMES[swapS], TINT_NAMES[tintC], TINTK_NAMES[tintK], BRIGHT_NAMES[bright]);
}
static void drawSettings() {
  memset(buf, 0, sizeof buf);
  static const char *NAMES[] = {"BLUSH", "SWAP EVERY", "THEIR COLOUR", "TINT", "BRIGHTNESS"};
  for (int i = 0; i < NROWS; i++) {
    int d = enterF - i; if (d < 0) continue;                       // rows slide in one after another
    int y = ROW_Y0 + i * ROW_H, xo = d < 4 ? -(4 - d) * 6 : 0;
    if (rowFlash[i] > 0) { rowFlash[i]--; rect(1, y, 1, 5, 2); }     // pink tick = just changed
    label(NAMES[i], 4 + xo, y, 1);
    int xr = 77 + xo; bool pop = rowFlash[i] > 5;                    // value nudges when it changes
    switch (i) {
      case 0: rrect(xr - 6, y + 2, 12, 5, 2.5f, blushOn ? 2 : 4); rrect(blushOn ? xr - 2 : xr - 10, y + 2, 3, 3, 1, 1); break;
      case 1: labelR(SWAP_NAMES[swapS], xr - pop, y, 2); break;
      case 2: rrect(xr - 6 - pop, y + 2, 12, 5, 2, 5); break;            // swatch in their colour
      case 3: labelR(TINTK_NAMES[tintK], xr - pop, y, 5); break;
      case 4: for (int b = 0; b < 3; b++) rect(xr - 10 + b * 4, y + 4 - b * 2, 3, 1 + b * 2, b <= bright ? 2 : 4); break;
    }
  }
  int ls = tp_link_state();
  static const char *LS[] = {"NO WIFI - TAP FOR QR", "CONNECTING", "ONLINE - PARTNER OFF", "ONLINE - PARTNER ON"};
  if (enterF > NROWS) labelC(ls < 0 ? "NO LINK" : LS[min(ls, 3)], 37, ls == 3 ? 3 : 4);
}

// ---------- home: my mood, partner's mood, sending ----------
static int lastSwipeF = -1, sentMood = -1, sentF = -1;     // stay 3 s on a mood = send it
static int partnerMood = -1, swapAt = 0; static bool showingPartner = false, fakePartner = false; // partner's mood shows on a faint tint (settings)

// food from the partner: it drops in from the top, the pet gasps (surprised), catches it, then munches 3 s
static const int SURPRISED = 8, FALL = 18;
static int arriveF = -1, arriveFood = 0, fakeFood = -1;    // fakeFood: serial test trigger ('a')

static void homeLogic() {
  uint8_t v;
  if (tp_partner_mood(v) && v < NMOODS) partnerMood = v; else if (!fakePartner) partnerMood = -1;
  bool got = tp_food_arrived(v);
  if (!got && fakeFood >= 0) { v = fakeFood; fakeFood = -1; got = true; }
  if (got && v < NFOODS) {
    if (screen != S_HOME) go(S_HOME);
    showingPartner = false; lastSwipeF = -1; reactUntil = -1; beforeReact = myMood;
    arriveFood = v; arriveF = 0; setMood(SURPRISED);
    Serial.printf("[face] food arrived %s\n", FOODS[v].name);
  }
  if (arriveF >= 0) {
    if (++arriveF == FALL) { setMood(MUNCH); reactUntil = f + 90; }   // caught it
    if (arriveF > FALL + 8) arriveF = -1;
  }
  if (screen != S_HOME) return;
  if (lastSwipeF >= 0 && f - lastSwipeF >= 90) {           // settled for 3 s
    lastSwipeF = -1; swapAt = f + 30 * max(SWAP_S[swapS], 3);
    if (myMood != sentMood) { tp_send_mood(myMood); sentMood = myMood; sentF = 0; Serial.printf("[face] sent mood %s\n", MOODS[myMood].name); }
  }
  if (sentF >= 0 && ++sentF > 24) sentF = -1;
  // mood swap: mine for SWAP EVERY seconds, then theirs for 3 s on a faint background in their colour.
  // Soft dissolve both ways (not the big squash + glitch: that's for my own swipes).
  int every = SWAP_S[swapS];
  if (partnerMood < 0 || !every) { if (showingPartner) { showingPartner = false; softTo(myMood); } }
  else if (lastSwipeF < 0 && reactUntil < 0 && arriveF < 0 && phase == HOLD && f >= swapAt) {
    showingPartner = !showingPartner;
    swapAt = f + (showingPartner ? 90 : every * 30);
    softTo(showingPartner ? partnerMood : myMood);
  } else if (showingPartner && phase == HOLD && reactUntil < 0 && mood != partnerMood) softTo(partnerMood); // they changed it meanwhile
}

// swipe tear: rows rip sideways in the swipe direction and leave blue streaks behind
static void applySwipeTear() {
  float k = phase == OUT ? (pf + 1) / 4.f : phase == GLITCH ? 1.f : 1 - pf / 4.f; if (k <= 0) return;
  uint8_t row[W];
  for (int y = 0; y < H; y++) {
    if (random(100) >= 40 * k) continue;
    int s = swipeDir * (1 + random(1 + (int)(6 * k)));
    memcpy(row, buf + y * W, W);
    for (int x = 0; x < W; x++) { int from = x - s; buf[y * W + x] = (from >= 0 && from < W) ? row[from] : 0; }
    for (int x = 0; x < W; x++) if (row[x] && row[x] != 3) for (int t = 1; t <= 2; t++) { int X = x + s - swipeDir * t; if (X >= 0 && X < W && !buf[y * W + X]) buf[y * W + X] = 3; }
  }
}

static void renderFace() {
  int dm = phase == OUT ? prevMood : mood;
  memset(buf, 0, sizeof buf); MOODS[dm].draw(f);
  if (blushOn && MOODS[dm].enter != E_OFF) blush();
  if (softT && phase != HOLD) { // ordered-dither dissolve: the old face crumbles away, the new one fills in, blue sparkles at the edge
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    float keep = phase == OUT ? 1 - (pf + 1) / 5.f : phase == GLITCH ? 0.f : (pf + 1) / (float)(inLen(mood) + 1);
    int k = lroundf(keep * 18);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { uint8_t &v = buf[y * W + x]; if (!v) continue;
      int b = BAYER[(y & 3) * 4 + (x & 3)]; if (b >= k) v = b < k + 3 ? 3 : 0; }
  }
  memcpy(tmp, buf, sizeof buf); memset(buf, 0, sizeof buf);
  float sx, sy, dx, dy; xf(sx, sy, dx, dy);
  const float ox = CX, oy = EY + 2;
  // the transform is separable: work out the source column/row once (122 float ops) instead of per pixel (3360)
  static int mapX[W], mapY[H];
  for (int x = 0; x < W; x++) mapX[x] = lroundf((x - ox - dx) / sx + ox);
  for (int y = 0; y < H; y++) mapY[y] = lroundf((y - oy - dy) / sy + oy);
  for (int y = 0; y < H; y++) { int Y = mapY[y]; if (Y < 0 || Y >= H) continue; const uint8_t *src = tmp + Y * W; uint8_t *dst = buf + y * W;
    for (int x = 0; x < W; x++) { int X = mapX[x]; if (X >= 0 && X < W && src[X]) dst[x] = src[X]; } }
  if (phase == IN && MOODS[mood].enter == E_OFF && pf < 11 && !softT) { // old-TV switch-off
    memset(buf, 0, sizeof buf);
    if (pf < 5) { int w = lroundf(76 * (1 - pf / 5.f)) + 2; rect(CX - w / 2, EY, w, 2, 1); }
    else if (pf % 2 || pf < 8) rect(CX - 1, EY, 2, 2, pf < 8 ? 1 : 4);
  }
  if (phase == GLITCH && !softT) {
    uint8_t row[W];
    for (int y = 0; y < H; y += 2 + random(5)) {
      int sh = random(13) - 6, h = 1 + random(4);
      for (int j = 0; j < h && y + j < H; j++) { memcpy(row, buf + (y + j) * W, W); for (int x = 0; x < W; x++) buf[(y + j) * W + x] = row[(x - sh + W) % W]; }
    }
    for (int i = 0; i < W * H / 20; i++) buf[random(W * H)] = random(100) < 85 ? 1 : 3;
  }
  if (swipeDir && phase != HOLD) applySwipeTear();                         // swipe = sideways tear
  applyPoke();                                                             // tap = poke (static ring + push)
  if (sentF >= 0) heart(70 + sentF * .2f, 30 - sentF * 1.4f, 2.2f, 2);   // little heart floats off = sent
  if (arriveF >= 0 && arriveF < FALL) {                                    // partner's food falls (speeds up like gravity)
    float t = (float)arriveF / FALL; FOODS[arriveFood].draw(CX, -8 + (MY + 1 + 8) * t * t, .45f);
  } else if (arriveF >= FALL) {                                            // catch sparkle either side of the mouth
    int k = arriveF - FALL; uint8_t c = k % 2 ? 1 : 2;
    px(CX - 7 - k / 2, MY - 3 - k / 2, c); px(CX + 7 + k / 2, MY - 3 - k / 2, c); px(CX - 9 - k / 2, MY - k / 3, c); px(CX + 9 + k / 2, MY - k / 3, c);
  }
}

// ---------- Wi-Fi setup screen: scan the QR with a phone camera -> joins the pod's own Wi-Fi -> setup page pops up ----------
static uint8_t qrMods[33 * 33]; static int qrSize = 0; static char qrFor[24] = "";
static void qrGrab(esp_qrcode_handle_t q) { qrSize = esp_qrcode_get_size(q); if (qrSize > 33) { qrSize = 0; return; }
  for (int y = 0; y < qrSize; y++) for (int x = 0; x < qrSize; x++) qrMods[y * qrSize + x] = esp_qrcode_get_module(q, x, y); }
static void drawSetup(const char *ap) {
  if (strcmp(qrFor, ap)) {                                    // build the QR once per network name
    char txt[64]; snprintf(txt, sizeof txt, "WIFI:T:nopass;S:%s;;", ap);
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT(); cfg.display_func = qrGrab; cfg.max_qrcode_version = 3;
    esp_qrcode_generate(&cfg, txt); strncpy(qrFor, ap, sizeof qrFor - 1);
  }
  memset(buf, 0, sizeof buf);
  int q0x = 3, q0y = (H - qrSize - 4) / 2;                    // white card with a 2-module quiet zone
  rect(q0x, q0y, qrSize + 4, qrSize + 4, 1);
  for (int y = 0; y < qrSize; y++) for (int x = 0; x < qrSize; x++) if (qrMods[y * qrSize + x]) px(q0x + 2 + x, q0y + 2 + y, 0);
  int tx = q0x + qrSize + 8;
  label("SCAN ME", tx, 6, 1);
  label("WITH YOUR", tx, 14, 4); label("CAMERA", tx, 20, 4);
  char up[24]; int n = 0; for (const char *c = ap; *c && n < 23; c++) up[n++] = toupper(*c); up[n] = 0;
  char *dash = strchr(up, '-');
  if (dash) { *dash = 0; label(up, tx, 29, 2); label(dash + 1, tx, 35, 2); } else label(up, tx, 29, 2);
  if ((f / 15) % 2) rect(tx - 4, 7, 2, 3, 2);                 // blinking pointer next to SCAN ME
}

static uint16_t shownBg = 0; // background colour on screen now (serial dump)
static void renderFrame() {
  const char *setupAp = setupHidden ? nullptr : tp_setup_ap();
  if (setupAp) drawSetup(setupAp); else
  switch (screen) {
    case S_MENU: drawMenu(); break;
    case S_FEED: drawFeed(); break;
    case S_SETTINGS: drawSettings(); break;
    default: renderFace(); break;
  }
  if (screenGlitch > 0) { screenGlitch--; for (int i = 0; i < W * H / 8; i++) buf[random(W * H)] = random(100) < 85 ? 1 : 3; }
  // grid -> 320x170 RGB565 framebuffer, one push per frame
  // partner's mood (or a settings preview) = faint background in their colour, faded in and out
  static float bgLvl = 0;
  float tgt = (!setupAp && ((screen == S_HOME && showingPartner) || (screen == S_SETTINGS && f < previewUntil))) ? 1 : 0;
  bgLvl += (tgt - bgLvl) * .2f; if (fabsf(tgt - bgLvl) < .03f) bgLvl = tgt;
  uint16_t bg = bgLvl > 0 ? tintCol(bgLvl * TINT_K[tintK]) : 0; shownBg = bg;
  if (bg) for (int i = 0; i < SW * SH; i++) fb[i] = bg; else memset(fb, 0, sizeof fb);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    uint8_t v = buf[y * W + x]; if (!v) continue;
    uint16_t c = COL[v];
    for (int j = 0; j < S; j++) { uint16_t *p = fb + (y * S + OY + j) * SW + x * S; p[0] = p[1] = p[2] = p[3] = c; }
  }
  // only send what changed: the screen wiring can't go faster than ~30 ms for a full frame, but most frames
  // change just a blink or a wobble. Compare with last frame's grid and push the changed rectangle only.
  static uint8_t prevBuf[W * H]; static uint16_t prevBg = 1; static bool first = true;
  int gx0 = W, gy0 = H, gx1 = -1, gy1 = -1;
  if (first || bg != prevBg) { gx0 = 0; gy0 = 0; gx1 = W - 1; gy1 = H - 1; }
  else for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (buf[y * W + x] != prevBuf[y * W + x]) {
    if (x < gx0) gx0 = x; if (x > gx1) gx1 = x; if (y < gy0) gy0 = y; if (y > gy1) gy1 = y; }
  memcpy(prevBuf, buf, sizeof buf); prevBg = bg;
  uint32_t t0 = micros();
  if (gx1 >= 0) {
    int sx0 = gx0 * S, sw = (gx1 - gx0 + 1) * S, sy0 = gy0 * S + OY, sh = (gy1 - gy0 + 1) * S;
    if (first) { sy0 = 0; sh = SH; }                                   // first frame also clears the 1 px margins
    gfx->startWrite();
    for (int y = sy0; y < sy0 + sh; y++) gfx->draw16bitRGBBitmap(sx0, y, fb + y * SW + sx0, sw, 1);
    gfx->endWrite();
  }
  first = false; enterF++;
  statPush = (micros() - t0) / 1000;
}

// ---------- side button (BOOT, GPIO 9, active LOW): home -> menu, anywhere else -> home ----------
#define PIN_BTN 9
static void pressButton() {
  if (tp_setup_ap() && !setupHidden) { setupHidden = true; go(S_HOME); return; } // hide the QR, play offline
  if (screen == S_HOME) { menuSel = 0; go(S_MENU); } else go(S_HOME);
}
static void handleButton() {
  static bool was = false; static uint32_t lastEdge = 0;
  bool pressed = digitalRead(PIN_BTN) == LOW;
  if (pressed && !was && millis() - lastEdge > 250) { lastEdge = millis(); pressButton(); }
  was = pressed;
}

// ---------- touch (CST816 family @0x15) ----------
void releaseTouch(int dx, int x);
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
static int downY = 0; static bool pokePending = false;
static void handleTouch() {
  if (!touchOk) return;
  int x, y; bool t = readTouch(x, y);
  if (t && !down) { down = true; downX = lastX = x; downY = y; downAt = millis(); pokePending = screen == S_HOME;
    if (screen == S_MENU && menuPickF < 0) menuPress = constrain(x * 3 / SW, 0, 2); Serial.printf("[touch] down x=%d y=%d screen=%d\n", x, y, (int)screen); }
  else if (t) {
    lastX = x;
    if (screen == S_FEED) dragDX = x - downX;                                  // carousel follows the finger
    if (abs(x - downX) > 12) { pokePending = false; menuPress = -1; }                             // it's a swipe: no poke, only the swipe tear
    else if (pokePending && millis() - downAt > 120) { pokePending = false; addPoke(downX / (float)S, (downY - OY) / (float)S); } // held still = poke
  }
  else if (!t && down) {
    down = false; menuPress = -1; if (pokePending && abs(lastX - downX) <= 12) addPoke(downX / (float)S, (downY - OY) / (float)S); pokePending = false;
    Serial.printf("[touch] up dx=%d screen=%d\n", lastX - downX, (int)screen); releaseTouch(lastX - downX, downX);
  }
}
void releaseTouch(int dx, int x) {   // x = where the finger went down (0..319)
  {
    bool swipe = abs(dx) > 40; int dir = dx < 0 ? 1 : -1;
    switch (screen) {
      case S_MENU:   // tap a card = it grows and opens; swipe = move the highlight
        if (menuPickF >= 0) return;
        if (swipe) menuSel = (menuSel + dir + 3) % 3;
        else { menuSel = constrain(x * 3 / SW, 0, 2); menuPickF = 0; }
        return;
      case S_FEED: {  // drag/swipe = slide the carousel, tap a side food = bring it in, tap the middle = send
        foodPos -= dx / (SLOT * S); dragDX = 0;
        int before = foodTarget;
        if (swipe) foodTarget += dir * max(1, (int)lroundf(fabsf(dx) / (SLOT * S)));
        else if (x < 90) foodTarget--;
        else if (x > SW - 90) foodTarget++;
        else if (foodSentF < 0 && fabsf(foodTarget - foodPos) < .35f) { tp_send_food(foodSel); foodSentF = 0; foodPopF = -1; Serial.printf("[face] sent food %s\n", FOODS[foodSel].name); }
        if (foodTarget != before) { foodSentF = foodPopF = -1; }
        foodSel = (foodTarget % NFOODS + NFOODS) % NFOODS;
        return; }
      case S_SETTINGS: {
        if (swipe) { go(S_MENU); return; }
        int gy = (downY - OY) / S, r = (gy - ROW_Y0 + 1) / ROW_H;
        if (gy >= ROW_Y0 - 1 && r >= 0 && r < NROWS) changeSetting(r);
        else if (gy >= ROW_Y0 + NROWS * ROW_H && tp_setup_ap()) { setupHidden = false; go(S_HOME); } // status line: back to the QR
        return; }
      default: break;
    }
    if (swipe) {
      myMood = (myMood + dir + SWIPE_MOODS) % SWIPE_MOODS; showingPartner = false; reactUntil = -1;
      swipeTo(myMood, -dir); lastSwipeF = f; Serial.printf("[face] swipe -> %s\n", MOODS[myMood].name);
    }
    // a tap keeps the current mood: the poke (ripple + push) already fired on touch-down
  }
}

// ---------- test hooks over USB serial: q = partner's mood (next), o = open menu card, 1-5 = settings row, a = food arrives, b = button, h = home, l/r = swipe left/right, t = tap, d = dump the 80x42 grid ----------
static void fakeTouch(int dx, int x = 160) { releaseTouch(dx, x); } // same as lifting a finger after moving dx (0 = tap)
static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'b') pressButton();
    else if (c == 'h') go(S_HOME);
    else if (c == 'a') fakeFood = random(NFOODS);                // pretend the partner sent food
    else if (c == 'q') { fakePartner = true; partnerMood = (partnerMood + 1) % SWIPE_MOODS; Serial.printf("[face] pretend partner mood %s\n", MOODS[partnerMood].name); }
    else if (c == 'o') { if (screen == S_MENU && menuPickF < 0) menuPickF = 0; }        // open the highlighted menu card
    else if (c >= '1' && c <= '5') { if (screen == S_SETTINGS) changeSetting(c - '1'); }   // tap a settings row
    else if (c == 'k') tp_print_status();                                       // link status
    else if (c == 'p') { if (screen == S_HOME) addPoke(random(W), random(H)); }          // poke at a random spot
    else if (c == 's') Serial.printf("[stats] frame max %lu ms avg %lu ms spi %lu ms push %.1f,%.1f vel %.1f,%.1f\n", statMax, statN ? statSum / statN : 0, statPush, pushX, pushY, velX, velY), statMax = statSum = statN = 0;
    else if (c == 'l') fakeTouch(-80); else if (c == 'r') fakeTouch(80); else if (c == 't') fakeTouch(0);
    else if (c == 'd') { Serial.printf("[dump] bg=%04x\n", shownBg); for (int y = 0; y < H; y++) { char row[W + 1]; for (int x = 0; x < W; x++) row[x] = '0' + buf[y * W + x]; row[W] = 0; Serial.println(row); } Serial.println("[end]"); }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);
  if (!gfx->begin(80000000)) Serial.println("[face] gfx->begin() failed"); // 80 MHz SPI: full frame in ~11 ms instead of 30
  gfx->fillScreen(0x0000);
  ledcAttach(PIN_BL, 20000, 8); ledcWrite(PIN_BL, 0);   // PWM backlight (active LOW: 0 = full on); level from settings below
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(20); // never let a touch read stall the frame loop
  pinMode(PIN_BTN, INPUT_PULLUP);
  prefs.begin("tamapair", false); blushOn = prefs.getBool("blush", true);
  swapS = prefs.getUChar("swap", 1) % 4; tintC = prefs.getUChar("tintc", 0) % 5; tintK = prefs.getUChar("tintk", 0) % 3; bright = prefs.getUChar("bright", 2) % 3;
  COL[5] = tintCol(1); applyBright();
  for (int i = 0; i < 10 && !touchOk; i++) { Wire.beginTransmission(TOUCH_ADDR); Wire.write(0x00); Wire.write(0x00); touchOk = Wire.endTransmission() == 0; if (!touchOk) delay(100); }
  Serial.printf("[face] up. touch %s\n", touchOk ? "ok" : "not found");
  randomSeed(esp_random());
  tp_begin(OFFLINE); // Wi-Fi + mailbox in the background (tp_link.cpp)
}

void loop() {
  static uint32_t next = 0; uint32_t now = millis();
  if (now < next) { delay(1); return; }
  next = now + 33; // 30 fps
  handleButton();
  handleTouch();
  handleSerial();
  tick();
  homeLogic();
  if (foodSentF >= 0 && ++foodSentF > 14) { foodSentF = -1; foodPopF = 0; }
  if (foodPopF >= 0 && ++foodPopF > 5) foodPopF = -1;
  if (menuPickF >= 0 && ++menuPickF > 5) go(menuSel == 0 ? S_HOME : menuSel == 1 ? S_FEED : S_SETTINGS);
  renderFrame();
  uint32_t took = millis() - now; statSum += took; statN++; if (took > statMax) statMax = took;
}
