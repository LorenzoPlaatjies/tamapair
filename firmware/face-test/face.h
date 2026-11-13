// Retro pixel faces — same shapes as tools/face-picker, drawn on the board.
// Owned by Dr. Smith's agent. Lorenzo's agent: call face_* from your code, don't edit here.
#pragma once
#include <Arduino_GFX_Library.h>

enum FaceId : uint8_t {
  F_IDLE, F_HAPPY, F_GIGGLE, F_LOVE, F_EXCITED, F_WINK, F_SHY, F_SURPRISED, F_SLEEPY, F_ASLEEP,
  F_HUNGRY, F_EATING, F_JAM, F_DIZZY, F_SAD, F_CRYING, F_ANGRY, F_BORED, F_CONFUSED, F_NERVOUS,
  F_COUNT
};

// Call once after the screen is up, with the screen's width/height. The face fills the screen.
bool face_begin(Arduino_GFX *screen, int16_t w, int16_t h);
// Draw a face. blink = eyes shut this frame. bob = nudge down one big pixel.
void face_draw(uint8_t id, bool blink = false, bool bob = false, bool blush = true);
const char *face_name(uint8_t id);
bool face_can_blink(uint8_t id);
