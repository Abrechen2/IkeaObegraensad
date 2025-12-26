#ifndef TESTS_H
#define TESTS_H

#include <Arduino.h>
#include <EEPROM.h>
#include "IkeaObegraensad.ino"

// Test-Funktionen für kritische Funktionen
// Diese Funktionen können manuell über Serial-Kommandos aufgerufen werden

// Test für EEPROM writeStringToEEPROM und readStringFromEEPROM
void testEEPROMStringOperations() {
  Serial.println("\n=== EEPROM String Operations Test ===");
  
  const char* testString = "Test String 123";
  const uint16_t testAddr = 900; // Test-Adresse am Ende des EEPROM
  const uint16_t testMaxLen = 64;
  
  // Test 1: String schreiben und lesen
  writeStringToEEPROM(testAddr, testString, testMaxLen);
  char readBuffer[testMaxLen];
  uint16_t readLen = readStringFromEEPROM(testAddr, readBuffer, sizeof(readBuffer));
  
  if (strcmp(testString, readBuffer) == 0) {
    Serial.println("✓ Test 1 PASSED: String write/read");
  } else {
    Serial.printf("✗ Test 1 FAILED: Expected '%s', got '%s'\n", testString, readBuffer);
  }
  
  // Test 2: Zu langer String wird gekürzt
  const char* longString = "This is a very long string that exceeds the maximum length limit of 64 characters and should be truncated";
  writeStringToEEPROM(testAddr, longString, testMaxLen);
  readLen = readStringFromEEPROM(testAddr, readBuffer, sizeof(readBuffer));
  
  if (readLen < strlen(longString) && strlen(readBuffer) < testMaxLen) {
    Serial.println("✓ Test 2 PASSED: Long string truncated");
  } else {
    Serial.printf("✗ Test 2 FAILED: String not properly truncated (len=%d)\n", readLen);
  }
  
  Serial.println("=== End EEPROM String Test ===\n");
}

// Test für Checksumme-Berechnung
void testEEPROMChecksum() {
  Serial.println("\n=== EEPROM Checksum Test ===");
  
  // Speichere aktuellen Zustand
  uint16_t originalChecksum = calculateEEPROMChecksum();
  
  // Ändere einen Wert
  uint16_t oldBrightness = brightness;
  brightness = 999;
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
  uint16_t newChecksum1 = calculateEEPROMChecksum();
  
  // Ändere zurück
  brightness = oldBrightness;
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
  uint16_t newChecksum2 = calculateEEPROMChecksum();
  
  if (originalChecksum != newChecksum1 && originalChecksum == newChecksum2) {
    Serial.println("✓ Checksum Test PASSED: Checksum changes with data");
  } else {
    Serial.printf("✗ Checksum Test FAILED: original=%d, new1=%d, new2=%d\n", 
                  originalChecksum, newChecksum1, newChecksum2);
  }
  
  Serial.println("=== End Checksum Test ===\n");
}

// Test für Input-Validierung
void testInputValidation() {
  Serial.println("\n=== Input Validation Test ===");
  
  // Test 1: String-Limits
  char testBuffer[INPUT_MQTT_SERVER_MAX];
  
  String testInput1 = "valid.server.com";
  copyServerArgToBuffer(testInput1, testBuffer, sizeof(testBuffer));
  if (strcmp(testBuffer, testInput1.c_str()) == 0) {
    Serial.println("✓ Test 1 PASSED: Valid input copied correctly");
  } else {
    Serial.println("✗ Test 1 FAILED: Valid input not copied correctly");
  }
  
  // Test 2: Zu langer Input wird gekürzt
  String longInput = "";
  for (int i = 0; i < INPUT_MQTT_SERVER_MAX + 50; i++) {
    longInput += "x";
  }
  copyServerArgToBuffer(longInput, testBuffer, sizeof(testBuffer));
  if (strlen(testBuffer) < INPUT_MQTT_SERVER_MAX) {
    Serial.println("✓ Test 2 PASSED: Long input truncated");
  } else {
    Serial.printf("✗ Test 2 FAILED: Input not truncated (len=%zu)\n", strlen(testBuffer));
  }
  
  Serial.println("=== End Input Validation Test ===\n");
}

// Alle Tests ausführen
void runAllTests() {
  Serial.println("\n\n========== RUNNING ALL TESTS ==========");
  testEEPROMStringOperations();
  testEEPROMChecksum();
  testInputValidation();
  Serial.println("========== ALL TESTS COMPLETE ==========\n\n");
}

#endif // TESTS_H

