#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <FS.h>
#include <ArduinoOTA.h>
extern "C" {
#include <user_interface.h>
}

// Debug-Logging aktivieren/deaktivieren
// Für Production-Builds: Kommentiere die nächste Zeile aus
// #define DEBUG_LOGGING_ENABLED

// Firmware-Version
#define FIRMWARE_VERSION "1.3"

#include "Matrix.h"
#include "Effect.h"
#include "Snake.h"
#include "Clock.h"
#include "Rain.h"
#include "Bounce.h"
#include "Stars.h"
#include "Lines.h"
#include "secrets.h"
#include "WebInterface.h"
#include "Pulse.h"
#include "Waves.h"
#include "Spiral.h"
#include "Fire.h"
#include "Plasma.h"
#include "Ripple.h"
#include "SandClock.h"

ESP8266WebServer server(80);
bool serverStarted = false;
bool ntpConfigured = false;

// MQTT Configuration for Aqara Presence Sensor
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqttServer = "";  // MQTT Broker IP (wird über Web-UI konfiguriert, kein Default)
uint16_t mqttPort = 1883; // Standard MQTT Port
String mqttUser = "";    // Optional
String mqttPassword = ""; // Optional
String mqttPresenceTopic = "Sonstige/Präsenz_Wz/Anwesenheit"; // Topic für Präsenzmelder (FP2 sendet JSON an Haupt-Topic)
bool mqttEnabled = false;
bool presenceDetected = false;
unsigned long lastPresenceTime = 0;
uint32_t presenceTimeout = 300000; // 5 Minuten in ms (Display bleibt 5 Min an nach letzter Erkennung)
bool displayEnabled = true; // Display-Status (wird durch Präsenz gesteuert)
bool haDisplayControlled = false; // Flag: Wird Display von HA gesteuert? (deaktiviert MQTT-Präsenz)

// Rate-Limiting für Web-API
struct RateLimit {
  unsigned long lastRequest;
  uint8_t requestCount;
};
RateLimit apiRateLimit = {0, 0};
const unsigned long RATE_LIMIT_WINDOW = 10000; // 10 Sekunden Zeitfenster
const uint8_t RATE_LIMIT_MAX_REQUESTS = 20; // Max. 20 Requests pro Zeitfenster

// MQTT Reconnect mit Exponential Backoff
unsigned long mqttReconnectBackoff = 1000; // Start mit 1 Sekunde
const unsigned long MQTT_MAX_BACKOFF = 60000; // Maximal 60 Sekunden
const unsigned long MQTT_BACKOFF_MULTIPLIER = 2; // Verdoppeln bei jedem Fehlschlag

// Log-Server Konfiguration (kann in secrets.h überschrieben werden)
#ifndef LOG_SERVER_URL
#define LOG_SERVER_URL "http://192.168.178.36:402/logs"  // Leer = deaktiviert, z.B. "http://192.168.1.100:3000/logs"
#endif
#ifndef LOG_SERVER_INTERVAL
#define LOG_SERVER_INTERVAL 60000  // Intervall in ms für automatisches Senden (Standard: 60 Sekunden)
#endif

String logServerUrl = LOG_SERVER_URL;
unsigned long lastLogUpload = 0;
const unsigned long LOG_UPLOAD_INTERVAL = LOG_SERVER_INTERVAL;
const unsigned long MQTT_CONNECT_TIMEOUT = 5000; // 5 Sekunden Timeout für Connect-Versuch

const uint16_t DEFAULT_BRIGHTNESS = 512;
uint16_t brightness = DEFAULT_BRIGHTNESS; // 0..1023

// Auto-Brightness Konfiguration
bool autoBrightnessEnabled = false;
uint16_t minBrightness = 100;   // Minimale Helligkeit (0-1023)
uint16_t maxBrightness = 1023;  // Maximale Helligkeit (0-1023)
uint16_t sensorMin = 5;         // Minimaler Sensorwert (dunkel) - LDR-spezifisch
uint16_t sensorMax = 450;       // Maximaler Sensorwert (hell) - LDR-spezifisch
const uint8_t LIGHT_SENSOR_PIN = A0; // Analoger Pin für Phototransistor


// Auto-Brightness Konstanten (optimiert gegen Watchdog-Resets)
const uint8_t LIGHT_SENSOR_SAMPLES = 5;     // Reduziert von 10 auf 5 für schnellere Messung
const uint8_t LIGHT_SENSOR_SAMPLE_DELAY = 10; // Reduziert von 20ms auf 10ms
const uint16_t BRIGHTNESS_CHANGE_THRESHOLD = 30; // Erhöht für sanftere Übergänge (nur für große Änderungen)
const unsigned long AUTO_BRIGHTNESS_UPDATE_INTERVAL = 3000; // 3s Update-Intervall

// Exponential Moving Average für sanftere Helligkeitsanpassung (besser als Simple Moving Average)
const float EMA_ALPHA_SENSOR = 0.08;  // Reduziert von 0.15 für langsamere Reaktion auf Sensor-Noise
const float EMA_ALPHA_BRIGHTNESS = 0.12;  // Zusätzliche Glättung für Helligkeitsänderungen
float emaSensorValue = 0.0;    // Aktueller EMA-Wert für Sensor
float emaBrightnessValue = 0.0; // EMA-Wert für Helligkeit (zusätzliche Glättung)
bool emaInitialized = false;   // Ist EMA initialisiert?
bool emaBrightnessInitialized = false; // Ist Brightness-EMA initialisiert?

// Non-blocking Sensor-Sampling
uint8_t sensorSampleCount = 0;
uint32_t sensorSampleSum = 0;
unsigned long lastSensorSample = 0;
bool sensorSamplingInProgress = false;

const uint8_t EEPROM_VERSION = 1;   // EEPROM-Layout Version
const uint8_t EEPROM_MAGIC = 0xB8;  // Magic Byte zur Erkennung initialisierter EEPROM
const uint16_t EEPROM_SIZE = 512;   // Erweitert für MQTT- und NTP-Konfiguration

// EEPROM-Adressen (mit Versionierung und Checksumme)
const uint16_t EEPROM_VERSION_ADDR = 0;      // uint8_t (1 byte)
const uint16_t EEPROM_MAGIC_ADDR = 1;        // uint8_t (1 byte)
const uint16_t EEPROM_CHECKSUM_ADDR = 2;     // uint16_t (2 bytes)
const uint16_t EEPROM_BRIGHTNESS_ADDR = 4;   // uint16_t (2 bytes)
const uint16_t EEPROM_AUTO_BRIGHTNESS_ADDR = 6;    // bool (1 byte)
const uint16_t EEPROM_MIN_BRIGHTNESS_ADDR = 7;     // uint16_t (2 bytes)
const uint16_t EEPROM_MAX_BRIGHTNESS_ADDR = 9;     // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MIN_ADDR = 11;         // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MAX_ADDR = 13;        // uint16_t (2 bytes)
const uint16_t EEPROM_MQTT_ENABLED_ADDR = 15;      // bool (1 byte)
const uint16_t EEPROM_MQTT_SERVER_ADDR = 16;       // String (64 bytes)
const uint16_t EEPROM_MQTT_PORT_ADDR = 80;         // uint16_t (2 bytes)
const uint16_t EEPROM_MQTT_USER_ADDR = 82;         // String (32 bytes)
const uint16_t EEPROM_MQTT_PASSWORD_ADDR = 114;    // String (32 bytes)
const uint16_t EEPROM_MQTT_TOPIC_ADDR = 146;       // String (64 bytes)
const uint16_t EEPROM_PRESENCE_TIMEOUT_ADDR = 210; // uint32_t (4 bytes)
const uint16_t EEPROM_NTP_SERVER1_ADDR = 214;       // String (64 bytes)
const uint16_t EEPROM_NTP_SERVER2_ADDR = 278;       // String (64 bytes)
const uint16_t EEPROM_HOUR_FORMAT_ADDR = 342;       // bool (1 byte) 24h=1, 12h=0

const uint8_t BUTTON_PIN = D4;

Effect *effects[] = {
  &snakeEffect,
  &clockEffect,
  &rainEffect,
  &bounceEffect,
  &starsEffect,
  &linesEffect,
  &pulseEffect,
  &wavesEffect,
  &spiralEffect,
  &fireEffect,
  &plasmaEffect,
  &rippleEffect,
  &sandClockEffect
};
const uint8_t effectCount = sizeof(effects) / sizeof(effects[0]);
uint8_t currentEffectIndex = 1; // start with clock
Effect *currentEffect = effects[currentEffectIndex];
// POSIX TZ String mit automatischer Sommer-/Winterzeit-Umstellung (DST)
// Format: STD<offset>DST<offset>,start[/time],end[/time]
// Beispiel: CET-1CEST-2,M3.5.0/2,M10.5.0/3
//   - CET-1 = Central European Time (Standardzeit), UTC+1 (Offset -1)
//   - CEST-2 = Central European Summer Time (Sommerzeit), UTC+2 (Offset -2)
//   - M3.5.0/2 = März (M3), 5. Woche, Sonntag (0) = letzter Sonntag im März um 02:00 Uhr lokaler Zeit
//   - M10.5.0/3 = Oktober (M10), 5. Woche, Sonntag, um 03:00 Uhr lokaler Zeit = letzter Sonntag im Oktober
String tzString = "CET-1CEST-2,M3.5.0/2,M10.5.0/3"; // default Europe/Berlin mit DST
String ntpServer1 = "0.de.pool.ntp.org"; // Primärer NTP-Server (konfigurierbar)
String ntpServer2 = "1.de.pool.ntp.org"; // Sekundärer NTP-Server (konfigurierbar)
bool use24HourFormat = true; // 24h-Format standardmäßig aktiv

// Helper-Funktion für sichere Zeitdifferenz-Berechnung (handles millis() overflow)
inline unsigned long timeDiff(unsigned long now, unsigned long then) {
  // Bei Overflow: now < then, dann ist die Differenz (ULONG_MAX - then) + now + 1
  if (now >= then) {
    return now - then;
  } else {
    // Overflow aufgetreten
    return (ULONG_MAX - then) + now + 1;
  }
}

int formatHourForDisplay(int hour) {
  if (use24HourFormat) {
    return hour;
  }

  int hour12 = hour % 12;
  return hour12 == 0 ? 12 : hour12;
}

// Helper-Funktionen für String-Speicherung in EEPROM
void writeStringToEEPROM(uint16_t addr, const String &str, uint16_t maxLen) {
  uint16_t len = min((uint16_t)str.length(), (uint16_t)(maxLen - 1));
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, 0); // Null-Terminator
}

String readStringFromEEPROM(uint16_t addr, uint16_t maxLen) {
  String str = "";
  str.reserve(maxLen); // Speicher vorreservieren für bessere Performance
  for (uint16_t i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0) break;
    str += c;
  }
  return str;
}

// Berechnet Checksumme über alle EEPROM-Daten (ab EEPROM_BRIGHTNESS_ADDR bis EEPROM_SIZE)
uint16_t calculateEEPROMChecksum() {
  uint16_t checksum = 0;
  for (uint16_t i = EEPROM_BRIGHTNESS_ADDR; i < EEPROM_SIZE; i++) {
    checksum += EEPROM.read(i);
    checksum = (checksum << 1) | (checksum >> 15); // Rotate für bessere Verteilung
  }
  return checksum;
}

// Validiert EEPROM-Daten: Version, Magic Byte und Checksumme
bool validateEEPROM() {
  uint8_t version = EEPROM.read(EEPROM_VERSION_ADDR);
  uint8_t magic = EEPROM.read(EEPROM_MAGIC_ADDR);
  uint16_t storedChecksum;
  EEPROM.get(EEPROM_CHECKSUM_ADDR, storedChecksum);
  
  // Prüfe Version und Magic Byte
  if (version != EEPROM_VERSION || magic != EEPROM_MAGIC) {
    return false;
  }
  
  // Prüfe Checksumme
  uint16_t calculatedChecksum = calculateEEPROMChecksum();
  return (storedChecksum == calculatedChecksum);
}

void loadBrightnessFromStorage() {
  // Prüfe ob EEPROM initialisiert und gültig ist
  if (validateEEPROM()) {
    // EEPROM ist gültig, lade Daten
    uint16_t stored = DEFAULT_BRIGHTNESS;
    EEPROM.get(EEPROM_BRIGHTNESS_ADDR, stored);
    brightness = constrain(stored, 0, 1023);

    // Auto-Brightness-Einstellungen laden
    autoBrightnessEnabled = EEPROM.read(EEPROM_AUTO_BRIGHTNESS_ADDR) == 1;
    EEPROM.get(EEPROM_MIN_BRIGHTNESS_ADDR, minBrightness);
    EEPROM.get(EEPROM_MAX_BRIGHTNESS_ADDR, maxBrightness);
    EEPROM.get(EEPROM_SENSOR_MIN_ADDR, sensorMin);
    EEPROM.get(EEPROM_SENSOR_MAX_ADDR, sensorMax);

    // MQTT-Einstellungen laden
    mqttEnabled = EEPROM.read(EEPROM_MQTT_ENABLED_ADDR) == 1;
    mqttServer = readStringFromEEPROM(EEPROM_MQTT_SERVER_ADDR, 64);
    EEPROM.get(EEPROM_MQTT_PORT_ADDR, mqttPort);
    mqttUser = readStringFromEEPROM(EEPROM_MQTT_USER_ADDR, 32);
    mqttPassword = readStringFromEEPROM(EEPROM_MQTT_PASSWORD_ADDR, 32);
    mqttPresenceTopic = readStringFromEEPROM(EEPROM_MQTT_TOPIC_ADDR, 64);
    EEPROM.get(EEPROM_PRESENCE_TIMEOUT_ADDR, presenceTimeout);
    
    // NTP-Server laden
    String loadedNtp1 = readStringFromEEPROM(EEPROM_NTP_SERVER1_ADDR, 64);
    String loadedNtp2 = readStringFromEEPROM(EEPROM_NTP_SERVER2_ADDR, 64);
    // Validierung: Nur verwenden wenn String nicht leer ist und gültig aussieht (enthält Punkt für Domain)
    if (loadedNtp1.length() > 0 && loadedNtp1.length() < 64 && loadedNtp1.indexOf('.') > 0) {
      ntpServer1 = loadedNtp1;
    }
    if (loadedNtp2.length() > 0 && loadedNtp2.length() < 64 && loadedNtp2.indexOf('.') > 0) {
      ntpServer2 = loadedNtp2;
    }
    use24HourFormat = EEPROM.read(EEPROM_HOUR_FORMAT_ADDR) != 0;

    // Werte validieren und korrigieren falls nötig
    minBrightness = constrain(minBrightness, 0, 1023);
    maxBrightness = constrain(maxBrightness, 0, 1023);
    sensorMin = constrain(sensorMin, 0, 1023);
    sensorMax = constrain(sensorMax, 0, 1023);
    if (mqttPort == 0 || mqttPort > 65535) mqttPort = 1883;
    if (presenceTimeout == 0) presenceTimeout = 300000;
    
    Serial.println("EEPROM data loaded and validated successfully");
  } else {
    // EEPROM ungültig oder nicht initialisiert, verwende Defaults
    Serial.println("EEPROM invalid or not initialized, using defaults");
    brightness = DEFAULT_BRIGHTNESS;
    autoBrightnessEnabled = false;
    minBrightness = 100;
    maxBrightness = 1023;
    sensorMin = 5;
    sensorMax = 450;
    mqttEnabled = false;
    mqttServer = "";
    mqttPort = 1883;
    mqttUser = "";
    mqttPassword = "";
    mqttPresenceTopic = "";
    presenceTimeout = 300000;
    ntpServer1 = "pool.ntp.org";
    ntpServer2 = "time.nist.gov";
    use24HourFormat = true;
  }
}

void persistBrightnessToStorage() {
  // Version und Magic Byte setzen
  EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION);
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  
  // Daten schreiben
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
  EEPROM.write(EEPROM_AUTO_BRIGHTNESS_ADDR, autoBrightnessEnabled ? 1 : 0);
  EEPROM.put(EEPROM_MIN_BRIGHTNESS_ADDR, minBrightness);
  EEPROM.put(EEPROM_MAX_BRIGHTNESS_ADDR, maxBrightness);
  EEPROM.put(EEPROM_SENSOR_MIN_ADDR, sensorMin);
  EEPROM.put(EEPROM_SENSOR_MAX_ADDR, sensorMax);
  EEPROM.write(EEPROM_HOUR_FORMAT_ADDR, use24HourFormat ? 1 : 0);
  
  // Checksumme berechnen und speichern
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  EEPROM.commit();
}

void persistMqttToStorage() {
  // Version und Magic Byte setzen (falls noch nicht gesetzt)
  EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION);
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  
  // MQTT-Daten schreiben
  EEPROM.write(EEPROM_MQTT_ENABLED_ADDR, mqttEnabled ? 1 : 0);
  writeStringToEEPROM(EEPROM_MQTT_SERVER_ADDR, mqttServer, 64);
  EEPROM.put(EEPROM_MQTT_PORT_ADDR, mqttPort);
  writeStringToEEPROM(EEPROM_MQTT_USER_ADDR, mqttUser, 32);
  writeStringToEEPROM(EEPROM_MQTT_PASSWORD_ADDR, mqttPassword, 32);
  writeStringToEEPROM(EEPROM_MQTT_TOPIC_ADDR, mqttPresenceTopic, 64);
  EEPROM.put(EEPROM_PRESENCE_TIMEOUT_ADDR, presenceTimeout);
  
  // Checksumme berechnen und speichern
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  // #region agent log
  unsigned long commitStart = millis();
  int freeHeapBefore = ESP.getFreeHeap();
  // #endregion
  EEPROM.commit();
  // #region agent log
  unsigned long commitDuration = millis() - commitStart;
  int freeHeapAfter = ESP.getFreeHeap();
  if (commitDuration > 100 || freeHeapBefore != freeHeapAfter) {
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"eeprom_mqtt_%lu\",\"timestamp\":%lu,\"location\":\"persistMqttToStorage\",\"message\":\"EEPROM commit\",\"data\":{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), commitDuration, freeHeapBefore, freeHeapAfter);
        logFile.close();
      }
    }
  }
  // #endregion
}

// VERBESSERT: Non-blocking Sensor-Sampling (verhindert Watchdog-Resets)
// Startet einen neuen Sample-Zyklus
void startLightSensorSampling() {
  sensorSampleCount = 0;
  sensorSampleSum = 0;
  sensorSamplingInProgress = true;
  lastSensorSample = millis();
}

// Nimmt ein einzelnes Sample, gibt true zurück wenn Sampling abgeschlossen
bool processLightSensorSample() {
  if (!sensorSamplingInProgress) {
    return false;
  }

  unsigned long now = millis();
  if (now - lastSensorSample >= LIGHT_SENSOR_SAMPLE_DELAY) {
    uint16_t sensorReading = analogRead(LIGHT_SENSOR_PIN);
    sensorSampleSum += sensorReading;
    sensorSampleCount++;
    lastSensorSample = now;

#ifdef DEBUG_LOGGING_ENABLED
    if (sensorSampleCount == 1 || sensorSampleCount == LIGHT_SENSOR_SAMPLES) { // Erste und letzte Messung loggen
      debugLogJson("processLightSensorSample", "Sensor sample", "D", "{\"sampleCount\":%d,\"reading\":%d,\"sum\":%lu}", sensorSampleCount, sensorReading, sensorSampleSum);
    }
#endif

    if (sensorSampleCount >= LIGHT_SENSOR_SAMPLES) {
      sensorSamplingInProgress = false;
      return true; // Sampling abgeschlossen
    }
  }
  return false; // Noch nicht fertig
}

// Gibt den gemittelten Sensorwert zurück
uint16_t getLightSensorResult() {
  if (sensorSampleCount == 0) return 0;
  return sensorSampleSum / sensorSampleCount;
}

// VERBESSERT: Exponential Moving Average (sanfter als Simple Moving Average)
float applyEMA(float newValue) {
#ifdef DEBUG_LOGGING_ENABLED
  float oldEma = emaSensorValue;
#endif
  
  if (!emaInitialized) {
    emaSensorValue = newValue;
    emaInitialized = true;
#ifdef DEBUG_LOGGING_ENABLED
    debugLogJson("applyEMA", "EMA initialized", "D", "{\"newValue\":%.2f}", newValue);
#endif
    return newValue;
  }

  // EMA Formel: EMA_new = alpha * value + (1 - alpha) * EMA_old
  emaSensorValue = EMA_ALPHA_SENSOR * newValue + (1.0 - EMA_ALPHA_SENSOR) * emaSensorValue;
  
#ifdef DEBUG_LOGGING_ENABLED
  float emaChange = abs(emaSensorValue - oldEma);
  if (emaChange > 10) { // Nur loggen bei größeren Änderungen
    debugLogJson("applyEMA", "EMA update", "D", "{\"newValue\":%.2f,\"oldEma\":%.2f,\"newEma\":%.2f,\"change\":%.2f,\"alpha\":%.2f}", newValue, oldEma, emaSensorValue, emaChange, EMA_ALPHA_SENSOR);
  }
#endif
  
  return emaSensorValue;
}

// Zusätzliche EMA-Glättung für Helligkeitsänderungen (verhindert abrupte Sprünge)
uint16_t applyBrightnessEMA(uint16_t newBrightness) {
  if (!emaBrightnessInitialized) {
    emaBrightnessValue = (float)newBrightness;
    emaBrightnessInitialized = true;
    return newBrightness;
  }
  
  // EMA auf Helligkeit anwenden
  emaBrightnessValue = EMA_ALPHA_BRIGHTNESS * (float)newBrightness + (1.0 - EMA_ALPHA_BRIGHTNESS) * emaBrightnessValue;
  
  // Auf Integer runden
  return (uint16_t)(emaBrightnessValue + 0.5);
}

void updateAutoBrightness() {
#ifdef DEBUG_LOGGING_ENABLED
    debugLogJson("updateAutoBrightness", "Function entry", "D", "{\"enabled\":%d,\"displayEnabled\":%d}", autoBrightnessEnabled ? 1 : 0, displayEnabled ? 1 : 0);
#endif
  
  if (!autoBrightnessEnabled || !displayEnabled) {
    return;
  }

  // Wenn Sampling noch läuft, nichts tun (wird in loop() verarbeitet)
  if (sensorSamplingInProgress) {
    return;
  }

  // BUGFIX: Erst die fertigen Samples auswerten, DANN neues Sampling starten
  // Prüfe ob Daten zum Auswerten da sind
  if (sensorSampleCount > 0) {
    // Alle Samples gesammelt, jetzt auswerten
    uint16_t rawSensorValue = getLightSensorResult();
#ifdef DEBUG_LOGGING_ENABLED
    debugLogJson("updateAutoBrightness", "Raw sensor value", "D", "{\"rawSensorValue\":%d}", rawSensorValue);
#endif
    
    float smoothedSensorValue = applyEMA(rawSensorValue);
#ifdef DEBUG_LOGGING_ENABLED
    debugLogJson("updateAutoBrightness", "EMA smoothed value", "D", "{\"smoothedSensorValue\":%.2f,\"emaSensorValue\":%.2f}", smoothedSensorValue, emaSensorValue);
#endif

    // Map Sensorwert (sensorMin..sensorMax) auf Helligkeit (minBrightness..maxBrightness)
    uint16_t newBrightness;
    bool atBoundary = false;  // Flag für Grenzbereiche

    if (smoothedSensorValue <= sensorMin) {
      newBrightness = minBrightness;
      atBoundary = true;  // An unterer Grenze
    } else if (smoothedSensorValue >= sensorMax) {
      newBrightness = maxBrightness;
      atBoundary = true;  // An oberer Grenze
    } else {
      // Lineare Interpolation zwischen Min und Max
      newBrightness = map((uint16_t)smoothedSensorValue, sensorMin, sensorMax, minBrightness, maxBrightness);
    }

#ifdef DEBUG_LOGGING_ENABLED
    int brightnessDiff = abs((int)newBrightness - (int)brightness);
    debugLogJson("updateAutoBrightness", "Brightness calculation", "D", "{\"newBrightness\":%d,\"currentBrightness\":%d,\"diff\":%d,\"threshold\":%d,\"atBoundary\":%d}", newBrightness, brightness, brightnessDiff, BRIGHTNESS_CHANGE_THRESHOLD, atBoundary ? 1 : 0);
#endif

    // Zusätzliche Glättung der Helligkeitsänderungen (verhindert abrupte Sprünge)
    uint16_t smoothedBrightness = applyBrightnessEMA(newBrightness);
    
    // An Grenzen immer aktualisieren, sonst nur bei signifikanter Änderung (Hysterese)
    // Hysterese verhindert Flackern im mittleren Bereich, sollte aber nicht Min/Max blockieren
    // Verwende smoothedBrightness für Vergleich, aber aktualisiere mit smoothedBrightness für sanfte Übergänge
    int smoothedDiff = abs((int)smoothedBrightness - (int)brightness);
    
    if (atBoundary || smoothedDiff > BRIGHTNESS_CHANGE_THRESHOLD) {
      brightness = smoothedBrightness; // Verwende geglättete Helligkeit
      analogWrite(PIN_ENABLE, 1023 - brightness);
#ifdef DEBUG_LOGGING_ENABLED
      debugLogJson("updateAutoBrightness", "Brightness updated", "D", "{\"brightness\":%d,\"rawSensorValue\":%d,\"smoothedSensorValue\":%.2f,\"newBrightness\":%d,\"smoothedBrightness\":%d,\"atBoundary\":%d}", brightness, rawSensorValue, smoothedSensorValue, newBrightness, smoothedBrightness, atBoundary ? 1 : 0);
#endif
      Serial.printf("Auto-Brightness: Raw=%d, EMA=%.1f -> New=%d, Smoothed=%d%s\n",
                    rawSensorValue, smoothedSensorValue, newBrightness, brightness,
                    atBoundary ? " (boundary)" : "");
    } else {
#ifdef DEBUG_LOGGING_ENABLED
      debugLogJson("updateAutoBrightness", "Brightness not updated (threshold)", "D", "{\"newBrightness\":%d,\"smoothedBrightness\":%d,\"currentBrightness\":%d,\"diff\":%d}", newBrightness, smoothedBrightness, brightness, smoothedDiff);
#endif
    }
  }

  // Neues Sampling für den nächsten Zyklus starten
  startLightSensorSampling();
}

// MQTT Callback für eingehende Nachrichten
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Payload zu String konvertieren (optimiert mit reserve)
  String message = "";
  message.reserve(length + 1); // Speicher vorreservieren
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("MQTT Message received on topic %s: %s\n", topic, message.c_str());

  // Präsenz-Status auswerten
  if (String(topic) == mqttPresenceTopic) {
    bool newPresence = false;

    // Verschiedene Payload-Formate unterstützen:
    // 1. Einfache Werte: "true", "false", "1", "0", "occupied"
    // 2. JSON (Aqara FP2): {"presence":true} oder {"occupancy":true}

    message.toLowerCase();
    message.trim();

    // Einfache Werte prüfen
    if (message == "true" || message == "1" || message == "occupied" || message == "on") {
      newPresence = true;
    }
    else if (message == "false" || message == "0" || message == "unoccupied" || message == "off") {
      newPresence = false;
    }
    // JSON-Parsing (einfach, ohne Bibliothek)
    else if (message.indexOf("\"presence\"") >= 0 || message.indexOf("\"occupancy\"") >= 0) {
      // Suche nach "presence":true oder "occupancy":true in JSON
      int truePos = message.indexOf(":true");
      int falsePos = message.indexOf(":false");

      if (truePos > 0) {
        // Prüfe ob "true" nach "presence" oder "occupancy" kommt
        int presencePos = message.indexOf("\"presence\"");
        int occupancyPos = message.indexOf("\"occupancy\"");

        if ((presencePos >= 0 && truePos > presencePos) ||
            (occupancyPos >= 0 && truePos > occupancyPos)) {
          newPresence = true;
        }
      }
    }

    if (newPresence != presenceDetected) {
      presenceDetected = newPresence;
      Serial.printf("Presence changed: %s\n", presenceDetected ? "DETECTED" : "NOT DETECTED");
    }

    if (presenceDetected) {
      lastPresenceTime = millis();
      // Nur Display einschalten wenn HA nicht aktiv steuert
      if (!haDisplayControlled && !displayEnabled) {
        displayEnabled = true;
        Serial.println("Display ENABLED by presence detection");
        // Display einschalten
        if (autoBrightnessEnabled) {
          // Auto-Brightness wird die Helligkeit setzen
        } else {
          analogWrite(PIN_ENABLE, 1023 - brightness);
        }
      }
    } else {
      // Präsenz nicht mehr erkannt - nur Timeout setzen, Display wird von updatePresenceTimeout() ausgeschaltet
      // Aber nur wenn HA nicht steuert
      if (!haDisplayControlled) {
        lastPresenceTime = millis(); // Reset Timer
      }
    }
  }
}

// MQTT Verbindung aufbauen/wiederherstellen
bool reconnectMQTT() {
  // #region agent log
  unsigned long reconnectStart = millis();
  int freeHeapBefore = ESP.getFreeHeap();
  int maxFreeBlockBefore = ESP.getMaxFreeBlockSize();
  if (SPIFFS.exists("/")) {
    File logFile = SPIFFS.open("/debug.log", "a");
    if (logFile) {
      logFile.printf("{\"id\":\"mqtt_start_%lu\",\"timestamp\":%lu,\"location\":\"reconnectMQTT\",\"message\":\"MQTT reconnect start\",\"data\":{\"freeHeap\":%d,\"maxFreeBlock\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                     millis(), millis(), freeHeapBefore, maxFreeBlockBefore);
      logFile.close();
    }
  }
  // #endregion
  
  if (!mqttEnabled || mqttServer.length() == 0) {
    return false;
  }

  if (mqttClient.connected()) {
    return true;
  }

  Serial.printf("Attempting MQTT connection to %s:%d...\n", mqttServer.c_str(), mqttPort);

  // Client-ID generieren (optimiert)
  char clientIdBuf[32];
  snprintf(clientIdBuf, sizeof(clientIdBuf), "IkeaClock-%x", ESP.getChipId());
  String clientId = String(clientIdBuf);

  // Socket-Timeout setzen (für non-blocking Connect)
  mqttClient.setSocketTimeout(MQTT_CONNECT_TIMEOUT / 1000); // in Sekunden
  
  bool connected = false;
  unsigned long connectStart = millis();
  // #region agent log
  ESP.wdtFeed(); // Watchdog vor blockierender Operation füttern
  // #endregion
  if (mqttUser.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str());
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  // #region agent log
  ESP.wdtFeed(); // Watchdog nach blockierender Operation füttern
  // #endregion
  
  // Prüfe ob Connect zu lange gedauert hat (Fallback)
  unsigned long connectDuration = millis() - connectStart;
  if (connectDuration > MQTT_CONNECT_TIMEOUT) {
    Serial.printf("MQTT connect timeout after %lu ms\n", connectDuration);
    connected = false;
  }

  // #region agent log
  unsigned long reconnectDuration = millis() - reconnectStart;
  int freeHeapAfter = ESP.getFreeHeap();
  int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
  // #endregion
  
  if (connected) {
    Serial.println("MQTT connected!");
    // Backoff zurücksetzen bei erfolgreicher Verbindung
    mqttReconnectBackoff = 1000;
    // Topic abonnieren
    if (mqttPresenceTopic.length() > 0) {
      mqttClient.subscribe(mqttPresenceTopic.c_str());
      Serial.printf("Subscribed to topic: %s\n", mqttPresenceTopic.c_str());
    }
    // #region agent log
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"mqtt_success_%lu\",\"timestamp\":%lu,\"location\":\"reconnectMQTT\",\"message\":\"MQTT reconnect success\",\"data\":{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), reconnectDuration, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
        logFile.close();
      }
    }
    // #endregion
    return true;
  } else {
    Serial.printf("MQTT connection failed, rc=%d, next retry in %lu ms\n", mqttClient.state(), mqttReconnectBackoff);
    // Exponential Backoff erhöhen
    mqttReconnectBackoff *= MQTT_BACKOFF_MULTIPLIER;
    if (mqttReconnectBackoff > MQTT_MAX_BACKOFF) {
      mqttReconnectBackoff = MQTT_MAX_BACKOFF;
    }
    // #region agent log
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"mqtt_failed_%lu\",\"timestamp\":%lu,\"location\":\"reconnectMQTT\",\"message\":\"MQTT reconnect failed\",\"data\":{\"duration\":%lu,\"rc\":%d,\"nextBackoff\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), reconnectDuration, mqttClient.state(), mqttReconnectBackoff, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
        logFile.close();
      }
    }
    // #endregion
    return false;
  }
}

// Präsenz-Timeout prüfen und Display entsprechend steuern
void updatePresenceTimeout() {
  // Wenn HA das Display steuert, MQTT-Präsenz-Logik komplett deaktivieren
  if (haDisplayControlled) {
    return;
  }

  if (!mqttEnabled) {
    return;
  }

  // Wenn Präsenz erkannt: Display ist bereits an, nichts zu tun
  if (presenceDetected) {
    return;
  }

  // Keine Präsenz: Prüfe ob Timeout abgelaufen und Display noch an
  unsigned long timeSincePresence = timeDiff(millis(), lastPresenceTime);

  if (displayEnabled && timeSincePresence > presenceTimeout) {
    displayEnabled = false;
    Serial.printf("Display DISABLED after %lu ms timeout (no presence)\n", timeSincePresence);

    // Display ausschalten (Helligkeit auf 0)
    analogWrite(PIN_ENABLE, 1023);
  }
}

// Rate-Limiting Prüfung für API-Endpoints
bool checkRateLimit() {
  unsigned long now = millis();
  unsigned long timeSinceLastWindow = timeDiff(now, apiRateLimit.lastRequest);
  
  if (timeSinceLastWindow > RATE_LIMIT_WINDOW) {
    // Neues Zeitfenster, Counter zurücksetzen
    apiRateLimit.requestCount = 1;
    apiRateLimit.lastRequest = now;
    return true;
  } else {
    // Innerhalb des Zeitfensters
    apiRateLimit.requestCount++;
    if (apiRateLimit.requestCount > RATE_LIMIT_MAX_REQUESTS) {
      return false; // Rate-Limit überschritten
    }
    return true;
  }
}

void handleRoot() {
  server.send_P(200, "text/html", WEB_INTERFACE_HTML);
}

void handleStatus() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buf[16];
  int displayHour = t ? formatHourForDisplay(t->tm_hour) : 0;
  const char* suffix = (!use24HourFormat && t) ? (t->tm_hour >= 12 ? " PM" : " AM") : "";
  if (t) {
    if (use24HourFormat) {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", displayHour, t->tm_min, t->tm_sec);
    } else {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d %s", displayHour, t->tm_min, t->tm_sec, suffix);
    }
  } else {
    strncpy(buf, "--:--:--", sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
  }
  uint16_t sensorValue = analogRead(LIGHT_SENSOR_PIN);
  const char* hourFormatStr = use24HourFormat ? "24h" : "12h";
  
  // JSON mit snprintf für bessere Performance (statt String-Konkatenation)
  // Buffer erweitert für Effekt-Liste und HA-Kompatibilität
  char json[1536];
  String ipAddress = WiFi.localIP().toString();
  
  // Effekt-Liste als JSON-Array erstellen
  String effectList = "[";
  for (uint8_t i = 0; i < effectCount; i++) {
    if (i > 0) effectList += ",";
    effectList += "\"";
    effectList += effects[i]->name;
    effectList += "\"";
  }
  effectList += "]";
  
  // Dynamischer Hostname basierend auf Chip-ID (eindeutig pro Gerät)
  char hostname[32];
  snprintf(hostname, sizeof(hostname), "IkeaClock-%x", ESP.getChipId());
  
  // JSON-Response mit beiden Feldnamen (alte für Web-Interface, neue für HA-Integration)
  snprintf(json, sizeof(json),
    "{\"time\":\"%s\",\"effect\":\"%s\",\"currentEffect\":\"%s\",\"tz\":\"%s\",\"timezone\":\"%s\",\"hourFormat\":\"%s\",\"use24HourFormat\":%s,\"brightness\":%d,"
    "\"autoBrightness\":%s,\"autoBrightnessEnabled\":%s,\"minBrightness\":%d,\"maxBrightness\":%d,"
    "\"autoBrightnessMin\":%d,\"autoBrightnessMax\":%d,\"sensorMin\":%d,\"sensorMax\":%d,"
    "\"autoBrightnessSensorMin\":%d,\"autoBrightnessSensorMax\":%d,\"sensorValue\":%d,"
    "\"mqttEnabled\":%s,\"mqttConnected\":%s,\"mqttServer\":\"%s\","
    "\"mqttPort\":%d,\"mqttTopic\":\"%s\",\"presenceDetected\":%s,\"presence\":%s,"
    "\"displayEnabled\":%s,\"haDisplayControlled\":%s,\"presenceTimeout\":%lu,"
    "\"availableEffects\":%s,"
    "\"otaEnabled\":%s,\"otaHostname\":\"%s\",\"ipAddress\":\"%s\","
    "\"firmwareVersion\":\"%s\",\"version\":\"%s\"}",
    buf, currentEffect->name, currentEffect->name, tzString.c_str(), tzString.c_str(), hourFormatStr, use24HourFormat ? "true" : "false", brightness,
    autoBrightnessEnabled ? "true" : "false", autoBrightnessEnabled ? "true" : "false", minBrightness, maxBrightness,
    minBrightness, maxBrightness, sensorMin, sensorMax,
    sensorMin, sensorMax, sensorValue,
    mqttEnabled ? "true" : "false", mqttClient.connected() ? "true" : "false",
    mqttServer.c_str(), mqttPort, mqttPresenceTopic.c_str(),
    presenceDetected ? "true" : "false", presenceDetected ? "true" : "false",
    displayEnabled ? "true" : "false",
    haDisplayControlled ? "true" : "false", presenceTimeout,
    effectList.c_str(),
    (WiFi.status() == WL_CONNECTED) ? "true" : "false", hostname, ipAddress.c_str(),
    FIRMWARE_VERSION, FIRMWARE_VERSION);
  server.send(200, "application/json", json);
}

void handleSetTimezone() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("tz")) {
    tzString = server.arg("tz");
    setenv("TZ", tzString.c_str(), 1);
    tzset();
    // Debug: Neue Zeit nach Zeitzone-Änderung
    time_t now = time(nullptr);
    struct tm *local = localtime(&now);
    if (local) {
      Serial.printf("Timezone changed to: %s, Local time: %02d:%02d:%02d\n",
                    tzString.c_str(), local->tm_hour, local->tm_min, local->tm_sec);
    }
    server.send(200, "application/json", String("{\"tz\":\"") + tzString + "\"}" );
  } else {
    server.send(400, "text/plain", "Missing tz");
  }
}

void handleSetClockFormat() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (!server.hasArg("format")) {
    server.send(400, "text/plain", "Missing format");
    return;
  }

  String format = server.arg("format");
  format.trim();
  format.toLowerCase();

  bool newUse24Hour = true;
  if (format == "24" || format == "24h") {
    newUse24Hour = true;
  } else if (format == "12" || format == "12h") {
    newUse24Hour = false;
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid format, expected 12 or 24\"}");
    return;
  }

  use24HourFormat = newUse24Hour;
  persistBrightnessToStorage();

  server.send(200, "application/json",
              String("{\"hourFormat\":\"") + (use24HourFormat ? "24h" : "12h") +
              "\",\"use24HourFormat\":" + (use24HourFormat ? "true" : "false") + "}");
}

void handleSetBrightness() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("b")) {
    brightness = constrain(server.arg("b").toInt(), 0, 1023);
    analogWrite(PIN_ENABLE, 1023 - brightness);

    // Auto-Brightness deaktivieren bei manueller Helligkeitsänderung
    if (autoBrightnessEnabled) {
      autoBrightnessEnabled = false;
      Serial.println("Auto-Brightness disabled due to manual brightness change");
    }

    persistBrightnessToStorage();
    server.send(200, "application/json", String("{\"brightness\":") + brightness + "}");
  } else {
    server.send(400, "text/plain", "Missing b");
  }
}

void handleSetAutoBrightness() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  // Parameter aus HTTP-Request lesen und validieren
  if (server.hasArg("enabled")) {
    String val = server.arg("enabled");
    autoBrightnessEnabled = (val == "true" || val == "1");
  }
  if (server.hasArg("min")) {
    minBrightness = constrain(server.arg("min").toInt(), 0, 1023);
  }
  if (server.hasArg("max")) {
    maxBrightness = constrain(server.arg("max").toInt(), 0, 1023);
  }
  if (server.hasArg("sensorMin")) {
    sensorMin = constrain(server.arg("sensorMin").toInt(), 0, 1023);
  }
  if (server.hasArg("sensorMax")) {
    sensorMax = constrain(server.arg("sensorMax").toInt(), 0, 1023);
  }

  // Sicherstellen dass min < max (automatische Korrektur ungültiger Werte)
  if (minBrightness > maxBrightness) {
    uint16_t temp = minBrightness;
    minBrightness = maxBrightness;
    maxBrightness = temp;
  }
  if (sensorMin > sensorMax) {
    uint16_t temp = sensorMin;
    sensorMin = sensorMax;
    sensorMax = temp;
  }

  persistBrightnessToStorage();

  // JSON-Response erstellen
  String json = String("{\"autoBrightness\":") + (autoBrightnessEnabled ? "true" : "false") +
                ",\"minBrightness\":" + String(minBrightness) +
                ",\"maxBrightness\":" + String(maxBrightness) +
                ",\"sensorMin\":" + String(sensorMin) +
                ",\"sensorMax\":" + String(sensorMax) + "}";
  server.send(200, "application/json", json);
}

void handleSetMqtt() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  // MQTT-Konfiguration setzen
  if (server.hasArg("enabled")) {
    String val = server.arg("enabled");
    bool newMqttEnabled = (val == "true" || val == "1");

    // Wenn MQTT deaktiviert wird, Display immer einschalten
    if (mqttEnabled && !newMqttEnabled) {
      displayEnabled = true;
      analogWrite(PIN_ENABLE, 1023 - brightness);
      Serial.println("MQTT disabled, display forced ON");
    }

    // Wenn MQTT aktiviert wird, HA-Steuerung zurücksetzen (MQTT hat dann Vorrang)
    if (!mqttEnabled && newMqttEnabled) {
      haDisplayControlled = false;
      Serial.println("MQTT enabled, HA display control reset");
    }

    mqttEnabled = newMqttEnabled;
  }
  if (server.hasArg("server")) {
    mqttServer = server.arg("server");
  }
  if (server.hasArg("port")) {
    mqttPort = constrain(server.arg("port").toInt(), 1, 65535);
  }
  if (server.hasArg("user")) {
    mqttUser = server.arg("user");
  }
  if (server.hasArg("password")) {
    mqttPassword = server.arg("password");
  }
  if (server.hasArg("topic")) {
    mqttPresenceTopic = server.arg("topic");
  }
  if (server.hasArg("timeout")) {
    presenceTimeout = constrain(server.arg("timeout").toInt(), 1000, 3600000); // 1s bis 1h
  }

  persistMqttToStorage();

  // MQTT neu konfigurieren wenn aktiviert
  if (mqttEnabled && mqttServer.length() > 0) {
    mqttClient.disconnect();
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
    Serial.println("MQTT configuration updated, will reconnect...");
  }

  String json = String("{\"mqttEnabled\":") + (mqttEnabled ? "true" : "false") +
                ",\"mqttServer\":\"" + mqttServer + "\"" +
                ",\"mqttPort\":" + String(mqttPort) +
                ",\"mqttTopic\":\"" + mqttPresenceTopic + "\"" +
                ",\"presenceTimeout\":" + String(presenceTimeout) + "}";
  server.send(200, "application/json", json);
}

void handleSetDisplay() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("enabled")) {
    String val = server.arg("enabled");
    val.toLowerCase();
    val.trim();
    bool newState = (val == "true" || val == "1");
    
    displayEnabled = newState;
    haDisplayControlled = true; // Aktiviert HA-Steuerung (deaktiviert MQTT-Präsenz)
    
    if (displayEnabled) {
      // Display einschalten
      if (autoBrightnessEnabled) {
        // Auto-Brightness wird die Helligkeit automatisch anpassen
        Serial.println("Display ENABLED via HA (auto-brightness active)");
      } else {
        analogWrite(PIN_ENABLE, 1023 - brightness);
        Serial.printf("Display ENABLED via HA (brightness: %d)\n", brightness);
      }
    } else {
      // Display ausschalten (Helligkeit auf 0)
      analogWrite(PIN_ENABLE, 1023);
      Serial.println("Display DISABLED via HA");
    }
    
    Serial.println("MQTT presence control DISABLED (HA active)");
    
    server.send(200, "application/json", 
                String("{\"displayEnabled\":") + (displayEnabled ? "true" : "false") + 
                ",\"haDisplayControlled\":true}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing enabled parameter\"}");
  }
}

bool applyEffect(uint8_t idx) {
  if (idx >= effectCount) {
    return false;
  }
  currentEffectIndex = idx;
  currentEffect = effects[currentEffectIndex];
  currentEffect->init();
  yield();
  return true;
}

void selectEffect(uint8_t idx) {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  bool ok = applyEffect(idx);
  WiFiClient client = server.client();
  if (!client || !client.connected()) {
    Serial.printf("selectEffect(%u) called without active HTTP client (ok=%d)\n", idx, ok);
    return;
  }
  if (!ok) {

    server.send(400, "application/json", "{\"error\":\"invalid effect\"}");
    return;
  }
  server.send(200, "application/json", String("{\"effect\":\"") + currentEffect->name + "\"}");
}

void nextEffect() {
  applyEffect((currentEffectIndex + 1) % effectCount);
  }

void startAnimation() {
  uint8_t frame[32];
  clearFrame(frame, sizeof(frame));
  for (int r = 0; r < 8; r++) {
    for (int x = 7 - r; x <= 8 + r; x++) {
      setPixel(frame, x, 7 - r, true);
      setPixel(frame, x, 8 + r, true);
    }
    for (int y = 7 - r; y <= 8 + r; y++) {
      setPixel(frame, 7 - r, y, true);
      setPixel(frame, 8 + r, y, true);
    }
    shiftOutBuffer(frame, sizeof(frame));
    ESP.wdtFeed();
    delay(80);
  }
  delay(300);
  clearFrame(frame, sizeof(frame));
  shiftOutBuffer(frame, sizeof(frame));
}

const char *wifiStatusToString(uint8_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

bool setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    yield();
    ESP.wdtFeed();
  }

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Free heap after WiFi: %d bytes\n", ESP.getFreeHeap());
    return true;
  }

  Serial.printf("\nWiFi connection failed (status: %s). Will keep running and retry in loop.\n",
                wifiStatusToString(status));
  return false;
}

void setupNTP() {
  // WICHTIG: configTime() speichert möglicherweise nur die Pointer, nicht die Strings
  // Daher müssen wir sicherstellen, dass die Pointer während der gesamten Laufzeit gültig bleiben
  // Lösung: Statische char-Arrays verwenden, die während der gesamten Laufzeit existieren
  static char ntp1[64] = {0};
  static char ntp2[64] = {0};
  
  // Validierung: Prüfe ob Strings gültig sind, sonst verwende Standard-Server
  const char* server1;
  const char* server2;
  
  // Prüfe ob Strings gültig sind (nicht leer und nicht nur Whitespace)
  if (ntpServer1.length() > 0 && ntpServer1.length() < 64) {
    server1 = ntpServer1.c_str();
  } else {
    server1 = "pool.ntp.org";
    Serial.println("Warning: ntpServer1 invalid, using default");
  }
  
  if (ntpServer2.length() > 0 && ntpServer2.length() < 64) {
    server2 = ntpServer2.c_str();
  } else {
    server2 = "time.nist.gov";
    Serial.println("Warning: ntpServer2 invalid, using default");
  }
  
  // Arrays zuerst mit Null-Bytes füllen (Sicherheit)
  memset(ntp1, 0, sizeof(ntp1));
  memset(ntp2, 0, sizeof(ntp2));
  
  // Strings in statische Arrays kopieren (bleiben während der gesamten Laufzeit gültig)
  strncpy(ntp1, server1, sizeof(ntp1) - 1);
  ntp1[sizeof(ntp1) - 1] = '\0';
  strncpy(ntp2, server2, sizeof(ntp2) - 1);
  ntp2[sizeof(ntp2) - 1] = '\0';
  
  Serial.printf("NTP servers: '%s', '%s'\n", ntp1, ntp2);
  Serial.printf("Original strings - ntpServer1.length()=%d, ntpServer2.length()=%d\n", ntpServer1.length(), ntpServer2.length());
  
  configTime(0, 0, ntp1, ntp2);
  // Zeitzone mit POSIX TZ String setzen - DST (Sommer-/Winterzeit) wird automatisch berücksichtigt
  setenv("TZ", tzString.c_str(), 1);
  tzset(); // Zeitzone anwenden

  Serial.print("Waiting for NTP sync");
  int attempts = 0;
  while (time(nullptr) < 100000 && attempts < 20) {
    delay(500);
    Serial.print(".");
    yield();
    attempts++;
    ESP.wdtFeed();
  }

  if (time(nullptr) < 100000) {
    Serial.println("\nNTP sync failed, continuing anyway...");
  } else {
    Serial.println("\nNTP sync successful!");
    // Debug: UTC und lokale Zeit ausgeben
    time_t now = time(nullptr);
    struct tm *utc = gmtime(&now);
    struct tm *local = localtime(&now);
    if (utc && local) {
      Serial.printf("UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
                    utc->tm_hour, utc->tm_min, utc->tm_sec);
      Serial.printf("Local time: %04d-%02d-%02d %02d:%02d:%02d (TZ: %s)\n",
                    local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                    local->tm_hour, local->tm_min, local->tm_sec, tzString.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Starting up... Free heap: %d bytes\n", ESP.getFreeHeap());
  system_set_os_print(1); // Debug-Ausgaben aktivieren

  // FIX: Watchdog sofort füttern (verhindert Neustarts während Setup)
  ESP.wdtFeed();

  // SPIFFS initialisieren (für Restart-Logging und ggf. Debug-Logging)
  bool spiffsOk = SPIFFS.begin();
  
  // Automatisches Restart-Logging (immer aktiv, unabhängig von DEBUG_LOGGING_ENABLED)
  if (spiffsOk) {
    logRestart();
    String resetReason = ESP.getResetReason();
    int freeHeap = ESP.getFreeHeap();
    int maxFreeBlock = ESP.getMaxFreeBlockSize();
    Serial.printf("Reset reason: %s, Free heap: %d, Max free block: %d\n", resetReason.c_str(), freeHeap, maxFreeBlock);
  }

#ifdef DEBUG_LOGGING_ENABLED
  // Debug-Logging initialisieren (nur wenn Flag gesetzt)
  if (!spiffsOk) {
    Serial.println("SPIFFS initialization failed! Debug logging disabled.");
  } else {
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
      Serial.printf("SPIFFS: %d bytes total, %d bytes used, %d bytes free\n", 
                    fs_info.totalBytes, fs_info.usedBytes, 
                    fs_info.totalBytes - fs_info.usedBytes);
    }
    // Alte Logs nur löschen wenn genug Platz vorhanden
    if (SPIFFS.exists("/debug.log")) {
      File logCheck = SPIFFS.open("/debug.log", "r");
      if (logCheck) {
        size_t logSize = logCheck.size();
        logCheck.close();
        // Nur löschen wenn Log sehr groß (>64KB) oder wenig Speicher
        if (logSize > 65536 || (fs_info.totalBytes - fs_info.usedBytes < 8192)) {
          SPIFFS.remove("/debug.log");
          Serial.println("Old debug.log removed due to size or low memory");
        }
      }
    }
    debugLogJson("setup", "System startup", "A", "{\"freeHeap\":%d}", ESP.getFreeHeap());
  }
#endif // DEBUG_LOGGING_ENABLED

  EEPROM.begin(EEPROM_SIZE);
  loadBrightnessFromStorage();
  
  // FIX: Watchdog während längerer Operationen füttern
  ESP.wdtFeed();

  matrixSetup();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ESP.wdtFeed(); // Watchdog während Setup füttern

  startAnimation();
  ESP.wdtFeed(); // Watchdog nach Animation füttern

  bool wifiConnected = setupWiFi();
  ESP.wdtFeed(); // Watchdog nach WiFi-Setup füttern
  ntpConfigured = wifiConnected;
  if (wifiConnected) {
    setupNTP();
    ESP.wdtFeed(); // Watchdog nach NTP-Setup füttern
    
    // Dynamischer Hostname basierend auf Chip-ID (eindeutig pro Gerät)
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "IkeaClock-%x", ESP.getChipId());
    
    // ArduinoOTA Setup
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword("admin"); // Passwort für OTA-Updates (sollte geändert werden!)
    
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_SPIFFS
        type = "filesystem";
      }
      Serial.println("Start updating " + type);
      // Display ausschalten während Update
      analogWrite(PIN_ENABLE, 1023);
    });
    
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
    
    ArduinoOTA.begin();
    
    // mDNS für Auto-Discovery in Home Assistant
    if (MDNS.begin(hostname)) {
      Serial.println("mDNS responder started");
      Serial.printf("mDNS hostname: %s.local\n", hostname);
      // HTTP Service für Auto-Discovery registrieren
      MDNS.addService("http", "tcp", 80);
      MDNS.addServiceTxt("http", "tcp", "device", "IkeaObegraensad");
      MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
    
    Serial.println("OTA ready");
    Serial.printf("OTA Hostname: %s\n", hostname);
    Serial.printf("OTA IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("OTA Port: 3232\n");
    Serial.println("========================================");
    Serial.println("OTA Update Instructions:");
    Serial.println("1. In Arduino IDE: Tools -> Port -> Network Ports");
    Serial.printf("2. Select: %s or %s.local\n", WiFi.localIP().toString().c_str(), hostname);
    Serial.println("3. Click Upload (password: admin)");
    Serial.println("========================================");
  }

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/setTimezone", handleSetTimezone);
  server.on("/api/setClockFormat", handleSetClockFormat);
  server.on("/api/setBrightness", handleSetBrightness);
  server.on("/api/setAutoBrightness", handleSetAutoBrightness);
  server.on("/api/setMqtt", handleSetMqtt);
  server.on("/api/setDisplay", handleSetDisplay);  // HA-Integration Endpoint
  server.on("/effect/snake", []() { selectEffect(0); });
  server.on("/effect/clock", []() { selectEffect(1); });
  server.on("/effect/rain", []() { selectEffect(2); });
  server.on("/effect/bounce", []() { selectEffect(3); });
  server.on("/effect/stars", []() { selectEffect(4); });
  server.on("/effect/lines", []() { selectEffect(5); });
  server.on("/effect/pulse", []() { selectEffect(6); });
  server.on("/effect/waves", []() { selectEffect(7); });
  server.on("/effect/spiral", []() { selectEffect(8); });
  server.on("/effect/fire", []() { selectEffect(9); });
  server.on("/effect/plasma", []() { selectEffect(10); });
  server.on("/effect/ripple", []() { selectEffect(11); });
  server.on("/effect/sandclock", []() { selectEffect(12); });
  server.on("/api/debuglog", []() {
    if (!SPIFFS.exists("/")) {
      server.send(503, "text/plain", "SPIFFS not available");
      return;
    }
    File logFile = SPIFFS.open("/debug.log", "r");
    if (logFile) {
      server.streamFile(logFile, "application/x-ndjson");
      logFile.close();
    } else {
      server.send(404, "text/plain", "Debug log not found");
    }
  });
  
  // Backup-Endpoint: Export aller Konfiguration als JSON
  server.on("/api/backup", []() {
    if (!checkRateLimit()) {
      server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
      return;
    }
    
    // Erstelle JSON mit allen Konfigurationsdaten
    char json[1024];
    time_t now = time(nullptr);
    snprintf(json, sizeof(json),
      "{\"version\":%d,\"timestamp\":%lu,\"checksum\":%d,"
      "\"config\":{"
      "\"brightness\":%d,\"autoBrightness\":%s,"
      "\"minBrightness\":%d,\"maxBrightness\":%d,"
      "\"sensorMin\":%d,\"sensorMax\":%d,"
      "\"mqttEnabled\":%s,\"mqttServer\":\"%s\",\"mqttPort\":%d,"
      "\"mqttUser\":\"%s\",\"mqttTopic\":\"%s\",\"presenceTimeout\":%lu,"
      "\"tz\":\"%s\",\"hourFormat\":\"%s\",\"use24HourFormat\":%s,\"ntpServer1\":\"%s\",\"ntpServer2\":\"%s\""
      "}}",
      EEPROM_VERSION, (unsigned long)now, calculateEEPROMChecksum(),
      brightness, autoBrightnessEnabled ? "true" : "false",
      minBrightness, maxBrightness,
      sensorMin, sensorMax,
      mqttEnabled ? "true" : "false", mqttServer.c_str(), mqttPort,
      mqttUser.c_str(), mqttPresenceTopic.c_str(), presenceTimeout,
      tzString.c_str(), use24HourFormat ? "24h" : "12h", use24HourFormat ? "true" : "false", ntpServer1.c_str(), ntpServer2.c_str());
    
    server.send(200, "application/json", json);
  });
  
  // Restore-Endpoint: Import Konfiguration aus JSON
  server.on("/api/restore", HTTP_POST, []() {
    if (!checkRateLimit()) {
      server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
      return;
    }
    
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"Missing JSON data\"}");
      return;
    }
    
    String jsonData = server.arg("plain");
    
    // Einfaches JSON-Parsing (ohne Bibliothek)
    // Suche nach Version
    int versionPos = jsonData.indexOf("\"version\":");
    if (versionPos < 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid backup format: missing version\"}");
      return;
    }
    
    // Extrahiere Version
    int versionStart = jsonData.indexOf(":", versionPos) + 1;
    int versionEnd = jsonData.indexOf(",", versionStart);
    if (versionEnd < 0) versionEnd = jsonData.indexOf("}", versionStart);
    int version = jsonData.substring(versionStart, versionEnd).toInt();
    
    if (version != EEPROM_VERSION) {
      server.send(400, "application/json", "{\"error\":\"Backup version mismatch\"}");
      return;
    }
    
    // Extrahiere config-Objekt (vereinfacht)
    int configStart = jsonData.indexOf("\"config\":{");
    if (configStart < 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid backup format: missing config\"}");
      return;
    }
    
    // Parse einzelne Werte (vereinfachte Implementierung)
    // In Produktion sollte eine JSON-Bibliothek verwendet werden
    // Hier nur grundlegende Validierung und Warnung
    Serial.println("Restore request received (basic validation only)");
    Serial.printf("Backup data length: %d\n", jsonData.length());
    
    // Für vollständige Implementierung: JSON-Bibliothek verwenden
    // Hier nur Bestätigung senden
    server.send(200, "application/json", "{\"status\":\"restore_not_fully_implemented\",\"message\":\"Basic validation passed. Full restore requires JSON library.\"}");
  });
  if (wifiConnected) {
    server.begin();
    serverStarted = true;
  } else {
    Serial.println("Web server not started because WiFi is not connected.");
  }

  // MQTT initialisieren falls konfiguriert
  if (mqttEnabled && mqttServer.length() > 0) {
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
    Serial.printf("MQTT enabled, server: %s:%d, topic: %s\n",
                  mqttServer.c_str(), mqttPort, mqttPresenceTopic.c_str());
  } else {
    // Wenn MQTT deaktiviert, Display immer an
    displayEnabled = true;
    Serial.println("MQTT disabled, display always ON");
  }

  applyEffect(currentEffectIndex);
}

void loop() {
  static unsigned long lastFrameUpdate = 0;
  static unsigned long lastButtonCheck = 0;
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastStatusPrint = 0;
  static unsigned long lastBrightnessUpdate = 0;
  static unsigned long lastMqttReconnect = 0;
  static unsigned long lastPresenceCheck = 0;
  static unsigned long lastWatchdogFeed = 0;
  static unsigned long loopCount = 0;
  static unsigned long lastLoopStart = 0;
  
  // #region agent log
  unsigned long loopStart = millis();
  unsigned long loopDuration = 0;
  if (lastLoopStart > 0) {
    loopDuration = timeDiff(loopStart, lastLoopStart);
    if (loopDuration > 100) { // Warnung wenn Loop länger als 100ms
      int freeHeap = ESP.getFreeHeap();
      int maxFreeBlock = ESP.getMaxFreeBlockSize();
      if (SPIFFS.exists("/")) {
        File logFile = SPIFFS.open("/debug.log", "a");
        if (logFile) {
          logFile.printf("{\"id\":\"loop_slow_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"Loop duration slow\",\"data\":{\"duration\":%lu,\"freeHeap\":%d,\"maxFreeBlock\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\"}\n",
                         millis(), millis(), loopDuration, freeHeap, maxFreeBlock);
          logFile.close();
        }
      }
    }
  }
  lastLoopStart = loopStart;
  // #endregion
  
  // FIX: Watchdog sofort füttern zu Beginn jedes Loops (verhindert Neustarts)
  ESP.wdtFeed();
  
  // FIX: lastWatchdogFeed beim ersten Loop-Durchlauf initialisieren
  unsigned long now = millis();
  if (lastWatchdogFeed == 0) {
    lastWatchdogFeed = now;
  }
  
  // #region agent log
  unsigned long timeSinceWatchdog = timeDiff(now, lastWatchdogFeed);
  if (timeSinceWatchdog > 5000) { // Warnung wenn länger als 5s seit letztem Feed
    int freeHeap = ESP.getFreeHeap();
    int maxFreeBlock = ESP.getMaxFreeBlockSize();
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"watchdog_delayed_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"Watchdog feed delayed\",\"data\":{\"timeSinceWatchdog\":%lu,\"freeHeap\":%d,\"maxFreeBlock\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\"}\n",
                       millis(), millis(), timeSinceWatchdog, freeHeap, maxFreeBlock);
        logFile.close();
      }
    }
  }
  lastWatchdogFeed = now;
  // #endregion

#ifdef DEBUG_LOGGING_ENABLED
  loopCount++;
  if (loopCount % 100 == 0) { // Jeden 100. Loop
    unsigned long timeSinceWatchdog = timeDiff(now, lastWatchdogFeed);
    int freeHeap = ESP.getFreeHeap();
    debugLogJson("loop", "Loop iteration", "A", "{\"loopCount\":%lu,\"timeSinceWatchdog\":%lu,\"freeHeap\":%d}", loopCount, timeSinceWatchdog, freeHeap);
  }
#endif

  server.handleClient();
  yield();
  
  // ArduinoOTA Handler (muss regelmäßig aufgerufen werden)
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    // mDNS Handler (muss regelmäßig aufgerufen werden)
    MDNS.update();
  }
  yield();

  // MQTT Loop (muss regelmäßig aufgerufen werden)
  if (mqttEnabled) {
    mqttClient.loop();
  }

  if (timeDiff(millis(), lastWiFiCheck) > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      // #region agent log
      unsigned long wifiReconnectStart = millis();
      int freeHeapBefore = ESP.getFreeHeap();
      int maxFreeBlockBefore = ESP.getMaxFreeBlockSize();
      ESP.wdtFeed(); // Watchdog vor blockierender Operation füttern
      // #endregion
      WiFi.reconnect();
      // #region agent log
      unsigned long wifiReconnectDuration = millis() - wifiReconnectStart;
      int freeHeapAfter = ESP.getFreeHeap();
      int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
      ESP.wdtFeed(); // Watchdog nach blockierender Operation füttern
      if (wifiReconnectDuration > 1000 || freeHeapBefore != freeHeapAfter) {
        if (SPIFFS.exists("/")) {
          File logFile = SPIFFS.open("/debug.log", "a");
          if (logFile) {
            logFile.printf("{\"id\":\"wifi_reconnect_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"WiFi reconnect\",\"data\":{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                           millis(), millis(), wifiReconnectDuration, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
            logFile.close();
          }
        }
      }
      // #endregion
      serverStarted = false;
      ntpConfigured = false;
    }
    lastWiFiCheck = millis();
  }

  if (!serverStarted && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected, starting web server...");
    server.begin();
    serverStarted = true;
  }

  if (!ntpConfigured && WiFi.status() == WL_CONNECTED) {
    // #region agent log
    unsigned long ntpStart = millis();
    int freeHeapBefore = ESP.getFreeHeap();
    int maxFreeBlockBefore = ESP.getMaxFreeBlockSize();
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"ntp_start_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"NTP setup start\",\"data\":{\"freeHeap\":%d,\"maxFreeBlock\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), freeHeapBefore, maxFreeBlockBefore);
        logFile.close();
      }
    }
    // #endregion
    setupNTP();
    // #region agent log
    unsigned long ntpDuration = millis() - ntpStart;
    int freeHeapAfter = ESP.getFreeHeap();
    int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"ntp_end_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"NTP setup end\",\"data\":{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), ntpDuration, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
        logFile.close();
      }
    }
    // #endregion
    ntpConfigured = true;
  }

  // Automatisches Senden von Logs an Server (falls konfiguriert)
  if (WiFi.status() == WL_CONNECTED && logServerUrl.length() > 0) {
    if (timeDiff(millis(), lastLogUpload) >= LOG_UPLOAD_INTERVAL) {
      sendLogsToServer();
      lastLogUpload = millis();
    }
  }

  // MQTT Reconnection mit Exponential Backoff
  if (mqttEnabled && !mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    if (timeDiff(millis(), lastMqttReconnect) >= mqttReconnectBackoff) {
      reconnectMQTT();
      lastMqttReconnect = millis();
    }
  }

  // Präsenz-Timeout prüfen (alle 2 Sekunden)
  if (mqttEnabled && timeDiff(millis(), lastPresenceCheck) > 2000) {
    updatePresenceTimeout();
    lastPresenceCheck = millis();
  }

  if (timeDiff(millis(), lastButtonCheck) > 50) {
    static unsigned long lastPress = 0;
    if (digitalRead(BUTTON_PIN) == LOW && timeDiff(millis(), lastPress) > 300) {
      nextEffect();
      lastPress = millis();
    }
    lastButtonCheck = millis();
  }

  // Frame nur zeichnen wenn Display aktiviert ist
  if (timeDiff(millis(), lastFrameUpdate) > 50) {
    if (displayEnabled) {
#ifdef DEBUG_LOGGING_ENABLED
      unsigned long frameStart = millis();
#endif
      uint8_t frame[32];
      clearFrame(frame, sizeof(frame));
      currentEffect->draw(frame);
      shiftOutBuffer(frame, sizeof(frame));
#ifdef DEBUG_LOGGING_ENABLED
      unsigned long frameDuration = millis() - frameStart;
      if (frameDuration > 30) { // Nur loggen wenn langsam
        debugLogJson("loop", "Frame draw slow", "B", "{\"duration\":%lu}", frameDuration);
      }
#endif
    }
    lastFrameUpdate = millis();
  }

  if (timeDiff(millis(), lastStatusPrint) > 60000) {
    int freeHeap = ESP.getFreeHeap();
    int maxFreeBlock = ESP.getMaxFreeBlockSize();
    Serial.printf("Uptime: %lus, Free heap: %d bytes, Max free block: %d bytes, Display: %s, Presence: %s\n",
                  millis() / 1000, freeHeap, maxFreeBlock,
                  displayEnabled ? "ON" : "OFF",
                  presenceDetected ? "DETECTED" : "NOT DETECTED");
    // #region agent log
    // Regelmäßige Heap-Überwachung (Hypothese B: Heap-Fragmentierung)
    int heapFragmentation = freeHeap - maxFreeBlock;
    if (heapFragmentation > 10000 || freeHeap < 10000 || maxFreeBlock < 5000) {
      if (SPIFFS.exists("/")) {
        File logFile = SPIFFS.open("/debug.log", "a");
        if (logFile) {
          logFile.printf("{\"id\":\"heap_status_%lu\",\"timestamp\":%lu,\"location\":\"loop\",\"message\":\"Heap status check\",\"data\":{\"freeHeap\":%d,\"maxFreeBlock\":%d,\"fragmentation\":%d,\"uptime\":%lu},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"B\"}\n",
                         millis(), millis(), freeHeap, maxFreeBlock, heapFragmentation, millis() / 1000);
          logFile.close();
        }
      }
    }
    // #endregion
    lastStatusPrint = millis();
  }

  // VERBESSERT: Auto-Brightness mit non-blocking Sampling
  // Wird jetzt in jedem Loop-Durchlauf verarbeitet statt blockierend
  if (autoBrightnessEnabled && displayEnabled) {
    // processLightSensorSample() ist non-blocking und gibt false zurück solange sampling läuft
    processLightSensorSample();
  }

  // Nur einen neuen Sample-Zyklus starten wenn der vorherige abgeschlossen ist
  if (timeDiff(millis(), lastBrightnessUpdate) > AUTO_BRIGHTNESS_UPDATE_INTERVAL) {
    updateAutoBrightness();
    lastBrightnessUpdate = millis();
  }

  yield();
  delay(1);
}

// Funktion zum Senden von Logs an einen Server
// Sendet alle Logs aus /debug.log als NDJSON (eine Zeile pro JSON-Objekt) per HTTP POST
// Löscht die Datei nach erfolgreichem Senden (HTTP 200-299)
// Konfiguration: Definiere LOG_SERVER_URL in secrets.h (z.B. "http://192.168.1.100:3000/logs")
// Deaktiviert wenn LOG_SERVER_URL leer oder nicht definiert ist
bool sendLogsToServer() {
  // Prüfe ob WiFi verbunden und Server-URL gesetzt ist
  if (WiFi.status() != WL_CONNECTED || logServerUrl.length() == 0) {
    return false;
  }
  
  // Prüfe ob Log-Datei existiert
  if (!SPIFFS.exists("/debug.log")) {
    return true; // Keine Logs zum Senden ist kein Fehler
  }
  
  File logFile = SPIFFS.open("/debug.log", "r");
  if (!logFile) {
    Serial.println("[LOG_SERVER] Fehler beim Öffnen der Log-Datei");
    return false;
  }
  
  // Prüfe Dateigröße (max 32KB zum Senden auf einmal)
  size_t fileSize = logFile.size();
  if (fileSize == 0) {
    logFile.close();
    return true; // Leere Datei ist kein Fehler
  }
  
  if (fileSize > 32768) {
    Serial.printf("[LOG_SERVER] Log-Datei zu groß (%d bytes), überspringe Sendung\n", fileSize);
    logFile.close();
    return false;
  }
  
  // Lese gesamte Datei in String (reserviere Speicher vorher)
  String payload;
  payload.reserve(fileSize + 1);
  
  while (logFile.available()) {
    char c = logFile.read();
    payload += c;
    yield(); // Wichtig: yield für Watchdog
  }
  logFile.close();
  
  // Erstelle HTTP-Client und sende
  HTTPClient http;
  http.begin(espClient, logServerUrl);
  http.addHeader("Content-Type", "application/x-ndjson"); // NDJSON Format
  http.setTimeout(15000); // 15 Sekunden Timeout (für Connect und Read)
  
  ESP.wdtFeed(); // Watchdog vor blockierender Operation
  int httpCode = http.POST(payload);
  ESP.wdtFeed(); // Watchdog nach blockierender Operation
  
  http.end();
  
  // Prüfe Antwort-Code
  if (httpCode >= 200 && httpCode < 300) {
    // Erfolgreich gesendet - lösche Log-Datei
    SPIFFS.remove("/debug.log");
    Serial.printf("[LOG_SERVER] Logs erfolgreich gesendet (%d bytes, HTTP %d)\n", fileSize, httpCode);
    return true;
  } else {
    Serial.printf("[LOG_SERVER] Fehler beim Senden: HTTP %d\n", httpCode);
    return false;
  }
}

// Funktion zum automatischen Speichern von Restart-Logs (immer aktiv)
// Diese Funktion wird bei jedem Systemstart automatisch aufgerufen
void logRestart() {
  if (!SPIFFS.exists("/")) {
    return; // SPIFFS nicht verfügbar
  }
  
  String resetReason = ESP.getResetReason();
  int freeHeap = ESP.getFreeHeap();
  int maxFreeBlock = ESP.getMaxFreeBlockSize();
  unsigned long uptime = millis();
  
  File logFile = SPIFFS.open("/debug.log", "a");
  if (logFile) {
    logFile.printf("{\"id\":\"restart_%lu\",\"timestamp\":%lu,\"location\":\"setup\",\"message\":\"System restart detected\",\"data\":{\"resetReason\":\"%s\",\"freeHeap\":%d,\"maxFreeBlock\":%d,\"uptime\":%lu},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\"}\n",
                   uptime, uptime, resetReason.c_str(), freeHeap, maxFreeBlock, uptime);
    logFile.close();
  }
}

#ifdef DEBUG_LOGGING_ENABLED
// Debug-Logging Konstanten
const size_t MAX_LOG_SIZE = 32768; // 32KB maximale Log-Größe
const size_t LOG_ROTATE_SIZE = 16384; // Bei 16KB rotieren (ältere Hälfte löschen)

// Debug-Logging Funktion mit Rotation
void debugLog(const char* location, const char* message, const char* hypothesisId, const char* dataJson) {
  unsigned long now = millis();
  int freeHeap = ESP.getFreeHeap();
  
  // Prüfe ob SPIFFS verfügbar ist
  if (!SPIFFS.exists("/")) {
    // SPIFFS nicht verfügbar, nur Serial ausgeben
    Serial.printf("[DEBUG] %s:%s [%s] %s freeHeap=%d\n", location, message, hypothesisId, dataJson ? dataJson : "{}", freeHeap);
    return;
  }
  
  // Prüfe Log-Dateigröße und rotiere bei Bedarf
  if (SPIFFS.exists("/debug.log")) {
    File logCheck = SPIFFS.open("/debug.log", "r");
    if (logCheck) {
      size_t logSize = logCheck.size();
      logCheck.close();
      
      if (logSize > LOG_ROTATE_SIZE) {
        // Log-Rotation: Lese Datei, behalte nur die zweite Hälfte
        File logRead = SPIFFS.open("/debug.log", "r");
        if (logRead) {
          // Springe zur Mitte der Datei
          logRead.seek(logSize / 2, SeekSet);
          
          // Lese ab der Mitte und schreibe in temporäre Datei
          File logTemp = SPIFFS.open("/debug.tmp", "w");
          if (logTemp) {
            while (logRead.available()) {
              logTemp.write(logRead.read());
            }
            logTemp.close();
          }
          logRead.close();
          
          // Ersetze alte Datei durch rotierte Version
          SPIFFS.remove("/debug.log");
          if (SPIFFS.exists("/debug.tmp")) {
            File logOld = SPIFFS.open("/debug.tmp", "r");
            File logNew = SPIFFS.open("/debug.log", "w");
            if (logOld && logNew) {
              while (logOld.available()) {
                logNew.write(logOld.read());
              }
            }
            if (logOld) logOld.close();
            if (logNew) logNew.close();
            SPIFFS.remove("/debug.tmp");
          }
        }
      }
    }
  }
  
  // Prüfe verfügbaren SPIFFS-Speicher
  FSInfo fs_info;
  if (SPIFFS.info(fs_info)) {
    if (fs_info.totalBytes - fs_info.usedBytes < 1024) {
      // Weniger als 1KB frei, lösche Log komplett
      SPIFFS.remove("/debug.log");
      Serial.println("[DEBUG] SPIFFS fast voll, Log gelöscht");
    }
  }
  
  // Log in Datei schreiben
  File logFile = SPIFFS.open("/debug.log", "a");
  if (logFile) {
    logFile.printf("{\"id\":\"log_%lu\",\"timestamp\":%lu,\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"%s\",\"freeHeap\":%d,\"uptime\":%lu}\n",
                   now, now, location, message, dataJson ? dataJson : "{}", hypothesisId, freeHeap, now);
    logFile.close();
  }
  
  // Auch über Serial ausgeben (für sofortige Sichtbarkeit)
  Serial.printf("[DEBUG] %s:%s [%s] %s freeHeap=%d\n", location, message, hypothesisId, dataJson ? dataJson : "{}", freeHeap);
}

// Hilfsfunktion für JSON-String-Erstellung
void debugLogJson(const char* location, const char* message, const char* hypothesisId, const char* format, ...) {
  char dataJson[256];
  va_list args;
  va_start(args, format);
  vsnprintf(dataJson, sizeof(dataJson), format, args);
  va_end(args);
  debugLog(location, message, hypothesisId, dataJson);
}
#else
// Stub-Funktionen wenn Debug-Logging deaktiviert
inline void debugLog(const char* location, const char* message, const char* hypothesisId, const char* dataJson) {}
inline void debugLogJson(const char* location, const char* message, const char* hypothesisId, const char* format, ...) {}
#endif // DEBUG_LOGGING_ENABLED
