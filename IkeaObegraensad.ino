#include <Arduino.h>
#include <SPI.h>

// Pin-Belegung für den D1 mini
const uint8_t PIN_ENABLE = D0;   // GPIO16, OE (active low)
const uint8_t PIN_LATCH  = D7;   // GPIO0, LE
const uint8_t PIN_CLOCK  = D5;   // GPIO14, SCK
const uint8_t PIN_DATA   = D3;   // GPIO13, MOSI
// optional: const uint8_t PIN_BUTTON = D4; // GPIO2

// Pixel in der Mitte des 16x16 Feldes
const uint16_t MID_PIXEL = (8 * 16) + 8;

void shiftOutBuffer(uint8_t *buffer, size_t size) {
  Serial.println("Sende Frame");
  digitalWrite(PIN_ENABLE, HIGH);
  digitalWrite(PIN_LATCH, LOW);
  SPI.writeBytes(buffer, size);
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_ENABLE, LOW);
  Serial.println("Frame gesendet");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup gestartet");
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_LATCH,  OUTPUT);
  SPI.begin();                       // nutzt automatisch D5/D7
  digitalWrite(PIN_ENABLE, LOW);     // LEDs aktivieren
  Serial.println("Setup fertig");
}

void loop() {
  uint8_t frame[32];                 // 16*16 Bits / 8 = 32 Bytes
  memset(frame, 0x00, sizeof(frame));

  // einzelnes Pixel setzen (active high)
  frame[MID_PIXEL >> 3] |= (0x80 >> (MID_PIXEL & 7));

  Serial.print("Frame-Inhalt: ");
  for (uint8_t i = 0; i < sizeof(frame); ++i) {
    Serial.print(frame[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  shiftOutBuffer(frame, sizeof(frame));

  delay(1000);
}
