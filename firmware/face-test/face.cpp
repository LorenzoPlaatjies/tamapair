// Draw each face at 240x180, then fill the whole screen with 4x4 "big pixels" sampled from it (retro look).
#include "face.h"

static const int W = 240, H = 180, EYE_LX = 82, EYE_RX = 158, EY = 78;
static const int CELL = 4;
static int GW, GH;            // big-pixel grid that covers the screen
static float S, DX, DY;       // canvas → screen: screen = canvas * S + D

static Arduino_GFX *scr;
static Arduino_Canvas *cv;      // 240x180 drawing surface (never shown directly)
static uint16_t *fb;
static uint16_t *rowbuf;
static int16_t *colX, *rowY;  // canvas x for each screen column, canvas y for each screen row (-1 = off canvas)

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t BLK = 0x0000, WHT = 0xFFFF;
static uint16_t PINK, HEART, GOLD, BLUE, TONGUE, GREY, RED, CRUMB;

struct Face { const char *name; uint8_t eL, eR, mouth, extra; bool shyBlush, noBlink; };
enum { E_ROUND, E_BIG, E_HAPPY, E_CLOSED, E_FLAT, E_HALF, E_HEART, E_STAR, E_SPIRAL, E_X,
       E_ANGRYL, E_ANGRYR, E_SADL, E_SADR, E_SIDE, E_TEAR };
enum { M_SMILE, M_BIG, M_O, M_SMALL, M_FLAT, M_FROWN, M_CAT, M_WAVY, M_TONGUE, M_DROOL, M_CHEW, M_TEETH };
enum { X_NONE, X_ZZZ, X_NOTES, X_SWEAT, X_VEIN, X_Q, X_SPARK, X_CRUMBS };

static const Face FACES[F_COUNT] = {
  {"Idle",        E_ROUND,  E_ROUND,  M_SMALL,  X_NONE,   false, false},
  {"Happy",       E_HAPPY,  E_HAPPY,  M_SMILE,  X_NONE,   false, false},
  {"Giggle",      E_HAPPY,  E_HAPPY,  M_BIG,    X_NONE,   true,  false},
  {"In love",     E_HEART,  E_HEART,  M_CAT,    X_NONE,   false, false},
  {"Excited",     E_STAR,   E_STAR,   M_BIG,    X_SPARK,  false, false},
  {"Wink",        E_ROUND,  E_HAPPY,  M_TONGUE, X_NONE,   false, false},
  {"Shy",         E_SIDE,   E_SIDE,   M_WAVY,   X_NONE,   true,  false},
  {"Surprised",   E_BIG,    E_BIG,    M_O,      X_NONE,   false, false},
  {"Sleepy",      E_HALF,   E_HALF,   M_SMALL,  X_ZZZ,    false, false},
  {"Asleep",      E_CLOSED, E_CLOSED, M_SMALL,  X_ZZZ,    false, true},
  {"Hungry",      E_BIG,    E_BIG,    M_DROOL,  X_NONE,   false, false},
  {"Munching",    E_CLOSED, E_CLOSED, M_CHEW,   X_CRUMBS, false, true},
  {"Jamming",     E_CLOSED, E_CLOSED, M_SMILE,  X_NOTES,  false, true},
  {"Dizzy",       E_SPIRAL, E_SPIRAL, M_WAVY,   X_NONE,   false, true},
  {"Sad",         E_SADL,   E_SADR,   M_FROWN,  X_NONE,   false, false},
  {"Crying",      E_TEAR,   E_TEAR,   M_FROWN,  X_NONE,   false, false},
  {"Grumpy",      E_ANGRYL, E_ANGRYR, M_TEETH,  X_VEIN,   false, false},
  {"Bored",       E_FLAT,   E_FLAT,   M_FLAT,   X_NONE,   false, false},
  {"Confused",    E_ROUND,  E_HALF,   M_WAVY,   X_Q,      false, false},
  {"Nervous",     E_ROUND,  E_ROUND,  M_WAVY,   X_SWEAT,  false, false},
};

const char *face_name(uint8_t id) { return id < F_COUNT ? FACES[id].name : "?"; }
bool face_can_blink(uint8_t id) { return id < F_COUNT && !FACES[id].noBlink; }

// ---------- little drawing helpers (240x180 space) ----------
static void dot(float x, float y, float r, uint16_t c) { cv->fillCircle(lroundf(x), lroundf(y), max(1L, lroundf(r)), c); }
static void strokeQuad(float x0, float y0, float cx, float cy, float x1, float y1, float w, uint16_t c) {
  for (int i = 0; i <= 40; i++) {
    float t = i / 40.0f, u = 1 - t;
    dot(u * u * x0 + 2 * u * t * cx + t * t * x1, u * u * y0 + 2 * u * t * cy + t * t * y1, w / 2, c);
  }
}
static void fillQuad(float x0, float y0, float cx, float cy, float x1, float y1, uint16_t c) {
  float px = x0, py = y0;
  for (int i = 1; i <= 30; i++) {
    float t = i / 30.0f, u = 1 - t;
    float nx = u * u * x0 + 2 * u * t * cx + t * t * x1, ny = u * u * y0 + 2 * u * t * cy + t * t * y1;
    cv->fillTriangle(x0, y0, px, py, nx, ny, c);
    px = nx; py = ny;
  }
}
static void line(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  for (int i = 0; i <= 24; i++) { float t = i / 24.0f; dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, w / 2, c); }
}
static void drop(float x, float top, float h, uint16_t c) {   // little teardrop
  dot(x, top + h * 0.65f, h * 0.32f, c);
  cv->fillTriangle(x, top, x - h * 0.3f, top + h * 0.6f, x + h * 0.3f, top + h * 0.6f, c);
}
static void star(float x, float y, float ro, float ri, int pts, uint16_t c) {
  float px = 0, py = 0;
  for (int i = 0; i <= pts * 2; i++) {
    float r = (i % 2) ? ri : ro, a = -PI / 2 + i * PI / pts;
    float nx = x + r * cosf(a), ny = y + r * sinf(a);
    if (i) cv->fillTriangle(x, y, px, py, nx, ny, c);
    px = nx; py = ny;
  }
}
static void note(float x, float y) {
  line(x, y, x, y - 14, 3, GOLD); line(x, y - 14, x + 10, y - 17, 3, GOLD); line(x + 10, y - 17, x + 10, y - 3, 3, GOLD);
  dot(x - 2, y, 4, GOLD); dot(x + 8, y - 3, 4, GOLD);
}

static void eye(uint8_t t, int x, bool blink) {
  if (blink) { cv->fillRoundRect(x - 17, EY - 3, 34, 7, 3, WHT); return; }
  switch (t) {
    case E_ROUND:  cv->fillRoundRect(x - 17, EY - 26, 34, 52, 16, WHT); break;
    case E_BIG:    cv->fillRoundRect(x - 21, EY - 30, 42, 60, 20, WHT); cv->fillCircle(x + 7, EY - 12, 7, BLK); break;
    case E_HAPPY:  strokeQuad(x - 18, EY + 8, x, EY - 22, x + 18, EY + 8, 9, WHT); break;
    case E_CLOSED: strokeQuad(x - 18, EY, x, EY + 14, x + 18, EY, 8, WHT); break;
    case E_FLAT:   cv->fillRoundRect(x - 20, EY - 4, 40, 9, 4, WHT); break;
    case E_HALF:   cv->fillRoundRect(x - 17, EY - 8, 34, 32, 16, WHT); cv->fillRect(x - 18, EY - 9, 36, 9, BLK); break;
    case E_HEART:  cv->fillCircle(x - 9, EY - 6, 11, HEART); cv->fillCircle(x + 9, EY - 6, 11, HEART);
                   cv->fillTriangle(x - 19, EY - 1, x + 19, EY - 1, x, EY + 22, HEART); break;
    case E_STAR:   star(x, EY, 22, 9, 5, GOLD); break;
    case E_SPIRAL: for (int i = 0; i <= 120; i++) { float a = i * 0.16f, r = 1.5f + a * 0.85f; dot(x + r * cosf(a), EY + r * sinf(a), 2, WHT); } break;
    case E_X:      line(x - 14, EY - 14, x + 14, EY + 14, 8, WHT); line(x + 14, EY - 14, x - 14, EY + 14, 8, WHT); break;
    case E_ANGRYL: cv->fillRoundRect(x - 17, EY - 14, 34, 40, 10, WHT); cv->fillTriangle(x - 18, EY - 15, x + 18, EY - 15, x + 18, EY - 2, BLK); break;
    case E_ANGRYR: cv->fillRoundRect(x - 17, EY - 14, 34, 40, 10, WHT); cv->fillTriangle(x - 18, EY - 15, x + 18, EY - 15, x - 18, EY - 2, BLK); break;
    case E_SADL:   cv->fillRoundRect(x - 17, EY - 18, 34, 44, 10, WHT); cv->fillTriangle(x - 18, EY - 19, x + 18, EY - 19, x - 18, EY - 4, BLK); break;
    case E_SADR:   cv->fillRoundRect(x - 17, EY - 18, 34, 44, 10, WHT); cv->fillTriangle(x - 18, EY - 19, x + 18, EY - 19, x + 18, EY - 4, BLK); break;
    case E_SIDE:   cv->fillRoundRect(x - 17, EY - 26, 34, 52, 16, WHT); cv->fillRoundRect(x + 1, EY - 14, 12, 22, 6, BLK); break;
    case E_TEAR:   cv->fillRoundRect(x - 17, EY - 26, 34, 52, 16, WHT); drop(x + 12, EY + 30, 16, BLUE); break;
  }
}

static void mouth(uint8_t t) {
  const int x = 120, y = 132;
  switch (t) {
    case M_SMILE:  strokeQuad(x - 12, y, x, y + 12, x + 12, y, 6, WHT); break;
    case M_BIG:    fillQuad(x - 18, y - 4, x, y + 24, x + 18, y - 4, WHT); break;
    case M_O:      cv->fillEllipse(x, y + 2, 9, 11, WHT); break;
    case M_SMALL:  cv->fillRoundRect(x - 6, y, 12, 5, 2, WHT); break;
    case M_FLAT:   cv->fillRoundRect(x - 14, y, 28, 6, 3, WHT); break;
    case M_FROWN:  strokeQuad(x - 12, y + 8, x, y - 4, x + 12, y + 8, 6, WHT); break;
    case M_CAT:    strokeQuad(x - 14, y, x - 7, y + 9, x, y, 5, WHT); strokeQuad(x, y, x + 7, y + 9, x + 14, y, 5, WHT); break;
    case M_WAVY:   for (int i = 0; i <= 36; i++) dot(x - 18 + i, y + 3 - 3.5f * sinf(i * PI / 9), 2.5f, WHT); break;
    case M_TONGUE: strokeQuad(x - 14, y, x, y + 10, x + 14, y, 5, WHT); cv->fillRoundRect(x - 3, y + 5, 12, 11, 5, TONGUE); break;
    case M_DROOL:  cv->fillEllipse(x, y + 2, 9, 11, WHT); drop(x + 8, y + 10, 16, BLUE); break;
    case M_CHEW:   cv->fillRoundRect(x - 12, y - 2, 24, 10, 5, WHT); break;
    case M_TEETH:  cv->fillRoundRect(x - 16, y - 4, 32, 13, 4, WHT); cv->fillRect(x - 16, y + 2, 32, 2, BLK); break;
  }
}

static void extra(uint8_t t) {
  switch (t) {
    case X_ZZZ:    cv->setTextColor(GREY); cv->setTextSize(3); cv->setCursor(192, 26); cv->print('z');
                   cv->setTextSize(2); cv->setCursor(210, 10); cv->print('z'); break;
    case X_NOTES:  note(26, 52); note(206, 40); break;
    case X_SWEAT:  drop(200, 40, 20, BLUE); break;
    case X_VEIN:   line(196, 30, 204, 38, 3, RED); line(204, 30, 196, 38, 3, RED);
                   line(210, 34, 218, 42, 3, RED); line(218, 34, 210, 42, 3, RED); break;
    case X_Q:      cv->setTextColor(GOLD); cv->setTextSize(4); cv->setCursor(196, 18); cv->print('?'); break;
    case X_SPARK:  star(33, 48, 9, 3, 4, GOLD); star(207, 126, 7, 2, 4, GOLD); break;
    case X_CRUMBS: dot(150, 150, 3, CRUMB); dot(160, 142, 2.5f, CRUMB); dot(92, 152, 2, CRUMB); break;
  }
}

bool face_begin(Arduino_GFX *screen, int16_t sw, int16_t sh) {
  scr = screen;
  GW = sw / CELL; GH = sh / CELL;
  // face content lives in canvas y≈8..162 and x≈0..240: fit its height to the screen, centre it
  S = (float)(GH * CELL) / 150.0f;
  DX = (GW * CELL - W * S) / 2;
  DY = -9 * S;
  rowbuf = (uint16_t *)malloc(GW * CELL * CELL * 2);
  colX = (int16_t *)malloc(GW * CELL * 2); rowY = (int16_t *)malloc(GH * CELL * 2);
  for (int i = 0; i < GW * CELL; i++) { int v = (int)((i + 0.5f - DX) / S); colX[i] = (v >= 0 && v < W) ? v : -1; }
  for (int i = 0; i < GH * CELL; i++) { int v = (int)((i + 0.5f - DY) / S); rowY[i] = (v >= 0 && v < H) ? v : -1; }
  PINK = rgb(0xff, 0x7f, 0x9f); HEART = rgb(0xff, 0x5c, 0x8a); GOLD = rgb(0xff, 0xd1, 0x66);
  BLUE = rgb(0x7c, 0xc7, 0xff); TONGUE = rgb(0xff, 0x7a, 0xa2); GREY = rgb(0x9a, 0x98, 0xa3);
  RED = rgb(0xff, 0x5c, 0x5c); CRUMB = rgb(0xd9, 0xa1, 0x5b);
  cv = new Arduino_Canvas(W, H, nullptr);
  if (!cv->begin(GFX_SKIP_OUTPUT_BEGIN)) return false;
  fb = cv->getFramebuffer();
  return true;
}

// Colour of one big pixel: sample a 4x4 patch of the canvas under it; ≥6 of 16 lit = on (like the picker).
static uint16_t cellColour(int gx, int gy) {
  uint32_t r = 0, g = 0, b = 0, n = 0;
  for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 4; dx++) {
    int cx = colX[gx * CELL + dx], cy = rowY[gy * CELL + dy];
    if (cx < 0 || cy < 0) continue;
    uint16_t p = fb[cy * W + cx];
    if (p) { r += (p >> 11) & 31; g += (p >> 5) & 63; b += p & 31; n++; }
  }
  if (n < 6) return BLK;
  r /= n; g /= n; b /= n;
  uint8_t R = r << 3, G = g << 2, B = b << 3;
  R = (R >> 5) << 5 | 31; G = (G >> 5) << 5 | 31; B = (B >> 5) << 5 | 31;
  return rgb(R, G, B);
}

void face_draw(uint8_t id, bool blink, bool bob, bool blush) {
  if (id >= F_COUNT) return;
  const Face &f = FACES[id];
  cv->fillScreen(BLK);
  if (blush || f.shyBlush) { cv->fillEllipse(EYE_LX - 14, 118, 16, 8, PINK); cv->fillEllipse(EYE_RX + 14, 118, 16, 8, PINK); }
  bool shut = blink && !f.noBlink;
  eye(f.eL, EYE_LX, shut); eye(f.eR, EYE_RX, shut);
  mouth(f.mouth);
  extra(f.extra);

  // bob = everything shifts down one big pixel
  for (int row = 0; row < GH; row++) {
    int gy = bob ? row - 1 : row;
    for (int gx = 0; gx < GW; gx++) {
      uint16_t c = (gy >= 0 && gy < GH) ? cellColour(gx, gy) : BLK;
      for (int py = 0; py < CELL; py++) for (int px = 0; px < CELL; px++)
        rowbuf[py * GW * CELL + gx * CELL + px] = c;
    }
    scr->draw16bitRGBBitmap(0, row * CELL, rowbuf, GW * CELL, CELL);
  }
}
