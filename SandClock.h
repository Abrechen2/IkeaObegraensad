#ifndef EFFECT_SANDCLOCK_H
#define EFFECT_SANDCLOCK_H

#include "Effect.h"
#include "Matrix.h"
#include "ClockFont.h"
#include <time.h>

extern bool use24HourFormat;
uint8_t formatHourForDisplay(uint8_t hour);
struct tm* getLocalTime(time_t utcTime); // Vorwärtsdeklaration

namespace SandClockEffect {
  struct Grain {
    float x, y;
    float vx, vy;
    bool active;
    uint8_t settleTime;
  };
  
  const uint8_t MAX_GRAINS = 64;
  extern Grain grains[MAX_GRAINS];
  extern uint8_t staticFrame[32];
  extern uint8_t lastMinute;
  extern uint8_t animationState; // 0=static, 1=falling, 2=settling
  extern uint8_t animationTimer;
  
  void init();
  void draw(uint8_t *frame);
  void drawDigitToBuffer(uint8_t *buffer, int digit, uint8_t xOffset, uint8_t yOffset);
  void startSandTransition();
  void updatePhysics();
  void drawStatic(uint8_t *frame);
  bool isPixelSet(uint8_t *buffer, uint8_t x, uint8_t y);
  void createGrainsFromDigit(int oldDigit, int newDigit, uint8_t xOffset, uint8_t yOffset);
}

// Globale Variablen
inline SandClockEffect::Grain SandClockEffect::grains[SandClockEffect::MAX_GRAINS];
inline uint8_t SandClockEffect::staticFrame[32];
inline uint8_t SandClockEffect::lastMinute = 255;
inline uint8_t SandClockEffect::animationState = 0;
inline uint8_t SandClockEffect::animationTimer = 0;

inline void SandClockEffect::init() {
  memset(grains, 0, sizeof(grains));
  memset(staticFrame, 0, sizeof(staticFrame));
  lastMinute = 255; // Trigger initial setup
  animationState = 0;
  animationTimer = 0;
  randomSeed(micros());
  Serial.printf("SandClock effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline bool SandClockEffect::isPixelSet(uint8_t *buffer, uint8_t x, uint8_t y) {
  if (x >= 16 || y >= 16) return false;
  uint8_t index = PIXEL_MAP[y][x];
  uint8_t mask = 0x80 >> (index & 7);
  return buffer[index >> 3] & mask;
}

inline void SandClockEffect::drawDigitToBuffer(uint8_t *buffer, int digit, uint8_t xOffset, uint8_t yOffset) {
  for (uint8_t y = 0; y < ClockFont::HEIGHT; ++y) {
    uint8_t row = ClockFont::DIGITS[digit][y];
    for (uint8_t x = 0; x < ClockFont::WIDTH; ++x) {
      if (row & (0x80 >> x)) {
        setPixel(buffer, x + xOffset, y + yOffset, true);
      }
    }
  }
}

inline void SandClockEffect::createGrainsFromDigit(int oldDigit, int newDigit, uint8_t xOffset, uint8_t yOffset) {
  uint8_t oldBuffer[32], newBuffer[32];
  memset(oldBuffer, 0, sizeof(oldBuffer));
  memset(newBuffer, 0, sizeof(newBuffer));
  
  drawDigitToBuffer(oldBuffer, oldDigit, xOffset, yOffset);
  drawDigitToBuffer(newBuffer, newDigit, xOffset, yOffset);
  
  uint8_t grainIndex = 0;
  for (uint8_t y = 0; y < ClockFont::HEIGHT && grainIndex < MAX_GRAINS; ++y) {
    for (uint8_t x = 0; x < ClockFont::WIDTH && grainIndex < MAX_GRAINS; ++x) {
      uint8_t pixelX = x + xOffset;
      uint8_t pixelY = y + yOffset;
      
      bool oldPixel = isPixelSet(oldBuffer, pixelX, pixelY);
      bool newPixel = isPixelSet(newBuffer, pixelX, pixelY);
      
      // Nur Pixel die verschwinden werden zu Sandkörnern
      if (oldPixel && !newPixel) {
        grains[grainIndex].x = pixelX + 0.5;
        grains[grainIndex].y = pixelY + 0.5;
        grains[grainIndex].vx = random(-50, 51) / 100.0; // -0.5 bis 0.5
        grains[grainIndex].vy = random(0, 100) / 100.0;  // 0 bis 1.0
        grains[grainIndex].active = true;
        grains[grainIndex].settleTime = 0;
        grainIndex++;
      }
    }
  }
}

inline void SandClockEffect::startSandTransition() {
  time_t now = time(nullptr);
  // Zeitvalidierung: Prüfe ob Zeit plausibel ist
  if (now < 100000) return; // Zeit nicht synchronisiert
  struct tm *tm_info = gmtime(&now);
  if (tm_info) {
    int year = tm_info->tm_year + 1900;
    if (year < 2020 || year >= 2100) return; // Zeit außerhalb des erwarteten Bereichs
  }
  struct tm *t = localtime(&now);
  if (!t) return;
  
  int h = t->tm_hour;
  int m = t->tm_min;
  int displayHour = formatHourForDisplay(h);
  
  const uint8_t digitWidth = ClockFont::WIDTH;
  const uint8_t digitHeight = ClockFont::HEIGHT;
  const uint8_t spacing = 2;
  const uint8_t totalWidth = digitWidth * 2 + spacing;
  const uint8_t startX = (16 - totalWidth) / 2;
  
  // Alte Zeit ermitteln
  int oldH = h;
  int oldM = m - 1;
  if (oldM < 0) {
    oldM = 59;
    oldH--;
    if (oldH < 0) oldH = 23;
  }
  int oldDisplayHour = formatHourForDisplay(oldH);
  
  // Sandkörner für geänderte Ziffern erstellen
  memset(grains, 0, sizeof(grains));
  
  // Stunden-Zehner
  if (oldDisplayHour / 10 != displayHour / 10) {
    createGrainsFromDigit(oldDisplayHour / 10, displayHour / 10, startX, 0);
  }
  // Stunden-Einer
  if (oldDisplayHour % 10 != displayHour % 10) {
    createGrainsFromDigit(oldDisplayHour % 10, displayHour % 10, startX + digitWidth + spacing, 0);
  }
  // Minuten-Zehner
  if (oldM / 10 != m / 10) {
    createGrainsFromDigit(oldM / 10, m / 10, startX, digitHeight);
  }
  // Minuten-Einer
  if (oldM % 10 != m % 10) {
    createGrainsFromDigit(oldM % 10, m % 10, startX + digitWidth + spacing, digitHeight);
  }
  
  animationState = 1; // Start falling animation
  animationTimer = 0;
}

inline void SandClockEffect::updatePhysics() {
  bool anyActive = false;
  
  for (uint8_t i = 0; i < MAX_GRAINS; i++) {
    if (!grains[i].active) continue;
    
    anyActive = true;
    
    // Physik: Schwerkraft und Bewegung
    grains[i].vy += 0.05; // Schwerkraft
    grains[i].x += grains[i].vx;
    grains[i].y += grains[i].vy;
    
    // Kollision mit Boden
    if (grains[i].y >= 15.5) {
      grains[i].y = 15.5;
      grains[i].vy = 0;
      grains[i].vx *= 0.8; // Reibung
      grains[i].settleTime++;
      
      if (grains[i].settleTime > 10) {
        grains[i].active = false;
      }
    }
    
    // Kollision mit Wänden
    if (grains[i].x <= 0) {
      grains[i].x = 0;
      grains[i].vx = 0;
    }
    if (grains[i].x >= 15) {
      grains[i].x = 15;
      grains[i].vx = 0;
    }
  }
  
  // Animation beenden wenn alle Körner zur Ruhe gekommen sind
  if (!anyActive) {
    animationState = 0;
    animationTimer = 0;
  }
}

inline void SandClockEffect::drawStatic(uint8_t *frame) {
  time_t now = time(nullptr);
  // Zeitvalidierung: Prüfe ob Zeit plausibel ist
  if (now < 100000) return; // Zeit nicht synchronisiert
  struct tm *tm_info = gmtime(&now);
  if (tm_info) {
    int year = tm_info->tm_year + 1900;
    if (year < 2020 || year >= 2100) return; // Zeit außerhalb des erwarteten Bereichs
  }
  // Verwende getLocalTime() für manuelle Zeitzonenberechnung
  struct tm *t = getLocalTime(now);
  int h = t ? formatHourForDisplay(t->tm_hour) : 0;
  int m = t ? t->tm_min : 0;
  
  const uint8_t digitWidth = ClockFont::WIDTH;
  const uint8_t digitHeight = ClockFont::HEIGHT;
  const uint8_t spacing = 2;
  const uint8_t totalWidth = digitWidth * 2 + spacing;
  const uint8_t startX = (16 - totalWidth) / 2;
  
  drawDigitToBuffer(frame, h / 10, startX, 0);
  drawDigitToBuffer(frame, h % 10, startX + digitWidth + spacing, 0);
  drawDigitToBuffer(frame, m / 10, startX, digitHeight);
  drawDigitToBuffer(frame, m % 10, startX + digitWidth + spacing, digitHeight);
}

inline void SandClockEffect::draw(uint8_t *frame) {
  time_t now = time(nullptr);
  // Zeitvalidierung: Prüfe ob Zeit plausibel ist
  if (now < 100000) {
    // Zeit nicht synchronisiert, zeige statische Anzeige ohne Animation
    drawStatic(frame);
    return;
  }
  struct tm *tm_info = gmtime(&now);
  if (tm_info) {
    int year = tm_info->tm_year + 1900;
    if (year < 2020 || year >= 2100) {
      drawStatic(frame);
      return;
    }
  }
  // Verwende getLocalTime() für manuelle Zeitzonenberechnung
  struct tm *t = getLocalTime(now);
  uint8_t currentMinute = t ? t->tm_min : 0;

  // Prüfe ob sich die Minute geändert hat
  if (lastMinute != 255 && lastMinute != currentMinute && animationState == 0) {
    startSandTransition();
  }
  lastMinute = currentMinute;
  
  if (animationState == 0) {
    // Normale Uhr anzeigen
    drawStatic(frame);
  } else {
    // Sand-Animation
    updatePhysics();
    
    // Neue Zeit als Basis anzeigen
    drawStatic(frame);
    
    // Fallende Sandkörner darüber zeichnen
    for (uint8_t i = 0; i < MAX_GRAINS; i++) {
      if (grains[i].active) {
        uint8_t x = (uint8_t)(grains[i].x + 0.5);
        uint8_t y = (uint8_t)(grains[i].y + 0.5);
        if (x < 16 && y < 16) {
          setPixel(frame, x, y, true);
        }
      }
    }
    
    animationTimer++;
  }
}

inline Effect sandClockEffect = {SandClockEffect::init, SandClockEffect::draw, "sandclock"};

#endif // EFFECT_SANDCLOCK_H