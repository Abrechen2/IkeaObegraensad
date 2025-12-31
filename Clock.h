#ifndef EFFECT_CLOCK_H
#define EFFECT_CLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include "ClockFont.h"
#include <time.h>

extern bool use24HourFormat;
uint8_t formatHourForDisplay(uint8_t hour);
struct tm* getLocalTime(time_t utcTime); // Vorwärtsdeklaration

namespace ClockEffect {
  void drawDigit(uint8_t *frame, int digit, uint8_t xOffset, uint8_t yOffset) {
    for (uint8_t y = 0; y < ClockFont::HEIGHT; ++y) {
      uint8_t row = ClockFont::DIGITS[digit][y];
      for (uint8_t x = 0; x < ClockFont::WIDTH; ++x) {
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
    // Zeitvalidierung: Prüfe ob Zeit plausibel ist
    if (now < 100000) {
      // Zeit nicht synchronisiert, zeige nichts oder Fehler
      return;
    }
    struct tm *tm_info = gmtime(&now);
    if (tm_info) {
      int year = tm_info->tm_year + 1900;
      if (year < 2020 || year >= 2100) {
        // Zeit außerhalb des erwarteten Bereichs
        return;
      }
    }
    // Verwende getLocalTime() für manuelle Zeitzonenberechnung
    struct tm *t = getLocalTime(now);
    int h = t ? formatHourForDisplay(t->tm_hour) : 0;
    int m = t ? t->tm_min : 0;
    // Render two digits per row: hours on top, minutes on bottom.
    // Digits are centered horizontally with a small gap between them.
    const uint8_t digitWidth = ClockFont::WIDTH;
    const uint8_t digitHeight = ClockFont::HEIGHT;
    const uint8_t spacing = 2; // columns between digits
    const uint8_t totalWidth = digitWidth * 2 + spacing;
    const uint8_t startX = (16 - totalWidth) / 2; // center within 16x16 matrix
    drawDigit(frame, h / 10, startX, 0);
    drawDigit(frame, h % 10, startX + digitWidth + spacing, 0);
    drawDigit(frame, m / 10, startX, digitHeight);
    drawDigit(frame, m % 10, startX + digitWidth + spacing, digitHeight);
  }
}

inline Effect clockEffect = {ClockEffect::init, ClockEffect::draw, "clock"};

#endif // EFFECT_CLOCK_H
