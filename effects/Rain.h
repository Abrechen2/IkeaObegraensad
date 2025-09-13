#ifndef EFFECT_RAIN_H
#define EFFECT_RAIN_H

#include "Effect.h"
#include "../display/Matrix.h"

namespace RainEffect {
  struct Drop { uint8_t x; int8_t y; };
  const uint8_t MAX_DROPS = 16;
  extern Drop drops[MAX_DROPS];
  void init();
  void draw(uint8_t *frame);
}

inline RainEffect::Drop RainEffect::drops[RainEffect::MAX_DROPS];

inline void RainEffect::init() {
  randomSeed(micros());
  for (uint8_t i = 0; i < MAX_DROPS; ++i) {
    drops[i].x = random(0, 16);
    drops[i].y = random(-16, 16);
  }
}

inline void RainEffect::draw(uint8_t *frame) {
  for (uint8_t i = 0; i < MAX_DROPS; ++i) {
    if (drops[i].y >= 0 && drops[i].y < 16) {
      setPixel(frame, drops[i].x, drops[i].y, true);
    }
    drops[i].y++;
    if (drops[i].y >= 16) {
      drops[i].x = random(0,16);
      drops[i].y = random(-8,0);
    }
  }
}

inline Effect rainEffect = {RainEffect::init, RainEffect::draw, "rain"};

#endif // EFFECT_RAIN_H
