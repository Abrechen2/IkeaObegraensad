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
#define DEBUG_LOGGING_ENABLED

// Firmware-Version
#define FIRMWARE_VERSION "1.4.1"

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
#include "Logging.h"

ESP8266WebServer server(80);
bool serverStarted = false;
bool ntpConfigured = false;

// String-Limit-Konstanten (müssen vor ihrer Verwendung definiert werden)
// Input-Validierung: Realistische Limits für User-Input (größer für bessere UX)
const size_t INPUT_MQTT_SERVER_MAX = 128;
const size_t INPUT_MQTT_USER_MAX = 64;
const size_t INPUT_MQTT_PASSWORD_MAX = 64;
const size_t INPUT_MQTT_TOPIC_MAX = 128;
const size_t INPUT_TZ_MAX = 128;
const size_t INPUT_NTP_SERVER_MAX = 128;
const size_t INPUT_LOG_URL_MAX = 256;
const size_t INPUT_RESET_REASON_MAX = 64;
const size_t INPUT_OPERATION_MAX = 64;

// MQTT Configuration for Aqara Presence Sensor
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqttServer[INPUT_MQTT_SERVER_MAX] = "";  // MQTT Broker IP (wird über Web-UI konfiguriert, kein Default)
const uint16_t MQTT_PORT_DEFAULT = 1883; // Standard MQTT Port
uint16_t mqttPort = MQTT_PORT_DEFAULT;
char mqttUser[INPUT_MQTT_USER_MAX] = "";    // Optional
char mqttPassword[INPUT_MQTT_PASSWORD_MAX] = ""; // Optional
char mqttPresenceTopic[INPUT_MQTT_TOPIC_MAX] = "Sonstige/Präsenz_Wz/Anwesenheit"; // Topic für Präsenzmelder (FP2 sendet JSON an Haupt-Topic)
bool mqttEnabled = false;
bool presenceDetected = false;
unsigned long lastPresenceTime = 0;
const uint32_t PRESENCE_TIMEOUT_DEFAULT = 300000; // 5 Minuten in ms (Display bleibt 5 Min an nach letzter Erkennung)
uint32_t presenceTimeout = PRESENCE_TIMEOUT_DEFAULT;
bool displayEnabled = true; // Display-Status (wird durch Präsenz gesteuert)
bool haDisplayControlled = false; // Flag: Wird Display von HA gesteuert? (deaktiviert MQTT-Präsenz)

// Restart-Counter für Diagnose
uint32_t restartCount = 0;
char lastResetReason[INPUT_RESET_REASON_MAX] = "";

// Restart-Diagnose Variablen
unsigned long lastUptimeBeforeRestart = 0;
uint32_t lastHeapBeforeRestart = 0;
char lastOperation[INPUT_OPERATION_MAX] = "";

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

char logServerUrl[INPUT_LOG_URL_MAX] = LOG_SERVER_URL;
unsigned long logServerInterval = LOG_SERVER_INTERVAL;
unsigned long lastLogUpload = 0;
const unsigned long MQTT_CONNECT_TIMEOUT = 5000; // 5 Sekunden Timeout für Connect-Versuch

// Brightness Konstanten
const uint16_t PWM_MAX = 1023;              // Maximale PWM-Wert (ESP8266 analogWrite Range)
const uint16_t DEFAULT_BRIGHTNESS = 512;    // Standard-Helligkeit (50% von PWM_MAX)
const uint16_t MIN_BRIGHTNESS_DEFAULT = 100; // Minimale Helligkeit (Standard)
const uint16_t MAX_BRIGHTNESS_DEFAULT = PWM_MAX; // Maximale Helligkeit (Standard)

// Sensor-Konstanten
const uint16_t SENSOR_MIN_DEFAULT = 5;      // Minimaler Sensorwert (dunkel) - LDR-spezifisch
const uint16_t SENSOR_MAX_DEFAULT = 450;    // Maximaler Sensorwert (hell) - LDR-spezifisch

uint16_t brightness = DEFAULT_BRIGHTNESS; // 0..PWM_MAX

// Auto-Brightness Konfiguration
bool autoBrightnessEnabled = false;
uint16_t minBrightness = MIN_BRIGHTNESS_DEFAULT;   // Minimale Helligkeit (0-PWM_MAX)
uint16_t maxBrightness = MAX_BRIGHTNESS_DEFAULT;   // Maximale Helligkeit (0-PWM_MAX)
uint16_t sensorMin = SENSOR_MIN_DEFAULT;           // Minimaler Sensorwert (dunkel) - LDR-spezifisch
uint16_t sensorMax = SENSOR_MAX_DEFAULT;           // Maximaler Sensorwert (hell) - LDR-spezifisch
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

const uint8_t EEPROM_VERSION = 3;   // EEPROM-Layout Version
const uint8_t EEPROM_MAGIC = 0xB8;  // Magic Byte zur Erkennung initialisierter EEPROM
const uint16_t EEPROM_SIZE = 1024;   // Erweitert für MQTT-, NTP- und Log-Server-Konfiguration

// EEPROM-Speicher: Feste Größen für Kompatibilität (Version 3)
const size_t EEPROM_MQTT_SERVER_LEN = 64;
const size_t EEPROM_MQTT_USER_LEN = 32;
const size_t EEPROM_MQTT_PASSWORD_LEN = 32;
const size_t EEPROM_MQTT_TOPIC_LEN = 64;
const size_t EEPROM_NTP_SERVER_LEN = 64;
const size_t EEPROM_LOG_SERVER_URL_LEN = 128;
const size_t EEPROM_RESET_REASON_LEN = 32;
const size_t EEPROM_OPERATION_LEN = 32;

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
const uint16_t EEPROM_RESTART_COUNT_ADDR = 343;     // uint32_t (4 bytes)
const uint16_t EEPROM_LAST_RESET_REASON_ADDR = 347; // String (32 bytes)
const uint16_t EEPROM_LOG_SERVER_URL_ADDR = 379;     // String (128 bytes)
const uint16_t EEPROM_LOG_SERVER_INTERVAL_ADDR = 507; // uint32_t (4 bytes)
const uint16_t EEPROM_LAST_UPTIME_ADDR = 511;        // uint32_t (4 bytes) - Uptime vor letztem Restart
const uint16_t EEPROM_LAST_HEAP_BEFORE_RESTART_ADDR = 515; // uint32_t (4 bytes) - Heap vor Restart

// Buffer-Größen Konstanten
const size_t BUFFER_SIZE_HOSTNAME = 32;      // Hostname Buffer
const size_t BUFFER_SIZE_TIME = 16;          // Zeit-Buffer
const size_t BUFFER_SIZE_JSON_SMALL = 128;   // Kleiner JSON-Buffer
const size_t BUFFER_SIZE_JSON_MEDIUM = 256;  // Mittlerer JSON-Buffer
const size_t BUFFER_SIZE_JSON_LARGE = 512;   // Großer JSON-Buffer
const size_t BUFFER_SIZE_JSON_BACKUP = 1024; // Backup JSON-Buffer
const size_t BUFFER_SIZE_JSON_STATUS = 1536; // Status JSON-Buffer (groß wegen vieler Felder)
const size_t BUFFER_SIZE_CLIENT_ID = 32;     // MQTT Client-ID Buffer

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
char tzString[INPUT_TZ_MAX] = "CET-1CEST-2,M3.5.0/2,M10.5.0/3"; // default Europe/Berlin mit DST
char ntpServer1[INPUT_NTP_SERVER_MAX] = "0.de.pool.ntp.org"; // Primärer NTP-Server (konfigurierbar)
char ntpServer2[INPUT_NTP_SERVER_MAX] = "1.de.pool.ntp.org"; // Sekundärer NTP-Server (konfigurierbar)
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

uint8_t formatHourForDisplay(uint8_t hour) {
  if (use24HourFormat) {
    return hour;
  }

  uint8_t hour12 = hour % 12;
  return hour12 == 0 ? 12 : hour12;
}

// Helper-Funktion für sichere String-Kopierung von server.arg() in char-Array
// Kopiert server.arg() sicher in dest mit maxLen (inkl. Null-Terminator), gibt true bei Erfolg zurück
bool copyServerArgToBuffer(const String& src, char* dest, size_t maxLen) {
  if (maxLen == 0) return false;
  size_t len = min(src.length(), maxLen - 1);
  strncpy(dest, src.c_str(), len);
  dest[len] = '\0';
  return true;
}

// Helper-Funktionen für EEPROM-Operationen
// Initialisiert EEPROM mit Version und Magic Byte (falls noch nicht gesetzt)
void ensureEEPROMInitialized() {
  uint8_t storedVersion = EEPROM.read(EEPROM_VERSION_ADDR);
  uint8_t storedMagic = EEPROM.read(EEPROM_MAGIC_ADDR);
  if (storedVersion != EEPROM_VERSION || storedMagic != EEPROM_MAGIC) {
    EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION);
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  }
}

// Commit EEPROM-Änderungen mit Watchdog-Fütterung und Logging
void commitEEPROMWithWatchdog(const char* operationName) {
  strncpy(lastOperation, operationName, sizeof(lastOperation) - 1);
  lastOperation[sizeof(lastOperation) - 1] = '\0';
  debugLogJson(operationName, "EEPROM commit start", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
  
  // Watchdog vor blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
  EEPROM.commit();
  // Watchdog nach blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
  
  debugLogJson(operationName, "EEPROM commit end", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
}

// Schreibt einen C-String in EEPROM (maxLen inkl. Null-Terminator)
void writeStringToEEPROM(uint16_t addr, const char* str, uint16_t maxLen) {
  uint16_t len = strlen(str);
  if (len >= maxLen) {
    len = maxLen - 1; // Platz für Null-Terminator lassen
  }
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, 0); // Null-Terminator
}

// Liest einen C-String aus EEPROM in den bereitgestellten Buffer
// Gibt die Anzahl gelesener Zeichen zurück (ohne Null-Terminator)
uint16_t readStringFromEEPROM(uint16_t addr, char* buffer, uint16_t bufferSize) {
  if (bufferSize == 0) return 0;
  uint16_t i = 0;
  for (; i < bufferSize - 1; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0) break;
    buffer[i] = c;
  }
  buffer[i] = '\0'; // Null-Terminator sicherstellen
  return i;
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
  
  // Prüfe Magic Byte (Version kann unterschiedlich sein für Migration)
  if (magic != EEPROM_MAGIC) {
    return false;
  }
  
  // Prüfe Checksumme nur wenn Version >= 1 (ältere Versionen haben möglicherweise andere Checksumme)
  if (version >= 1) {
    uint16_t calculatedChecksum = calculateEEPROMChecksum();
    return (storedChecksum == calculatedChecksum);
  }
  
  return false;
}

void loadBrightnessFromStorage() {
  // Prüfe ob EEPROM initialisiert und gültig ist
  if (validateEEPROM()) {
    // EEPROM ist gültig, lade Daten
    uint16_t stored = DEFAULT_BRIGHTNESS;
    EEPROM.get(EEPROM_BRIGHTNESS_ADDR, stored);
    brightness = constrain(stored, 0, PWM_MAX);

    // Auto-Brightness-Einstellungen laden
    autoBrightnessEnabled = EEPROM.read(EEPROM_AUTO_BRIGHTNESS_ADDR) == 1;
    EEPROM.get(EEPROM_MIN_BRIGHTNESS_ADDR, minBrightness);
    EEPROM.get(EEPROM_MAX_BRIGHTNESS_ADDR, maxBrightness);
    EEPROM.get(EEPROM_SENSOR_MIN_ADDR, sensorMin);
    EEPROM.get(EEPROM_SENSOR_MAX_ADDR, sensorMax);

    // MQTT-Einstellungen laden
    mqttEnabled = EEPROM.read(EEPROM_MQTT_ENABLED_ADDR) == 1;
    readStringFromEEPROM(EEPROM_MQTT_SERVER_ADDR, mqttServer, sizeof(mqttServer));
    EEPROM.get(EEPROM_MQTT_PORT_ADDR, mqttPort);
    readStringFromEEPROM(EEPROM_MQTT_USER_ADDR, mqttUser, sizeof(mqttUser));
    readStringFromEEPROM(EEPROM_MQTT_PASSWORD_ADDR, mqttPassword, sizeof(mqttPassword));
    readStringFromEEPROM(EEPROM_MQTT_TOPIC_ADDR, mqttPresenceTopic, sizeof(mqttPresenceTopic));
    EEPROM.get(EEPROM_PRESENCE_TIMEOUT_ADDR, presenceTimeout);
    
    // NTP-Server laden
    char loadedNtp1[EEPROM_NTP_SERVER_LEN + 1];
    char loadedNtp2[EEPROM_NTP_SERVER_LEN + 1];
    readStringFromEEPROM(EEPROM_NTP_SERVER1_ADDR, loadedNtp1, sizeof(loadedNtp1));
    readStringFromEEPROM(EEPROM_NTP_SERVER2_ADDR, loadedNtp2, sizeof(loadedNtp2));
    // Validierung: Nur verwenden wenn String nicht leer ist und gültig aussieht (enthält Punkt für Domain)
    if (strlen(loadedNtp1) > 0 && strlen(loadedNtp1) < EEPROM_NTP_SERVER_LEN && strchr(loadedNtp1, '.') != nullptr) {
      strncpy(ntpServer1, loadedNtp1, sizeof(ntpServer1) - 1);
      ntpServer1[sizeof(ntpServer1) - 1] = '\0';
    }
    if (strlen(loadedNtp2) > 0 && strlen(loadedNtp2) < EEPROM_NTP_SERVER_LEN && strchr(loadedNtp2, '.') != nullptr) {
      strncpy(ntpServer2, loadedNtp2, sizeof(ntpServer2) - 1);
      ntpServer2[sizeof(ntpServer2) - 1] = '\0';
    }
    use24HourFormat = EEPROM.read(EEPROM_HOUR_FORMAT_ADDR) != 0;

    // Restart-Counter laden (nur wenn EEPROM_VERSION >= 2)
    uint8_t version = EEPROM.read(EEPROM_VERSION_ADDR);
    if (version >= 2) {
      EEPROM.get(EEPROM_RESTART_COUNT_ADDR, restartCount);
      readStringFromEEPROM(EEPROM_LAST_RESET_REASON_ADDR, lastResetReason, sizeof(lastResetReason));
    } else {
      // Migration von Version 1: Restart-Counter auf 0 setzen
      restartCount = 0;
      lastResetReason[0] = '\0';
    }

    // Log-Server-Einstellungen laden (nur wenn EEPROM_VERSION >= 3)
    if (version >= 3) {
      char loadedLogServerUrl[EEPROM_LOG_SERVER_URL_LEN + 1];
      readStringFromEEPROM(EEPROM_LOG_SERVER_URL_ADDR, loadedLogServerUrl, sizeof(loadedLogServerUrl));
      if (strlen(loadedLogServerUrl) > 0) {
        strncpy(logServerUrl, loadedLogServerUrl, sizeof(logServerUrl) - 1);
        logServerUrl[sizeof(logServerUrl) - 1] = '\0';
      }
      uint32_t loadedInterval = 0;
      EEPROM.get(EEPROM_LOG_SERVER_INTERVAL_ADDR, loadedInterval);
      if (loadedInterval > 0) {
        logServerInterval = loadedInterval;
      }
      
      // Uptime und Heap vor Restart laden
      EEPROM.get(EEPROM_LAST_UPTIME_ADDR, lastUptimeBeforeRestart);
      EEPROM.get(EEPROM_LAST_HEAP_BEFORE_RESTART_ADDR, lastHeapBeforeRestart);
    } else {
      // Migration von Version 2: Log-Server-Einstellungen aus secrets.h verwenden
      // (Variablen sind bereits mit Defaults initialisiert)
      lastUptimeBeforeRestart = 0;
      lastHeapBeforeRestart = 0;
    }

    // Werte validieren und korrigieren falls nötig
    minBrightness = constrain(minBrightness, 0, PWM_MAX);
    maxBrightness = constrain(maxBrightness, 0, PWM_MAX);
    sensorMin = constrain(sensorMin, 0, PWM_MAX);
    sensorMax = constrain(sensorMax, 0, PWM_MAX);
    if (mqttPort == 0 || mqttPort > 65535) mqttPort = MQTT_PORT_DEFAULT;
    if (presenceTimeout == 0) presenceTimeout = PRESENCE_TIMEOUT_DEFAULT;
    
    Serial.println("EEPROM data loaded and validated successfully");
  } else {
    // EEPROM ungültig oder nicht initialisiert, verwende Defaults
    Serial.println("EEPROM invalid or not initialized, using defaults");
    brightness = DEFAULT_BRIGHTNESS;
    autoBrightnessEnabled = false;
    minBrightness = MIN_BRIGHTNESS_DEFAULT;
    maxBrightness = MAX_BRIGHTNESS_DEFAULT;
    sensorMin = SENSOR_MIN_DEFAULT;
    sensorMax = SENSOR_MAX_DEFAULT;
    mqttEnabled = false;
    mqttServer[0] = '\0';
    mqttPort = MQTT_PORT_DEFAULT;
    mqttUser[0] = '\0';
    mqttPassword[0] = '\0';
    mqttPresenceTopic[0] = '\0';
    presenceTimeout = PRESENCE_TIMEOUT_DEFAULT;
    strncpy(ntpServer1, "pool.ntp.org", sizeof(ntpServer1) - 1);
    ntpServer1[sizeof(ntpServer1) - 1] = '\0';
    strncpy(ntpServer2, "time.nist.gov", sizeof(ntpServer2) - 1);
    ntpServer2[sizeof(ntpServer2) - 1] = '\0';
    use24HourFormat = true;
    restartCount = 0;
    lastResetReason[0] = '\0';
    lastUptimeBeforeRestart = 0;
    lastHeapBeforeRestart = 0;
    // Log-Server-Einstellungen bleiben bei Defaults aus secrets.h
  }
}

void persistBrightnessToStorage() {
  ensureEEPROMInitialized();
  
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
  
  // Commit mit Watchdog-Fütterung
  commitEEPROMWithWatchdog("persistBrightnessToStorage");
}

void persistMqttToStorage() {
  // Version und Magic Byte setzen (falls noch nicht gesetzt)
  EEPROM.write(EEPROM_VERSION_ADDR, EEPROM_VERSION);
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  
  // MQTT-Daten schreiben
  EEPROM.write(EEPROM_MQTT_ENABLED_ADDR, mqttEnabled ? 1 : 0);
  writeStringToEEPROM(EEPROM_MQTT_SERVER_ADDR, mqttServer, EEPROM_MQTT_SERVER_LEN + 1);
  EEPROM.put(EEPROM_MQTT_PORT_ADDR, mqttPort);
  writeStringToEEPROM(EEPROM_MQTT_USER_ADDR, mqttUser, EEPROM_MQTT_USER_LEN + 1);
  writeStringToEEPROM(EEPROM_MQTT_PASSWORD_ADDR, mqttPassword, EEPROM_MQTT_PASSWORD_LEN + 1);
  writeStringToEEPROM(EEPROM_MQTT_TOPIC_ADDR, mqttPresenceTopic, EEPROM_MQTT_TOPIC_LEN + 1);
  EEPROM.put(EEPROM_PRESENCE_TIMEOUT_ADDR, presenceTimeout);
  
  // Checksumme berechnen und speichern
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  // Kritische Operation: EEPROM.commit()
  strncpy(lastOperation, "persistMqttToStorage", sizeof(lastOperation) - 1);
  lastOperation[sizeof(lastOperation) - 1] = '\0';
  // Commit mit Watchdog-Fütterung
  commitEEPROMWithWatchdog("persistMqttToStorage");
}

// Speichert Restart-Counter und Reset-Grund in EEPROM
void persistRestartInfo() {
  EEPROM.put(EEPROM_RESTART_COUNT_ADDR, restartCount);
  writeStringToEEPROM(EEPROM_LAST_RESET_REASON_ADDR, lastResetReason, EEPROM_RESET_REASON_LEN + 1);
  
  // Checksumme neu berechnen und speichern (Restart-Info ist Teil der Checksumme)
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  // Watchdog vor blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
  EEPROM.commit();
  // Watchdog nach blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
}

// Speichert Log-Server-Einstellungen in EEPROM
void persistLogServerToStorage() {
  ensureEEPROMInitialized();
  
  // Log-Server-Daten schreiben
  writeStringToEEPROM(EEPROM_LOG_SERVER_URL_ADDR, logServerUrl, EEPROM_LOG_SERVER_URL_LEN + 1);
  EEPROM.put(EEPROM_LOG_SERVER_INTERVAL_ADDR, logServerInterval);
  
  // Checksumme neu berechnen und speichern
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  // Commit mit Watchdog-Fütterung
  commitEEPROMWithWatchdog("persistLogServerToStorage");
}

// Speichert Uptime und Heap-Status vor Restart in EEPROM
void persistUptimeHeapStatus() {
  lastUptimeBeforeRestart = millis();
  lastHeapBeforeRestart = ESP.getFreeHeap();
  
  EEPROM.put(EEPROM_LAST_UPTIME_ADDR, lastUptimeBeforeRestart);
  EEPROM.put(EEPROM_LAST_HEAP_BEFORE_RESTART_ADDR, lastHeapBeforeRestart);
  
  // Checksumme neu berechnen und speichern
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  
  // Watchdog vor blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
  EEPROM.commit();
  // Watchdog nach blockierender EEPROM-Operation füttern
  ESP.wdtFeed();
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
      analogWrite(PIN_ENABLE, PWM_MAX - brightness);
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
  if (strcmp(topic, mqttPresenceTopic) == 0) {
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
          analogWrite(PIN_ENABLE, PWM_MAX - brightness);
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
#ifdef DEBUG_LOGGING_ENABLED
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
#endif
  
  if (!mqttEnabled || strlen(mqttServer) == 0) {
    return false;
  }

  if (mqttClient.connected()) {
    return true;
  }

  Serial.printf("Attempting MQTT connection to %s:%d...\n", mqttServer, mqttPort);

  // Client-ID generieren (optimiert)
  char clientIdBuf[BUFFER_SIZE_CLIENT_ID];
  snprintf(clientIdBuf, sizeof(clientIdBuf), "IkeaClock-%x", ESP.getChipId());
  String clientId = String(clientIdBuf);

  // Socket-Timeout setzen (für non-blocking Connect)
  mqttClient.setSocketTimeout(MQTT_CONNECT_TIMEOUT / 1000); // in Sekunden
  
  bool connected = false;
  unsigned long connectStart = millis();
  
  // Kritische Operation: MQTT-Connect
  strncpy(lastOperation, "reconnectMQTT", sizeof(lastOperation) - 1);
  lastOperation[sizeof(lastOperation) - 1] = '\0';
  debugLogJson("reconnectMQTT", "MQTT connect start", "C", "{\"server\":\"%s\",\"port\":%d,\"freeHeap\":%d}", mqttServer, mqttPort, ESP.getFreeHeap());
  
  ESP.wdtFeed(); // Watchdog vor blockierender Operation füttern
  if (strlen(mqttUser) > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  ESP.wdtFeed(); // Watchdog nach blockierender Operation füttern
  
  debugLogJson("reconnectMQTT", connected ? "MQTT connect success" : "MQTT connect failed", "C", "{\"rc\":%d,\"duration\":%lu}", mqttClient.state(), millis() - connectStart);
  
  // Prüfe ob Connect zu lange gedauert hat (Fallback)
  unsigned long connectDuration = millis() - connectStart;
  if (connectDuration > MQTT_CONNECT_TIMEOUT) {
    Serial.printf("MQTT connect timeout after %lu ms\n", connectDuration);
    connected = false;
  }

#ifdef DEBUG_LOGGING_ENABLED
  unsigned long reconnectDuration = millis() - reconnectStart;
  int freeHeapAfter = ESP.getFreeHeap();
  int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
#endif
  
  if (connected) {
    Serial.println("MQTT connected!");
    // Backoff zurücksetzen bei erfolgreicher Verbindung
    mqttReconnectBackoff = 1000;
    // Topic abonnieren
    if (strlen(mqttPresenceTopic) > 0) {
      mqttClient.subscribe(mqttPresenceTopic);
      Serial.printf("Subscribed to topic: %s\n", mqttPresenceTopic);
    }
#ifdef DEBUG_LOGGING_ENABLED
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"mqtt_success_%lu\",\"timestamp\":%lu,\"location\":\"reconnectMQTT\",\"message\":\"MQTT reconnect success\",\"data\":{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), reconnectDuration, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
        logFile.close();
      }
    }
#endif
    return true;
  } else {
    Serial.printf("MQTT connection failed, rc=%d, next retry in %lu ms\n", mqttClient.state(), mqttReconnectBackoff);
    // Exponential Backoff erhöhen
    mqttReconnectBackoff *= MQTT_BACKOFF_MULTIPLIER;
    if (mqttReconnectBackoff > MQTT_MAX_BACKOFF) {
      mqttReconnectBackoff = MQTT_MAX_BACKOFF;
    }
#ifdef DEBUG_LOGGING_ENABLED
    if (SPIFFS.exists("/")) {
      File logFile = SPIFFS.open("/debug.log", "a");
      if (logFile) {
        logFile.printf("{\"id\":\"mqtt_failed_%lu\",\"timestamp\":%lu,\"location\":\"reconnectMQTT\",\"message\":\"MQTT reconnect failed\",\"data\":{\"duration\":%lu,\"rc\":%d,\"nextBackoff\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"maxFreeBlockBefore\":%d,\"maxFreeBlockAfter\":%d},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"C\"}\n",
                       millis(), millis(), reconnectDuration, mqttClient.state(), mqttReconnectBackoff, freeHeapBefore, freeHeapAfter, maxFreeBlockBefore, maxFreeBlockAfter);
        logFile.close();
      }
    }
#endif
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
  char json[BUFFER_SIZE_JSON_STATUS];
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
  char hostname[BUFFER_SIZE_HOSTNAME];
  snprintf(hostname, sizeof(hostname), "IkeaClock-%x", ESP.getChipId());
  
  // Formatiere Uptime und Heap vor Restart für Web-Interface
  uint16_t uptimeHours = 0;
  uint16_t uptimeMinutes = 0;
  uint16_t heapKB = 0;
  
  if (lastUptimeBeforeRestart > 0) {
    uptimeHours = lastUptimeBeforeRestart / 3600000; // Millisekunden zu Stunden
    uptimeMinutes = (lastUptimeBeforeRestart % 3600000) / 60000; // Rest zu Minuten
  }
  
  if (lastHeapBeforeRestart > 0) {
    heapKB = lastHeapBeforeRestart / 1024; // Bytes zu KB
  }
  
  // JSON-Response mit beiden Feldnamen (alte für Web-Interface, neue für HA-Integration)
  int jsonLen = snprintf(json, sizeof(json),
    "{\"time\":\"%s\",\"effect\":\"%s\",\"currentEffect\":\"%s\",\"tz\":\"%s\",\"timezone\":\"%s\",\"hourFormat\":\"%s\",\"use24HourFormat\":%s,\"brightness\":%d,"
    "\"autoBrightness\":%s,\"autoBrightnessEnabled\":%s,\"minBrightness\":%d,\"maxBrightness\":%d,"
    "\"autoBrightnessMin\":%d,\"autoBrightnessMax\":%d,\"sensorMin\":%d,\"sensorMax\":%d,"
    "\"autoBrightnessSensorMin\":%d,\"autoBrightnessSensorMax\":%d,\"sensorValue\":%d,"
    "\"mqttEnabled\":%s,\"mqttConnected\":%s,\"mqttServer\":\"%s\","
    "\"mqttPort\":%d,\"mqttTopic\":\"%s\",\"presenceDetected\":%s,\"presence\":%s,"
    "\"displayEnabled\":%s,\"haDisplayControlled\":%s,\"presenceTimeout\":%lu,"
    "\"availableEffects\":%s,"
    "\"otaEnabled\":%s,\"otaHostname\":\"%s\",\"ipAddress\":\"%s\","
    "\"firmwareVersion\":\"%s\",\"version\":\"%s\","
    "\"restartCount\":%lu,\"lastResetReason\":\"%s\","
    "\"logServerUrl\":\"%s\",\"logServerInterval\":%lu,"
    "\"lastUptimeBeforeRestart\":%lu,\"lastHeapBeforeRestart\":%u,"
    "\"lastUptimeBeforeRestartHours\":%u,\"lastUptimeBeforeRestartMinutes\":%u,\"lastHeapBeforeRestartKB\":%u}",
    buf, currentEffect->name, currentEffect->name, tzString, tzString, hourFormatStr, use24HourFormat ? "true" : "false", brightness,
    autoBrightnessEnabled ? "true" : "false", autoBrightnessEnabled ? "true" : "false", minBrightness, maxBrightness,
    minBrightness, maxBrightness, sensorMin, sensorMax,
    sensorMin, sensorMax, sensorValue,
    mqttEnabled ? "true" : "false", mqttClient.connected() ? "true" : "false",
    mqttServer, mqttPort, mqttPresenceTopic,
    presenceDetected ? "true" : "false", presenceDetected ? "true" : "false",
    displayEnabled ? "true" : "false",
    haDisplayControlled ? "true" : "false", presenceTimeout,
    effectList.c_str(),
    (WiFi.status() == WL_CONNECTED) ? "true" : "false", hostname, ipAddress.c_str(),
    FIRMWARE_VERSION, FIRMWARE_VERSION,
    restartCount, lastResetReason,
    logServerUrl, logServerInterval,
    lastUptimeBeforeRestart, lastHeapBeforeRestart,
    uptimeHours, uptimeMinutes, heapKB);
  
  // Prüfe ob snprintf erfolgreich war (Rückgabewert >= 0 und < sizeof(json))
  if (jsonLen < 0 || jsonLen >= (int)sizeof(json)) {
    // Buffer Overflow oder Fehler - sende Fehler-Response
    server.send(500, "application/json", "{\"error\":\"Internal server error: JSON generation failed\"}");
    return;
  }
  
  server.send(200, "application/json", json);
}

void handleSetTimezone() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("tz")) {
    String tzArg = server.arg("tz");
    size_t tzLen = min(tzArg.length(), (size_t)(INPUT_TZ_MAX - 1));
    strncpy(tzString, tzArg.c_str(), tzLen);
    tzString[tzLen] = '\0';
    setenv("TZ", tzString, 1);
    tzset();
    // Debug: Neue Zeit nach Zeitzone-Änderung
    time_t now = time(nullptr);
    struct tm *local = localtime(&now);
    if (local) {
      Serial.printf("Timezone changed to: %s, Local time: %02d:%02d:%02d\n",
                    tzString, local->tm_hour, local->tm_min, local->tm_sec);
    }
    char json[BUFFER_SIZE_JSON_SMALL];
    snprintf(json, sizeof(json), "{\"tz\":\"%s\"}", tzString);
    server.send(200, "application/json", json);
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

  char json[128];
  snprintf(json, sizeof(json), "{\"hourFormat\":\"%s\",\"use24HourFormat\":%s}",
           use24HourFormat ? "24h" : "12h", use24HourFormat ? "true" : "false");
  server.send(200, "application/json", json);
}

void handleSetBrightness() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("b")) {
    brightness = constrain(server.arg("b").toInt(), 0, PWM_MAX);
    analogWrite(PIN_ENABLE, PWM_MAX - brightness);

    // Auto-Brightness deaktivieren bei manueller Helligkeitsänderung
    if (autoBrightnessEnabled) {
      autoBrightnessEnabled = false;
      Serial.println("Auto-Brightness disabled due to manual brightness change");
    }

    persistBrightnessToStorage();
    char json[BUFFER_SIZE_JSON_SMALL / 2];
    snprintf(json, sizeof(json), "{\"brightness\":%d}", brightness);
    server.send(200, "application/json", json);
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
    minBrightness = constrain(server.arg("min").toInt(), 0, PWM_MAX);
  }
  if (server.hasArg("max")) {
    maxBrightness = constrain(server.arg("max").toInt(), 0, PWM_MAX);
  }
  if (server.hasArg("sensorMin")) {
    sensorMin = constrain(server.arg("sensorMin").toInt(), 0, PWM_MAX);
  }
  if (server.hasArg("sensorMax")) {
    sensorMax = constrain(server.arg("sensorMax").toInt(), 0, PWM_MAX);
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
    char json[BUFFER_SIZE_JSON_MEDIUM];
  snprintf(json, sizeof(json), "{\"autoBrightness\":%s,\"minBrightness\":%d,\"maxBrightness\":%d,\"sensorMin\":%d,\"sensorMax\":%d}",
           autoBrightnessEnabled ? "true" : "false", minBrightness, maxBrightness, sensorMin, sensorMax);
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
      analogWrite(PIN_ENABLE, PWM_MAX - brightness);
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
    String serverArg = server.arg("server");
    if (serverArg.length() > INPUT_MQTT_SERVER_MAX - 1) {
      server.send(400, "application/json", "{\"error\":\"MQTT server string too long\"}");
      return;
    }
    copyServerArgToBuffer(serverArg, mqttServer, sizeof(mqttServer));
  }
  if (server.hasArg("port")) {
    int port = server.arg("port").toInt();
    if (port < 1 || port > 65535) {
      server.send(400, "application/json", "{\"error\":\"Invalid port number\"}");
      return;
    }
    mqttPort = port;
  }
  if (server.hasArg("user")) {
    String userArg = server.arg("user");
    if (userArg.length() > INPUT_MQTT_USER_MAX - 1) {
      server.send(400, "application/json", "{\"error\":\"MQTT user string too long\"}");
      return;
    }
    copyServerArgToBuffer(userArg, mqttUser, sizeof(mqttUser));
  }
  if (server.hasArg("password")) {
    String passArg = server.arg("password");
    if (passArg.length() > INPUT_MQTT_PASSWORD_MAX - 1) {
      server.send(400, "application/json", "{\"error\":\"MQTT password string too long\"}");
      return;
    }
    copyServerArgToBuffer(passArg, mqttPassword, sizeof(mqttPassword));
  }
  if (server.hasArg("topic")) {
    String topicArg = server.arg("topic");
    if (topicArg.length() > INPUT_MQTT_TOPIC_MAX - 1) {
      server.send(400, "application/json", "{\"error\":\"MQTT topic string too long\"}");
      return;
    }
    copyServerArgToBuffer(topicArg, mqttPresenceTopic, sizeof(mqttPresenceTopic));
  }
  if (server.hasArg("timeout")) {
    presenceTimeout = constrain(server.arg("timeout").toInt(), 1000, 3600000); // 1s bis 1h
  }

  persistMqttToStorage();

  // MQTT neu konfigurieren wenn aktiviert
  if (mqttEnabled && strlen(mqttServer) > 0) {
    mqttClient.disconnect();
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    Serial.println("MQTT configuration updated, will reconnect...");
  }

    char json[BUFFER_SIZE_JSON_LARGE];
  snprintf(json, sizeof(json), "{\"mqttEnabled\":%s,\"mqttServer\":\"%s\",\"mqttPort\":%d,\"mqttTopic\":\"%s\",\"presenceTimeout\":%lu}",
           mqttEnabled ? "true" : "false", mqttServer, mqttPort, mqttPresenceTopic, presenceTimeout);
  server.send(200, "application/json", json);
}

void handleSetLogServer() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  // Log-Server-Konfiguration setzen
  if (server.hasArg("url")) {
    String url = server.arg("url");
    url.trim();
    
    // URL-Validierung: Muss mit http:// oder https:// beginnen oder leer sein (deaktiviert)
    if (url.length() > 0 && url.indexOf("http://") != 0 && url.indexOf("https://") != 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid URL. Must start with http:// or https://\"}");
      return;
    }
    
    if (url.length() > INPUT_LOG_URL_MAX - 1) {
      server.send(400, "application/json", "{\"error\":\"Log server URL too long\"}");
      return;
    }
    copyServerArgToBuffer(url, logServerUrl, sizeof(logServerUrl));
  }
  
  if (server.hasArg("interval")) {
    unsigned long interval = server.arg("interval").toInt();
    // Validierung: Mindestens 10 Sekunden, maximal 1 Stunde
    if (interval < 10000) interval = 10000;
    if (interval > 3600000) interval = 3600000;
    logServerInterval = interval;
  }
  
  // In EEPROM speichern
  persistLogServerToStorage();
  
    char json[BUFFER_SIZE_JSON_MEDIUM];
  snprintf(json, sizeof(json), "{\"logServerUrl\":\"%s\",\"logServerInterval\":%lu}", 
           logServerUrl, logServerInterval);
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
        analogWrite(PIN_ENABLE, PWM_MAX - brightness);
        Serial.printf("Display ENABLED via HA (brightness: %d)\n", brightness);
      }
    } else {
      // Display ausschalten (Helligkeit auf 0)
      analogWrite(PIN_ENABLE, PWM_MAX);
      Serial.println("Display DISABLED via HA");
    }
    
    Serial.println("MQTT presence control DISABLED (HA active)");
    
    char json[BUFFER_SIZE_JSON_SMALL];
    snprintf(json, sizeof(json), "{\"displayEnabled\":%s,\"haDisplayControlled\":true}",
             displayEnabled ? "true" : "false");
    server.send(200, "application/json", json);
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
  char json[128];
  snprintf(json, sizeof(json), "{\"effect\":\"%s\"}", currentEffect->name);
  server.send(200, "application/json", json);
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
  if (strlen(ntpServer1) > 0 && strlen(ntpServer1) < EEPROM_NTP_SERVER_LEN) {
    server1 = ntpServer1;
  } else {
    server1 = "pool.ntp.org";
    Serial.println("Warning: ntpServer1 invalid, using default");
  }
  
  if (strlen(ntpServer2) > 0 && strlen(ntpServer2) < EEPROM_NTP_SERVER_LEN) {
    server2 = ntpServer2;
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
  Serial.printf("Original strings - ntpServer1 length=%zu, ntpServer2 length=%zu\n", strlen(ntpServer1), strlen(ntpServer2));
  
  configTime(0, 0, ntp1, ntp2);
  // Zeitzone mit POSIX TZ String setzen - DST (Sommer-/Winterzeit) wird automatisch berücksichtigt
  setenv("TZ", tzString, 1);
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
                    local->tm_hour, local->tm_min, local->tm_sec, tzString);
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
  // Bei ESP8266/Wemos D1 Mini: SPIFFS.begin() mit true formatiert automatisch, wenn nötig
  Serial.println("Initialisiere SPIFFS...");
  bool spiffsOk = SPIFFS.begin();
  
  // SPIFFS-Status ausgeben (immer, nicht nur bei DEBUG_LOGGING_ENABLED)
  if (!spiffsOk) {
    Serial.println("FEHLER: SPIFFS konnte nicht initialisiert werden!");
    Serial.println("Versuche SPIFFS zu formatieren...");
    SPIFFS.format();
    delay(200); // Länger warten nach Formatierung
    spiffsOk = SPIFFS.begin();
    if (spiffsOk) {
      Serial.println("SPIFFS erfolgreich formatiert und initialisiert!");
    } else {
      Serial.println("FEHLER: SPIFFS konnte auch nach Formatierung nicht initialisiert werden!");
      Serial.println("HINWEIS: Prüfe in Arduino IDE: Tools -> Flash Size -> sollte mindestens 1MB sein");
    }
  }
  
  if (spiffsOk) {
    Serial.println("SPIFFS erfolgreich initialisiert");
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
      Serial.printf("SPIFFS: %d bytes total, %d bytes used, %d bytes free\n", 
                    fs_info.totalBytes, fs_info.usedBytes, 
                    fs_info.totalBytes - fs_info.usedBytes);
    } else {
      Serial.println("WARNUNG: SPIFFS.info() fehlgeschlagen");
    }
  } else {
    Serial.println("WARNUNG: SPIFFS ist nicht verfügbar - Debug-Logging wird nur über Serial funktionieren");
  }
  
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
  
  // Uptime und Heap vor Restart wurden bereits in loadBrightnessFromStorage() geladen
  // Diese Werte stammen vom letzten Lauf vor dem Restart
  
  // Restart-Counter erhöhen und aktuellen Reset-Grund speichern
  // (wird bereits in loadBrightnessFromStorage() geladen, falls vorhanden)
  restartCount++;
  String resetReasonStr = ESP.getResetReason();
  strncpy(lastResetReason, resetReasonStr.c_str(), sizeof(lastResetReason) - 1);
  lastResetReason[sizeof(lastResetReason) - 1] = '\0';
  persistRestartInfo();
  
  // Exception-Handler registrieren (falls ESP8266 unterstützt)
  // Dies hilft bei der Diagnose von Crashes
  // ESP8266 hat keinen direkten Exception-Handler, aber wir können Panic-Callbacks nutzen
  
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
    char hostname[BUFFER_SIZE_HOSTNAME];
    snprintf(hostname, sizeof(hostname), "IkeaClock-%x", ESP.getChipId());
    
    // ArduinoOTA Setup
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(otaPassword); // Passwort für OTA-Updates aus secrets.h
    
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_SPIFFS
        type = "filesystem";
      }
      Serial.println("Start updating " + type);
      // Display ausschalten während Update
      analogWrite(PIN_ENABLE, PWM_MAX);
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
    
    // WICHTIG: Restart-Logs sofort nach WiFi-Verbindung senden
    // Dies stellt sicher, dass Restart-Logs nicht verloren gehen, wenn das Gerät schnell neu startet
    if (strlen(logServerUrl) > 0 && SPIFFS.exists("/debug.log")) {
      File logFile = SPIFFS.open("/debug.log", "r");
      if (logFile && logFile.size() > 0) {
        // Prüfe ob Restart-Logs enthalten sind (schnelle Prüfung der ersten Zeile)
        String firstLine = logFile.readStringUntil('\n');
        logFile.close();
        
        if (firstLine.indexOf("restart_") >= 0) {
          Serial.println("[SETUP] Restart-Logs gefunden - sende sofort nach WiFi-Verbindung...");
          ESP.wdtFeed(); // Watchdog füttern vor blockierender Operation
          bool success = sendLogsToServer();
          if (success) {
            Serial.println("[SETUP] Restart-Logs erfolgreich gesendet");
            lastLogUpload = millis(); // Reset Intervall
          } else {
            Serial.println("[SETUP] Fehler beim Senden der Restart-Logs - werden später erneut versucht");
          }
          ESP.wdtFeed(); // Watchdog nach blockierender Operation
        }
      }
    }
  }

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/setTimezone", handleSetTimezone);
  server.on("/api/setClockFormat", handleSetClockFormat);
  server.on("/api/setBrightness", handleSetBrightness);
  server.on("/api/setAutoBrightness", handleSetAutoBrightness);
  server.on("/api/setMqtt", handleSetMqtt);
  server.on("/api/setDisplay", handleSetDisplay);  // HA-Integration Endpoint
  server.on("/api/setLogServer", handleSetLogServer);
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
    char json[BUFFER_SIZE_JSON_BACKUP];
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
      mqttEnabled ? "true" : "false", mqttServer, mqttPort,
      mqttUser, mqttPresenceTopic, presenceTimeout,
      tzString, use24HourFormat ? "24h" : "12h", use24HourFormat ? "true" : "false", ntpServer1, ntpServer2);
    
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
  if (mqttEnabled && strlen(mqttServer) > 0) {
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    Serial.printf("MQTT enabled, server: %s:%d, topic: %s\n",
                  mqttServer, mqttPort, mqttPresenceTopic);
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
  
#ifdef DEBUG_LOGGING_ENABLED
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
#else
  static unsigned long lastLoopStart = 0;
#endif
  
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
      
      // Kritische Operation: WiFi-Reconnect
      strncpy(lastOperation, "WiFi.reconnect", sizeof(lastOperation) - 1);
      lastOperation[sizeof(lastOperation) - 1] = '\0';
      debugLogJson("loop", "WiFi reconnect start", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
      
#ifdef DEBUG_LOGGING_ENABLED
      unsigned long wifiReconnectStart = millis();
      int freeHeapBefore = ESP.getFreeHeap();
      int maxFreeBlockBefore = ESP.getMaxFreeBlockSize();
#endif
      ESP.wdtFeed(); // Watchdog vor blockierender Operation füttern
      WiFi.reconnect();
      ESP.wdtFeed(); // Watchdog nach blockierender Operation füttern
      
#ifdef DEBUG_LOGGING_ENABLED
      unsigned long wifiReconnectDuration = millis() - wifiReconnectStart;
      int freeHeapAfter = ESP.getFreeHeap();
      int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
      debugLogJson("loop", "WiFi reconnect end", "C", "{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d,\"status\":%d}", 
                   wifiReconnectDuration, freeHeapBefore, freeHeapAfter, WiFi.status());
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
#endif
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
    // Kritische Operation: NTP-Setup
    strncpy(lastOperation, "setupNTP", sizeof(lastOperation) - 1);
    lastOperation[sizeof(lastOperation) - 1] = '\0';
    debugLogJson("loop", "NTP setup start", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
    
#ifdef DEBUG_LOGGING_ENABLED
    unsigned long ntpStart = millis();
    int freeHeapBefore = ESP.getFreeHeap();
    int maxFreeBlockBefore = ESP.getMaxFreeBlockSize();
#endif
    setupNTP();
#ifdef DEBUG_LOGGING_ENABLED
    unsigned long ntpDuration = millis() - ntpStart;
    int freeHeapAfter = ESP.getFreeHeap();
    int maxFreeBlockAfter = ESP.getMaxFreeBlockSize();
    debugLogJson("loop", "NTP setup end", "C", "{\"duration\":%lu,\"freeHeapBefore\":%d,\"freeHeapAfter\":%d}", 
                 ntpDuration, freeHeapBefore, freeHeapAfter);
#endif
    ntpConfigured = true;
  }

  // Uptime und Heap-Status alle 30 Sekunden in EEPROM speichern (nur wenn sich signifikant ändert)
  static unsigned long lastUptimeHeapSave = 0;
  if (timeDiff(millis(), lastUptimeHeapSave) >= 30000) {
    unsigned long currentUptime = millis();
    uint32_t currentHeap = ESP.getFreeHeap();
    
    // Speichern wenn:
    // 1. Beim ersten Mal (Werte noch 0) - immer speichern
    // 2. Uptime sich signifikant geändert hat (mehr als 1 Minute)
    // 3. Heap sich um mehr als 1KB geändert hat
    if (lastUptimeBeforeRestart == 0 || 
        abs((long)(currentUptime - lastUptimeBeforeRestart)) > 60000 || 
        abs((long)(currentHeap - lastHeapBeforeRestart)) > 1024) {
      persistUptimeHeapStatus();
    }
    lastUptimeHeapSave = millis();
  }

  // Automatisches Senden von Logs an Server (falls konfiguriert)
  if (WiFi.status() == WL_CONNECTED && strlen(logServerUrl) > 0) {
    // Sofort senden wenn WiFi gerade verbunden wurde und Restart-Logs vorhanden sind
    static bool wifiJustConnected = false;
    static unsigned long lastWifiStatusCheck = 0;
    static wl_status_t lastWifiStatus = WL_DISCONNECTED;
    
    wl_status_t currentWifiStatus = WiFi.status();
    unsigned long now = millis();
    
    // Prüfe ob WiFi gerade verbunden wurde (Status-Wechsel von nicht-verbunden zu verbunden)
    if (currentWifiStatus == WL_CONNECTED && lastWifiStatus != WL_CONNECTED) {
      wifiJustConnected = true;
      Serial.println("[LOOP] WiFi gerade verbunden - prüfe auf Restart-Logs...");
    }
    lastWifiStatus = currentWifiStatus;
    lastWifiStatusCheck = now;
    
    // Wenn WiFi gerade verbunden wurde, sofort Restart-Logs senden
    if (wifiJustConnected && SPIFFS.exists("/debug.log")) {
      File logFile = SPIFFS.open("/debug.log", "r");
      if (logFile && logFile.size() > 0) {
        // Prüfe ob Restart-Logs enthalten sind
        String firstLine = logFile.readStringUntil('\n');
        logFile.close();
        
        if (firstLine.indexOf("restart_") >= 0) {
          Serial.println("[LOOP] Restart-Logs gefunden - sende sofort nach WiFi-Verbindung...");
          strncpy(lastOperation, "sendLogsToServer", sizeof(lastOperation) - 1);
          lastOperation[sizeof(lastOperation) - 1] = '\0';
          debugLogJson("loop", "Log upload start (immediate after WiFi)", "C", "{\"url\":\"%s\",\"freeHeap\":%d}", logServerUrl, ESP.getFreeHeap());
          bool success = sendLogsToServer();
          if (success) {
            Serial.println("[LOOP] Restart-Logs erfolgreich gesendet");
            lastLogUpload = millis(); // Reset Intervall
          } else {
            Serial.println("[LOOP] Fehler beim Senden der Restart-Logs - werden später erneut versucht");
          }
          debugLogJson("loop", success ? "Log upload success (immediate)" : "Log upload failed (immediate)", "C", "{}");
          wifiJustConnected = false; // Reset Flag
        } else {
          wifiJustConnected = false; // Keine Restart-Logs, Flag zurücksetzen
        }
      } else {
        wifiJustConnected = false; // Keine Log-Datei, Flag zurücksetzen
      }
    }
    
    // Normales Intervall-basiertes Senden
    if (timeDiff(millis(), lastLogUpload) >= logServerInterval) {
      strncpy(lastOperation, "sendLogsToServer", sizeof(lastOperation) - 1);
      lastOperation[sizeof(lastOperation) - 1] = '\0';
      Serial.printf("[LOG_SERVER] Versuche Logs zu senden an: %s\n", logServerUrl);
      debugLogJson("loop", "Log upload start", "C", "{\"url\":\"%s\",\"freeHeap\":%d}", logServerUrl, ESP.getFreeHeap());
      bool success = sendLogsToServer();
      if (!success) {
        Serial.println("[LOG_SERVER] Senden fehlgeschlagen - siehe Details oben");
      }
      debugLogJson("loop", success ? "Log upload success" : "Log upload failed", "C", "{}");
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
#ifdef DEBUG_LOGGING_ENABLED
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
#endif
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
