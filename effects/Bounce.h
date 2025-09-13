#ifndef EFFECT_BOUNCE_H
#define EFFECT_BOUNCE_H

#include "Effect.h"
#include "../display/Matrix.h"

namespace BounceEffect {
  extern uint8_t x, y;
  extern int8_t dx, dy;
  void init();
  void draw(uint8_t *frame);
}

inline uint8_t BounceEffect::x;
inline uint8_t BounceEffect::y;
inline int8_t BounceEffect::dx;
inline int8_t BounceEffect::dy;

inline void BounceEffect::init() {
  x = 0;
  y = 0;
  dx = 1;
  dy = 1;
}

inline void BounceEffect::draw(uint8_t *frame) {
  setPixel(frame, x, y, true);
  x += dx;
  y += dy;
  if (x == 0 || x == 15) dx = -dx;
  if (y == 0 || y == 15) dy = -dy;
}

inline Effect bounceEffect = {BounceEffect::init, BounceEffect::draw, "bounce"};

#endif // EFFECT_BOUNCE_H
