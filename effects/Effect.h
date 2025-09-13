#ifndef EFFECT_H
#define EFFECT_H
#include <Arduino.h>

// Simple interface for visual effects
typedef void (*EffectInit)();
typedef void (*EffectDraw)(uint8_t *frame);

struct Effect {
  EffectInit init;       // initialize effect state
  EffectDraw draw;       // render effect into a frame buffer
  const char *name;      // name used in web interface
};

#endif // EFFECT_H
