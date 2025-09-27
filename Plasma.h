#ifndef EFFECT_PLASMA_H
#define EFFECT_PLASMA_H

#include "Effect.h"
#include "Matrix.h"
#include <math.h>

namespace PlasmaEffect {
  extern float time_counter;
  void init();
  void draw(uint8_t *frame);
}

inline float PlasmaEffect::time_counter;

inline void PlasmaEffect::init() {
  time_counter = 0.0;
  Serial.printf("Plasma effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void PlasmaEffect::draw(uint8_t *frame) {
  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      // Mehrere überlagerte Sinuswellen für Plasma-Effekt
      float val = sin(x * 0.3 + time_counter) + 
                  sin(y * 0.3 + time_counter * 0.8) + 
                  sin((x + y) * 0.2 + time_counter * 1.2) + 
                  sin(sqrt((x-8)*(x-8) + (y-8)*(y-8)) * 0.4 + time_counter * 0.6);
      
      // Normalisierung und Schwellwert
      val = (val + 4.0) / 8.0; // Von [-4,4] auf [0,1]
      
      if (val > 0.5) {
        setPixel(frame, x, y, true);
      }
    }
  }
  
  time_counter += 0.08;
  if (time_counter > 2 * PI * 10) {
    time_counter = 0.0;
  }
}

inline Effect plasmaEffect = {PlasmaEffect::init, PlasmaEffect::draw, "plasma"};

#endif // EFFECT_PLASMA_H