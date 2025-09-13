#ifndef EFFECT_LINES_H
#define EFFECT_LINES_H

#include "Effect.h"
#include "Matrix.h"

namespace LinesEffect {
  extern uint8_t offset;
  void init();
  void draw(uint8_t *frame);
}

inline uint8_t LinesEffect::offset;

inline void LinesEffect::init() {
  offset = 0;
}

inline void LinesEffect::draw(uint8_t *frame) {
  for (uint8_t x = 0; x < 16; ++x) {
    if (((x + offset) & 3) == 0) {
      for (uint8_t y = 0; y < 16; ++y) {
        setPixel(frame, x, y, true);
      }
    }
  }
  offset = (offset + 1) & 3;
}

inline Effect linesEffect = {LinesEffect::init, LinesEffect::draw, "lines"};

#endif // EFFECT_LINES_H
