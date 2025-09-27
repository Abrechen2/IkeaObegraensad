#ifndef EFFECT_STARS_H
#define EFFECT_STARS_H

#include <cstring>

#include "Effect.h"
#include "Matrix.h"

namespace StarsEffect {
  struct Star { uint8_t x, y, life; bool on; };
  const uint8_t MAX_STARS = 20;
  extern Star stars[MAX_STARS];
  void init();
  void draw(uint8_t *frame);
}

inline StarsEffect::Star StarsEffect::stars[StarsEffect::MAX_STARS];

inline void StarsEffect::init() {
  memset(stars, 0, sizeof(stars));

  randomSeed(micros());
  for (uint8_t i = 0; i < MAX_STARS; ++i) {
    stars[i].x = random(0,16);
    stars[i].y = random(0,16);
    stars[i].life = random(5,20);
    stars[i].on = random(0,2);
  }

  Serial.printf("Stars effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void StarsEffect::draw(uint8_t *frame) {
  for (uint8_t i = 0; i < MAX_STARS; ++i) {
    if (stars[i].on) {
      setPixel(frame, stars[i].x, stars[i].y, true);
    }
    if (stars[i].life > 0) {
      stars[i].life--;
    } else {
      stars[i].on = !stars[i].on;
      stars[i].x = random(0,16);
      stars[i].y = random(0,16);
      stars[i].life = stars[i].on ? random(5,20) : random(5,30);
    }
  }
}

inline Effect starsEffect = {StarsEffect::init, StarsEffect::draw, "stars"};

#endif // EFFECT_STARS_H
