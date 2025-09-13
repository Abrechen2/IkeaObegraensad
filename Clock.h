#ifndef EFFECT_CLOCK_H
#define EFFECT_CLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include <time.h>

namespace ClockEffect {
  const uint8_t DIGITS[10][5] = {
    {0b111,0b101,0b101,0b101,0b111},
    {0b010,0b110,0b010,0b010,0b111},
    {0b111,0b001,0b111,0b100,0b111},
    {0b111,0b001,0b111,0b001,0b111},
    {0b101,0b101,0b111,0b001,0b001},
    {0b111,0b100,0b111,0b001,0b111},
    {0b111,0b100,0b111,0b101,0b111},
    {0b111,0b001,0b010,0b010,0b010},
    {0b111,0b101,0b111,0b101,0b111},
    {0b111,0b101,0b111,0b001,0b111}
  };

  void drawDigit(uint8_t *frame, int digit, uint8_t xOffset) {
    for (uint8_t y = 0; y < 5; ++y) {
      for (uint8_t x = 0; x < 3; ++x) {
        bool on = DIGITS[digit][y] & (1 << (2 - x));
        if (on) setPixel(frame, x + xOffset, y + 5, true);
      }
    }
  }

  void drawColon(uint8_t *frame) {
    setPixel(frame, 7, 6, true);
    setPixel(frame, 7, 8, true);
  }

  inline void init() {
    // Time is synchronized globally via NTP in the main sketch
  }

  inline void draw(uint8_t *frame) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    int h = t->tm_hour;
    int m = t->tm_min;
    drawDigit(frame, h / 10, 0);
    drawDigit(frame, h % 10, 4);
    drawColon(frame);
    drawDigit(frame, m / 10, 9);
    drawDigit(frame, m % 10, 13);
  }
}

inline Effect clockEffect = {ClockEffect::init, ClockEffect::draw, "clock"};

#endif // EFFECT_CLOCK_H
