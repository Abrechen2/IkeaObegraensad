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
static const uint8_t PIXEL_MAP[16][16] = {
  { 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0 },
  { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 },
  { 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32 },
  { 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 },
  { 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64 },
  { 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95 },
  {111,110,109,108,107,106,105,104,103,102,101,100, 99, 98, 97, 96 },
  {112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127 },
  {143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128 },
  {144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159 },
  {175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160 },
  {176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191 },
  {207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192 },
  {208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223 },
  {239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224 },
  {240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255 }
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
