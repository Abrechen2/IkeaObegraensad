#ifndef EFFECT_RIPPLE_H
#define EFFECT_RIPPLE_H

#include "Effect.h"
#include "Matrix.h"
#include <math.h>

namespace RippleEffect {
  extern float time_counter;
  extern uint8_t ripple_centers[3][2]; // Bis zu 3 Ripple-Zentren
  extern float ripple_phases[3];
  void init();
  void draw(uint8_t *frame);
}

inline float RippleEffect::time_counter;
inline uint8_t RippleEffect::ripple_centers[3][2];
inline float RippleEffect::ripple_phases[3];

inline void RippleEffect::init() {
  time_counter = 0.0;
  
  // Verschiedene Ripple-Zentren
  ripple_centers[0][0] = 8;  ripple_centers[0][1] = 8;   // Mitte
  ripple_centers[1][0] = 4;  ripple_centers[1][1] = 4;   // Links oben
  ripple_centers[2][0] = 12; ripple_centers[2][1] = 12;  // Rechts unten
  
  // Unterschiedliche Phasen für interessantere Überlagerung
  ripple_phases[0] = 0.0;
  ripple_phases[1] = PI * 0.6;
  ripple_phases[2] = PI * 1.3;
  
  Serial.printf("Ripple effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void RippleEffect::draw(uint8_t *frame) {
  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      float total_ripple = 0.0;
      
      // Berechne Ripples von allen Zentren
      for (uint8_t i = 0; i < 3; i++) {
        float dx = x - ripple_centers[i][0];
        float dy = y - ripple_centers[i][1];
        float distance = sqrt(dx*dx + dy*dy);
        
        // Ripple-Welle mit Abschwächung über Distanz
        float ripple = sin(distance * 0.8 - time_counter * 3.0 + ripple_phases[i]) * 
                      (1.0 / (1.0 + distance * 0.1));
        
        total_ripple += ripple;
      }
      
      if (total_ripple > 0.4) {
        setPixel(frame, x, y, true);
      }
    }
  }
  
  time_counter += 0.12;
  if (time_counter > 2 * PI * 10) {
    time_counter = 0.0;
  }
}

inline Effect rippleEffect = {RippleEffect::init, RippleEffect::draw, "ripple"};

#endif // EFFECT_RIPPLE_H