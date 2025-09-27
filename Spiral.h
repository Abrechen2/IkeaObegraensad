#ifndef EFFECT_SPIRAL_H
#define EFFECT_SPIRAL_H

#include "Effect.h"
#include "Matrix.h"
#include <math.h>

namespace SpiralEffect {
  extern float offset;
  void init();
  void draw(uint8_t *frame);
}

inline float SpiralEffect::offset;

inline void SpiralEffect::init() {
  offset = 0.0;
  Serial.printf("Spiral effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void SpiralEffect::draw(uint8_t *frame) {
  const float centerX = 7.5;
  const float centerY = 7.5;
  
  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      float dx = x - centerX;
      float dy = y - centerY;
      float distance = sqrt(dx*dx + dy*dy);
      float angle = atan2(dy, dx);
      
      // Spiralmuster: Distanz + Winkel + Zeit
      float spiral = distance - angle * 2.0 + offset;
      
      if (fmod(spiral + 10.0, 3.0) < 1.5) {
        setPixel(frame, x, y, true);
      }
    }
  }
  
  offset += 0.1;
  if (offset > 20.0) {
    offset = 0.0;
  }
}

inline Effect spiralEffect = {SpiralEffect::init, SpiralEffect::draw, "spiral"};

#endif // EFFECT_SPIRAL_H