#ifndef EFFECT_PULSE_H
#define EFFECT_PULSE_H

#include "Effect.h"
#include "Matrix.h"
#include <math.h>

namespace PulseEffect {
  extern float phase;
  void init();
  void draw(uint8_t *frame);
}

inline float PulseEffect::phase;

inline void PulseEffect::init() {
  phase = 0.0;
  Serial.printf("Pulse effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void PulseEffect::draw(uint8_t *frame) {
  float intensity = (sin(phase) + 1.0) * 0.5; // 0.0 bis 1.0
  
  if (intensity > 0.3) { // Nur ab bestimmter Helligkeit anzeigen
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        setPixel(frame, x, y, true);
      }
    }
  }
  
  phase += 0.08; // Geschwindigkeit des Pulsierens
  if (phase > 2 * PI) {
    phase -= 2 * PI;
  }
}

inline Effect pulseEffect = {PulseEffect::init, PulseEffect::draw, "pulse"};

#endif // EFFECT_PULSE_H