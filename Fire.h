#ifndef EFFECT_FIRE_H
#define EFFECT_FIRE_H

#include "Effect.h"
#include "Matrix.h"

namespace FireEffect {
  extern uint8_t heat[16][16];
  void init();
  void draw(uint8_t *frame);
}

inline uint8_t FireEffect::heat[16][16];

inline void FireEffect::init() {
  memset(heat, 0, sizeof(heat));
  randomSeed(micros());
  Serial.printf("Fire effect initialized. Free heap: %d\n", ESP.getFreeHeap());
}

inline void FireEffect::draw(uint8_t *frame) {
  // Abkühlung von oben nach unten
  for (uint8_t y = 0; y < 15; y++) {
    for (uint8_t x = 0; x < 16; x++) {
      uint8_t cooldown = random(0, 25);
      if (heat[x][y] > cooldown) {
        heat[x][y] -= cooldown;
      } else {
        heat[x][y] = 0;
      }
    }
  }
  
  // Hitze nach oben propagieren
  for (uint8_t y = 1; y < 16; y++) {
    for (uint8_t x = 0; x < 16; x++) {
      heat[x][y-1] = (heat[x][y] + heat[x][y] + 
                      (x > 0 ? heat[x-1][y] : 0) + 
                      (x < 15 ? heat[x+1][y] : 0)) / 4;
    }
  }
  
  // Neue Hitze am Boden erzeugen
  for (uint8_t x = 0; x < 16; x++) {
    if (random(0, 100) < 60) {
      heat[x][15] = random(160, 255);
    }
  }
  
  // Hitze in Pixel umwandeln
  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      if (heat[x][y] > 100) {
        setPixel(frame, x, y, true);
      }
    }
  }
}

inline Effect fireEffect = {FireEffect::init, FireEffect::draw, "fire"};

#endif // EFFECT_FIRE_H