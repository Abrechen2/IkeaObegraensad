#ifndef EFFECT_SNAKE_H
#define EFFECT_SNAKE_H

#include "Effect.h"
#include "../display/Matrix.h"

namespace SnakeEffect {
  const uint8_t LENGTH = 8;
  extern uint16_t body[LENGTH];
  void init();
  void draw(uint8_t *frame);
}

inline uint16_t SnakeEffect::body[SnakeEffect::LENGTH];

inline void SnakeEffect::init() {
  for (uint8_t i = 0; i < LENGTH; ++i) {
    body[i] = i;
  }
}

inline void SnakeEffect::draw(uint8_t *frame) {
  for (uint8_t i = 0; i < LENGTH; ++i) {
    uint8_t x = body[i] % 16;
    uint8_t y = body[i] / 16;
    setPixel(frame, x, y, true);
  }
  for (uint8_t i = 0; i < LENGTH - 1; ++i) {
    body[i] = body[i + 1];
  }
  body[LENGTH - 1] = (body[LENGTH - 1] + 1) % 256;
}

inline Effect snakeEffect = {SnakeEffect::init, SnakeEffect::draw, "snake"};

#endif // EFFECT_SNAKE_H
