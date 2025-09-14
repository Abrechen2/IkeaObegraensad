#ifndef MATRIX_H
#define MATRIX_H

#include <Arduino.h>
#include <SPI.h>

// Pin mappings for D1 mini driving the LED matrix
const uint8_t PIN_ENABLE = D0;   // GPIO16, OE (active low)
const uint8_t PIN_LATCH  = D3;   // GPIO0, LE
const uint8_t PIN_CLOCK  = D5;   // GPIO14, SCK
const uint8_t PIN_DATA   = D7;   // GPIO13, MOSI

// Each entry maps an (x,y) coordinate to its physical LED index on the
// shift register chain.  This makes the wiring layout fully explicit so a
// wrong pixel can be fixed by adjusting a single value in this table.
// The default table matches the common "serpentine" wiring of 16x16 panels
// where every even row is reversed.  Modify as needed for different panels.

// gespiegelt mit Mittel‑Trennung (links: 128–255, rechts: 0–127, jeweils serpentin)
const uint8_t PIXEL_MAP[16][16] = {
    {23, 22, 21, 20, 19, 18, 17, 16, 7, 6, 5, 4, 3, 2, 1, 0},
    {24, 25, 26, 27, 28, 29, 30, 31, 8, 9, 10, 11, 12, 13, 14, 15},
    {39, 38, 37, 36, 35, 34, 33, 32, 55, 54, 53, 52, 51, 50, 49, 48},
    {40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63},

    {87, 86, 85, 84, 83, 82, 81, 80, 71, 70, 69, 68, 67, 66, 65, 64},
    {88, 89, 90, 91, 92, 93, 94, 95, 72, 73, 74, 75, 76, 77, 78, 79},
    {103, 102, 101, 100, 99, 98, 97, 96, 119, 118, 117, 116, 115, 114, 113, 112},
    {104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127},

    {151, 150, 149, 148, 147, 146, 145, 144, 135, 134, 133, 132, 131, 130, 129, 128},
    {152, 153, 154, 155, 156, 157, 158, 159, 136, 137, 138, 139, 140, 141, 142, 143},
    {167, 166, 165, 164, 163, 162, 161, 160, 183, 182, 181, 180, 179, 178, 177, 176},
    {168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191},

    {215, 214, 213, 212, 211, 210, 209, 208, 199, 198, 197, 196, 195, 194, 193, 192},
    {216, 217, 218, 219, 220, 221, 222, 223, 200, 201, 202, 203, 204, 205, 206, 207},
    {231, 230, 229, 228, 227, 226, 225, 224, 247, 246, 245, 244, 243, 242, 241, 240},
    {232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255},
};


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
  if (x >= 16 || y >= 16) {
    return; // outside of matrix bounds
  }
  uint8_t index = PIXEL_MAP[y][x];
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
