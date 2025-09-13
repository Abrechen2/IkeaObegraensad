#ifndef MATRIX_H
#define MATRIX_H

#include <Arduino.h>
#include <SPI.h>

// Pin mappings for D1 mini driving the LED matrix
const uint8_t PIN_ENABLE = D0;   // GPIO16, OE (active low)
const uint8_t PIN_LATCH  = D3;   // GPIO0, LE
const uint8_t PIN_CLOCK  = D5;   // GPIO14, SCK
const uint8_t PIN_DATA   = D7;   // GPIO13, MOSI

inline void matrixSetup() {
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_LATCH,  OUTPUT);
  SPI.begin();
  digitalWrite(PIN_ENABLE, LOW); // enable LEDs
}

inline void clearFrame(uint8_t *buffer, size_t size) {
  memset(buffer, 0x00, size); // all bits low -> LEDs off
}

inline void setPixel(uint8_t *buffer, uint8_t x, uint8_t y, bool on) {
  if ((y & 1) == 0) {
    x = 15 - x;               // even rows are wired right-to-left
  }
  uint16_t index = y * 16 + x;
  uint8_t mask = 0x80 >> (index & 7);
  if (on) {
    buffer[index >> 3] |= mask;  // bit 1 -> LED on
  } else {
    buffer[index >> 3] &= ~mask; // bit 0 -> LED off
  }
}

inline void shiftOutBuffer(uint8_t *buffer, size_t size) {
  digitalWrite(PIN_ENABLE, HIGH);
  digitalWrite(PIN_LATCH, LOW);
  SPI.writeBytes(buffer, size);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_ENABLE, LOW);
}

#endif // MATRIX_H
