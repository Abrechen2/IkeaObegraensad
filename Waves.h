#ifndef EFFECT_WAVES_H
#define EFFECT_WAVES_H

#include "Effect.h"
#include "Matrix.h"
#include <math.h>

namespace WavesEffect {
  extern float offset;
  void init();
  void draw(uint8_t *frame);
}

inline float WavesEffect::offset;

inline void WavesEffect::init() {
  offset = 0.0;
  Serial.printf("Waves effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void WavesEffect::draw(uint8_t *frame) {
  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      // Horizontale Wellen
      float wave1 = sin((y + offset) * 0.4);
      // Vertikale Wellen
      float wave2 = sin((x + offset * 0.8) * 0.3);
      // Kombinierte Wellen
      float combined = wave1 + wave2 * 0.5;
      
      if (combined > 0.2) {
        setPixel(frame, x, y, true);
      }
    }
  }
  
  offset += 0.15;
  if (offset > 100.0) {
    offset = 0.0;
  }
}

inline Effect wavesEffect = {WavesEffect::init, WavesEffect::draw, "waves"};

#endif // EFFECT_WAVES_H