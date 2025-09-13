#include <Arduino.h>
#include <SPI.h>

// Pin-Belegung für den D1 mini
const uint8_t PIN_ENABLE = D0;   // GPIO16, OE (active low)
const uint8_t PIN_LATCH  = D7;   // GPIO0, LE
const uint8_t PIN_CLOCK  = D5;   // GPIO14, SCK
const uint8_t PIN_DATA   = D3;   // GPIO13, MOSI
// optional: const uint8_t PIN_BUTTON = D4; // GPIO2

void shiftOutBuffer(uint8_t *buffer, size_t size) {
  digitalWrite(PIN_ENABLE, HIGH);
  digitalWrite(PIN_LATCH, LOW);
  SPI.writeBytes(buffer, size);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_ENABLE, LOW);
}

void setup() {
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_LATCH,  OUTPUT);
  SPI.begin();                       // nutzt automatisch D5/D7
  digitalWrite(PIN_ENABLE, LOW);     // LEDs aktivieren
}

void loop() {
  static uint16_t pixel = 0;         // aktuelles Pixel (0–255)
  uint8_t frame[32];                 // 16*16 Bits / 8 = 32 Bytes
  memset(frame, 0xFF, sizeof(frame));

  // einzelnes Pixel setzen (active low)
  frame[pixel >> 3] &= ~(0x80 >> (pixel & 7));

  shiftOutBuffer(frame, sizeof(frame));

  pixel = (pixel + 1) % 256;         // nächstes Pixel
  delay(100);                        // Tempo anpassen
}
