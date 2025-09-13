#ifndef EFFECT_CLOCK_H
#define EFFECT_CLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include "ClockFont.h"
#include <time.h>

namespace ClockEffect {
  void drawDigit(uint8_t *frame, int digit, uint8_t xOffset, uint8_t yOffset) {
    for (uint8_t y = 0; y < 8; ++y) {
      uint8_t row = ClockFont::DIGITS[digit][y];
      for (uint8_t x = 0; x < 8; ++x) {
        if (row & (0x80 >> x)) {
          setPixel(frame, x + xOffset, y + yOffset, true);
        }
      }
    }
  }

  inline void init() {
    // Time is synchronized globally via NTP in the main sketch
  }

  inline void draw(uint8_t *frame) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    int h = t ? t->tm_hour : 0;
    int m = t ? t->tm_min : 0;
    // Render two digits per row: hours on top, minutes on bottom.
    const uint8_t gap = 8;  // digit width
    drawDigit(frame, h / 10, 0, 0);
    drawDigit(frame, h % 10, gap, 0);
    drawDigit(frame, m / 10, 0, gap);
    drawDigit(frame, m % 10, gap, gap);
  }
}

inline Effect clockEffect = {ClockEffect::init, ClockEffect::draw, "clock"};

#endif // EFFECT_CLOCK_H
