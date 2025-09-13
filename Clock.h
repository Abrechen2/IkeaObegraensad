#ifndef EFFECT_CLOCK_H
#define EFFECT_CLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include <time.h>

namespace ClockEffect {
  // 6x7 pixel font for digits 0-9
  static const uint8_t DIGITS[10][6] = {
    {0x3e,0x7f,0x63,0x63,0x7f,0x3e}, // 0
    {0x00,0x02,0x7f,0x7f,0x00,0x00}, // 1
    {0x62,0x73,0x7b,0x6b,0x6f,0x66}, // 2
    {0x22,0x63,0x6b,0x6b,0x7f,0x36}, // 3
    {0x0f,0x0f,0x08,0x08,0x7f,0x7f}, // 4
    {0x2f,0x6f,0x6b,0x6b,0x7b,0x3b}, // 5
    {0x3e,0x7f,0x6b,0x6b,0x7b,0x3a}, // 6
    {0x03,0x03,0x7b,0x7b,0x0f,0x07}, // 7
    {0x36,0x7f,0x6b,0x6b,0x7f,0x36}, // 8
    {0x26,0x6f,0x6b,0x6b,0x7f,0x3e}  // 9
  };

  void drawDigit(uint8_t *frame, int digit, uint8_t xOffset, uint8_t yOffset) {
    for (uint8_t x = 0; x < 6; ++x) {
      uint8_t col = DIGITS[digit][x];
      for (uint8_t y = 0; y < 7; ++y) {
        if (col & (1 << y)) {
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
    // Center two digits horizontally. Hours on top, minutes on bottom.
    const uint8_t x0 = 2;      // left margin
    const uint8_t gap = 7;     // digit width (6) + 1 space
    drawDigit(frame, h / 10, x0, 0);
    drawDigit(frame, h % 10, x0 + gap, 0);
    drawDigit(frame, m / 10, x0, 9);
    drawDigit(frame, m % 10, x0 + gap, 9);
  }
}

inline Effect clockEffect = {ClockEffect::init, ClockEffect::draw, "clock"};

#endif // EFFECT_CLOCK_H
