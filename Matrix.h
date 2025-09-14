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
// neue Tabelle mit Mittel‑Trennung (links: 0–127, rechts: 128–255, jeweils serpentin)
const uint8_t PIXEL_MAP[16][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23},
    {15, 14, 13, 12, 11, 10, 9, 8, 31, 30, 29, 28, 27, 26, 25, 24},
    {48, 49, 50, 51, 52, 53, 54, 55, 32, 33, 34, 35, 36, 37, 38, 39},
    {63, 62, 61, 60, 59, 58, 57, 56, 47, 46, 45, 44, 43, 42, 41, 40},

    {64, 65, 66, 67, 68, 69, 70, 71, 80, 81, 82, 83, 84, 85, 86, 87},
    {79, 78, 77, 76, 75, 74, 73, 72, 95, 94, 93, 92, 91, 90, 89, 88},
    {112, 113, 114, 115, 116, 117, 118, 119, 96, 97, 98, 99, 100, 101, 102, 103},
    {127, 126, 125, 124, 123, 122, 121, 120, 111, 110, 109, 108, 107, 106, 105, 104},

    {128, 129, 130, 131, 132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151},
    {143, 142, 141, 140, 139, 138, 137, 136, 159, 158, 157, 156, 155, 154, 153, 152},
    {176, 177, 178, 179, 180, 181, 182, 183, 160, 161, 162, 163, 164, 165, 166, 167},
    {191, 190, 189, 188, 187, 186, 185, 184, 175, 174, 173, 172, 171, 170, 169, 168},

    {192, 193, 194, 195, 196, 197, 198, 199, 208, 209, 210, 211, 212, 213, 214, 215},
    {207, 206, 205, 204, 203, 202, 201, 200, 223, 222, 221, 220, 219, 218, 217, 216},
    {240, 241, 242, 243, 244, 245, 246, 247, 224, 225, 226, 227, 228, 229, 230, 231},
    {255, 254, 253, 252, 251, 250, 249, 248, 239, 238, 237, 236, 235, 234, 233, 232},
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
