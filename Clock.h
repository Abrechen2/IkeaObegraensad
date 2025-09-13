#ifndef EFFECT_CLOCK_H
#define EFFECT_CLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include <time.h>

namespace ClockEffect {
  // 5x7 pixel font for digits 0-9
  const uint8_t DIGITS[10][7] = {
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, // 0
    {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}, // 1
    {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}, // 2
    {0x0e,0x11,0x01,0x06,0x01,0x11,0x0e}, // 3
    {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}, // 4
    {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}, // 5
    {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}, // 6
    {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, // 8
    {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}  // 9
  };

  void drawDigit(uint8_t *frame, int digit, uint8_t xOffset, uint8_t yOffset) {
    for (uint8_t y = 0; y < 7; ++y) {
      uint8_t row = DIGITS[digit][y];
      for (uint8_t x = 0; x < 5; ++x) {
        bool on = row & (1 << (4 - x));
        if (on) setPixel(frame, x + xOffset, y + yOffset, true);
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
    // Center two digits horizontally. Hours on top, minutes on bottom.
    const uint8_t x0 = 2;      // left margin
    const uint8_t gap = 6;     // digit width (5) + 1 space
    drawDigit(frame, h / 10, x0, 0);
    drawDigit(frame, h % 10, x0 + gap, 0);
    drawDigit(frame, m / 10, x0, 9);
    drawDigit(frame, m % 10, x0 + gap, 9);
  }
}

inline Effect clockEffect = {ClockEffect::init, ClockEffect::draw, "clock"};

#endif // EFFECT_CLOCK_H
