#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <FS.h>
extern "C" {
#include <user_interface.h>
}

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
String mqttServer = "192.168.178.36" ;  // MQTT Broker IP (wird über Web-UI konfiguriert)
uint16_t mqttPort = 403;
String mqttUser = "";    // Optional
String mqttPassword = ""; // Optional
String mqttPresenceTopic = "Sonstige/Präsenz_Wz/Anwesenheit"; // Topic für Präsenzmelder (FP2 sendet JSON an Haupt-Topic)
bool mqttEnabled = false;
bool presenceDetected = false;
unsigned long lastPresenceTime = 0;
uint32_t presenceTimeout = 300000; // 5 Minuten in ms (Display bleibt 5 Min an nach letzter Erkennung)
bool displayEnabled = true; // Display-Status (wird durch Präsenz gesteuert)

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

const uint8_t EEPROM_MAGIC = 0xB8;  // Geändert von 0xB7 wegen neuem Layout
const uint16_t EEPROM_SIZE = 256;   // Erweitert für MQTT-Konfiguration

const uint16_t EEPROM_MAGIC_ADDR = 0;
const uint16_t EEPROM_BRIGHTNESS_ADDR = 1;
const uint16_t EEPROM_AUTO_BRIGHTNESS_ADDR = 3;    // bool (1 byte)
const uint16_t EEPROM_MIN_BRIGHTNESS_ADDR = 4;     // uint16_t (2 bytes)
const uint16_t EEPROM_MAX_BRIGHTNESS_ADDR = 6;     // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MIN_ADDR = 8;         // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MAX_ADDR = 10;        // uint16_t (2 bytes)
const uint16_t EEPROM_MQTT_ENABLED_ADDR = 12;      // bool (1 byte)
const uint16_t EEPROM_MQTT_SERVER_ADDR = 13;       // String (64 bytes)
const uint16_t EEPROM_MQTT_PORT_ADDR = 77;         // uint16_t (2 bytes)
const uint16_t EEPROM_MQTT_USER_ADDR = 79;         // String (32 bytes)
const uint16_t EEPROM_MQTT_PASSWORD_ADDR = 111;    // String (32 bytes)
const uint16_t EEPROM_MQTT_TOPIC_ADDR = 143;       // String (64 bytes)
const uint16_t EEPROM_PRESENCE_TIMEOUT_ADDR = 207; // uint32_t (4 bytes)

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
// Format: STD offset DST offset,start/time,end/time
// Beispiel: CET-1CEST,M3.5.0,M10.5.0/3
//   - CET = Central European Time (Standardzeit)
//   - -1 = UTC+1 (Offset zur UTC)
//   - CEST = Central European Summer Time (Sommerzeit)
//   - M3.5.0 = März (M3), 5. Woche, Sonntag (0) = letzter Sonntag im März um 02:00 UTC
//   - M10.5.0/3 = Oktober (M10), 5. Woche, Sonntag, um 03:00 UTC = letzter Sonntag im Oktober
String tzString = "CET-1CEST,M3.5.0,M10.5.0/3"; // default Europe/Berlin mit DST

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
  for (uint16_t i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0) break;
    str += c;
  }
  return str;
}

void loadBrightnessFromStorage() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC) {
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

    // Werte validieren
    minBrightness = constrain(minBrightness, 0, 1023);
    maxBrightness = constrain(maxBrightness, 0, 1023);
    sensorMin = constrain(sensorMin, 0, 1023);
    sensorMax = constrain(sensorMax, 0, 1023);
    if (mqttPort == 0 || mqttPort > 65535) mqttPort = 1883;
    if (presenceTimeout == 0) presenceTimeout = 300000;
  } else {
    brightness = DEFAULT_BRIGHTNESS;
    autoBrightnessEnabled = false;
    minBrightness = 100;
    maxBrightness = 1023;
    sensorMin = 5;
    sensorMax = 450;
    mqttEnabled = false;
    presenceTimeout = 300000;
  }
}

void persistBrightnessToStorage() {
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
  EEPROM.write(EEPROM_AUTO_BRIGHTNESS_ADDR, autoBrightnessEnabled ? 1 : 0);
  EEPROM.put(EEPROM_MIN_BRIGHTNESS_ADDR, minBrightness);
  EEPROM.put(EEPROM_MAX_BRIGHTNESS_ADDR, maxBrightness);
  EEPROM.put(EEPROM_SENSOR_MIN_ADDR, sensorMin);
  EEPROM.put(EEPROM_SENSOR_MAX_ADDR, sensorMax);
  EEPROM.commit();
}

void persistMqttToStorage() {
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.write(EEPROM_MQTT_ENABLED_ADDR, mqttEnabled ? 1 : 0);
  writeStringToEEPROM(EEPROM_MQTT_SERVER_ADDR, mqttServer, 64);
  EEPROM.put(EEPROM_MQTT_PORT_ADDR, mqttPort);
  writeStringToEEPROM(EEPROM_MQTT_USER_ADDR, mqttUser, 32);
  writeStringToEEPROM(EEPROM_MQTT_PASSWORD_ADDR, mqttPassword, 32);
  writeStringToEEPROM(EEPROM_MQTT_TOPIC_ADDR, mqttPresenceTopic, 64);
  EEPROM.put(EEPROM_PRESENCE_TIMEOUT_ADDR, presenceTimeout);
  EEPROM.commit();
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

    // #region agent log
    if (sensorSampleCount == 1 || sensorSampleCount == LIGHT_SENSOR_SAMPLES) { // Erste und letzte Messung loggen
      debugLogJson("processLightSensorSample", "Sensor sample", "D", "{\"sampleCount\":%d,\"reading\":%d,\"sum\":%lu}", sensorSampleCount, sensorReading, sensorSampleSum);
    }
    // #endregion

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
  // #region agent log
  float oldEma = emaSensorValue;
  // #endregion
  
  if (!emaInitialized) {
    emaSensorValue = newValue;
    emaInitialized = true;
    // #region agent log
    debugLogJson("applyEMA", "EMA initialized", "D", "{\"newValue\":%.2f}", newValue);
    // #endregion
    return newValue;
  }

  // EMA Formel: EMA_new = alpha * value + (1 - alpha) * EMA_old
  emaSensorValue = EMA_ALPHA_SENSOR * newValue + (1.0 - EMA_ALPHA_SENSOR) * emaSensorValue;
  
  // #region agent log
  float emaChange = abs(emaSensorValue - oldEma);
  if (emaChange > 10) { // Nur loggen bei größeren Änderungen
    debugLogJson("applyEMA", "EMA update", "D", "{\"newValue\":%.2f,\"oldEma\":%.2f,\"newEma\":%.2f,\"change\":%.2f,\"alpha\":%.2f}", newValue, oldEma, emaSensorValue, emaChange, EMA_ALPHA_SENSOR);
  }
  // #endregion
  
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
    // #region agent log
    debugLogJson("updateAutoBrightness", "Function entry", "D", "{\"enabled\":%d,\"displayEnabled\":%d}", autoBrightnessEnabled ? 1 : 0, displayEnabled ? 1 : 0);
    // #endregion
  
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
    // #region agent log
    debugLogJson("updateAutoBrightness", "Raw sensor value", "D", "{\"rawSensorValue\":%d}", rawSensorValue);
    // #endregion
    
    float smoothedSensorValue = applyEMA(rawSensorValue);
    // #region agent log
    debugLogJson("updateAutoBrightness", "EMA smoothed value", "D", "{\"smoothedSensorValue\":%.2f,\"emaSensorValue\":%.2f}", smoothedSensorValue, emaSensorValue);
    // #endregion

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

    // #region agent log
    int brightnessDiff = abs((int)newBrightness - (int)brightness);
    debugLogJson("updateAutoBrightness", "Brightness calculation", "D", "{\"newBrightness\":%d,\"currentBrightness\":%d,\"diff\":%d,\"threshold\":%d,\"atBoundary\":%d}", newBrightness, brightness, brightnessDiff, BRIGHTNESS_CHANGE_THRESHOLD, atBoundary ? 1 : 0);
    // #endregion

    // Zusätzliche Glättung der Helligkeitsänderungen (verhindert abrupte Sprünge)
    uint16_t smoothedBrightness = applyBrightnessEMA(newBrightness);
    
    // An Grenzen immer aktualisieren, sonst nur bei signifikanter Änderung (Hysterese)
    // Hysterese verhindert Flackern im mittleren Bereich, sollte aber nicht Min/Max blockieren
    // Verwende smoothedBrightness für Vergleich, aber aktualisiere mit smoothedBrightness für sanfte Übergänge
    int smoothedDiff = abs((int)smoothedBrightness - (int)brightness);
    
    if (atBoundary || smoothedDiff > BRIGHTNESS_CHANGE_THRESHOLD) {
      brightness = smoothedBrightness; // Verwende geglättete Helligkeit
      analogWrite(PIN_ENABLE, 1023 - brightness);
      // #region agent log
      debugLogJson("updateAutoBrightness", "Brightness updated", "D", "{\"brightness\":%d,\"rawSensorValue\":%d,\"smoothedSensorValue\":%.2f,\"newBrightness\":%d,\"smoothedBrightness\":%d,\"atBoundary\":%d}", brightness, rawSensorValue, smoothedSensorValue, newBrightness, smoothedBrightness, atBoundary ? 1 : 0);
      // #endregion
      Serial.printf("Auto-Brightness: Raw=%d, EMA=%.1f -> New=%d, Smoothed=%d%s\n",
                    rawSensorValue, smoothedSensorValue, newBrightness, brightness,
                    atBoundary ? " (boundary)" : "");
    } else {
      // #region agent log
      debugLogJson("updateAutoBrightness", "Brightness not updated (threshold)", "D", "{\"newBrightness\":%d,\"smoothedBrightness\":%d,\"currentBrightness\":%d,\"diff\":%d}", newBrightness, smoothedBrightness, brightness, smoothedDiff);
      // #endregion
    }
  }

  // Neues Sampling für den nächsten Zyklus starten
  startLightSensorSampling();
}

// MQTT Callback für eingehende Nachrichten
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Payload zu String konvertieren
  String message = "";
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
      if (!displayEnabled) {
        displayEnabled = true;
        Serial.println("Display ENABLED by presence detection");
      }
    }
  }
}

// MQTT Verbindung aufbauen/wiederherstellen
bool reconnectMQTT() {
    // #region agent log
    unsigned long reconnectStart = millis();
    debugLogJson("reconnectMQTT", "MQTT reconnect start", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
    // #endregion
  
  if (!mqttEnabled || mqttServer.length() == 0) {
    return false;
  }

  if (mqttClient.connected()) {
    return true;
  }

  Serial.printf("Attempting MQTT connection to %s:%d...\n", mqttServer.c_str(), mqttPort);

  // Client-ID generieren
  String clientId = "IkeaClock-" + String(ESP.getChipId(), HEX);

  bool connected = false;
  if (mqttUser.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str());
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  // #region agent log
  unsigned long reconnectDuration = millis() - reconnectStart;
  // #endregion
  
  if (connected) {
    Serial.println("MQTT connected!");
    // Topic abonnieren
    if (mqttPresenceTopic.length() > 0) {
      mqttClient.subscribe(mqttPresenceTopic.c_str());
      Serial.printf("Subscribed to topic: %s\n", mqttPresenceTopic.c_str());
    }
    // #region agent log
    debugLogJson("reconnectMQTT", "MQTT reconnect success", "C", "{\"duration\":%lu}", reconnectDuration);
    // #endregion
    return true;
  } else {
    Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
    // #region agent log
    debugLogJson("reconnectMQTT", "MQTT reconnect failed", "C", "{\"duration\":%lu,\"rc\":%d}", reconnectDuration, mqttClient.state());
    // #endregion
    return false;
  }
}

// Präsenz-Timeout prüfen und Display entsprechend steuern
void updatePresenceTimeout() {
  if (!mqttEnabled) {
    return;
  }

  // Wenn Präsenz erkannt: Display ist bereits an, nichts zu tun
  if (presenceDetected) {
    return;
  }

  // Keine Präsenz: Prüfe ob Timeout abgelaufen und Display noch an
  unsigned long timeSincePresence = millis() - lastPresenceTime;

  if (displayEnabled && timeSincePresence > presenceTimeout) {
    displayEnabled = false;
    Serial.printf("Display DISABLED after %lu ms timeout (no presence)\n", timeSincePresence);

    // Display ausschalten (Helligkeit auf 0)
    analogWrite(PIN_ENABLE, 1023);
  }
}

void handleRoot() {
  server.send_P(200, "text/html", WEB_INTERFACE_HTML);
}

void handleStatus() {
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buf[16];
  if (t) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  } else {
    strncpy(buf, "--:--:--", sizeof(buf));
  }
  uint16_t sensorValue = analogRead(LIGHT_SENSOR_PIN);
  String json = String("{\"time\":\"") + buf +
                "\",\"effect\":\"" + currentEffect->name +
                "\",\"tz\":\"" + tzString +
                "\",\"brightness\":" + String(brightness) +
                ",\"autoBrightness\":" + (autoBrightnessEnabled ? "true" : "false") +
                ",\"minBrightness\":" + String(minBrightness) +
                ",\"maxBrightness\":" + String(maxBrightness) +
                ",\"sensorMin\":" + String(sensorMin) +
                ",\"sensorMax\":" + String(sensorMax) +
                ",\"sensorValue\":" + String(sensorValue) +
                ",\"mqttEnabled\":" + (mqttEnabled ? "true" : "false") +
                ",\"mqttConnected\":" + (mqttClient.connected() ? "true" : "false") +
                ",\"mqttServer\":\"" + mqttServer + "\"" +
                ",\"mqttPort\":" + String(mqttPort) +
                ",\"mqttTopic\":\"" + mqttPresenceTopic + "\"" +
                ",\"presenceDetected\":" + (presenceDetected ? "true" : "false") +
                ",\"displayEnabled\":" + (displayEnabled ? "true" : "false") +
                ",\"presenceTimeout\":" + String(presenceTimeout) + "}";
  server.send(200, "application/json", json);
}

void handleSetTimezone() {
  if (server.hasArg("tz")) {
    tzString = server.arg("tz");
    setenv("TZ", tzString.c_str(), 1);
    tzset();
    server.send(200, "application/json", String("{\"tz\":\"") + tzString + "\"}" );
  } else {
    server.send(400, "text/plain", "Missing tz");
  }
}

void handleSetBrightness() {
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
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
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
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Starting up... Free heap: %d bytes\n", ESP.getFreeHeap());
  system_set_os_print(1); // Debug-Ausgaben aktivieren

  // FIX: Watchdog sofort füttern (verhindert Neustarts während Setup)
  ESP.wdtFeed();

  // #region agent log
  SPIFFS.begin();
  SPIFFS.remove("/debug.log"); // Alte Logs löschen
  debugLogJson("setup", "System startup", "A", "{\"freeHeap\":%d}", ESP.getFreeHeap());
  // #endregion

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
  }

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/setTimezone", handleSetTimezone);
  server.on("/api/setBrightness", handleSetBrightness);
  server.on("/api/setAutoBrightness", handleSetAutoBrightness);
  server.on("/api/setMqtt", handleSetMqtt);
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
    File logFile = SPIFFS.open("/debug.log", "r");
    if (logFile) {
      server.streamFile(logFile, "application/x-ndjson");
      logFile.close();
    } else {
      server.send(404, "text/plain", "Debug log not found");
    }
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
  
  // FIX: Watchdog sofort füttern zu Beginn jedes Loops (verhindert Neustarts)
  ESP.wdtFeed();
  
  // FIX: lastWatchdogFeed beim ersten Loop-Durchlauf initialisieren
  unsigned long now = millis();
  if (lastWatchdogFeed == 0) {
    lastWatchdogFeed = now;
  }

  // #region agent log
  loopCount++;
  if (loopCount % 100 == 0) { // Jeden 100. Loop
    unsigned long timeSinceWatchdog = now - lastWatchdogFeed;
    int freeHeap = ESP.getFreeHeap();
    debugLogJson("loop", "Loop iteration", "A", "{\"loopCount\":%lu,\"timeSinceWatchdog\":%lu,\"freeHeap\":%d}", loopCount, timeSinceWatchdog, freeHeap);
  }
  // #endregion

  server.handleClient();
  yield();

  // MQTT Loop (muss regelmäßig aufgerufen werden)
  if (mqttEnabled) {
    mqttClient.loop();
  }

  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      WiFi.reconnect();
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
    debugLogJson("loop", "NTP setup start", "C", "{\"freeHeap\":%d}", ESP.getFreeHeap());
    // #endregion
    setupNTP();
    // #region agent log
    unsigned long ntpDuration = millis() - ntpStart;
    debugLogJson("loop", "NTP setup end", "C", "{\"duration\":%lu}", ntpDuration);
    // #endregion
    ntpConfigured = true;
  }

  // MQTT Reconnection (alle 10 Sekunden versuchen falls nicht verbunden)
  if (mqttEnabled && millis() - lastMqttReconnect > 10000) {
    if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
    }
    lastMqttReconnect = millis();
  }

  // Präsenz-Timeout prüfen (alle 2 Sekunden)
  if (mqttEnabled && millis() - lastPresenceCheck > 2000) {
    updatePresenceTimeout();
    lastPresenceCheck = millis();
  }

  if (millis() - lastButtonCheck > 50) {
    static unsigned long lastPress = 0;
    if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > 300) {
      nextEffect();
      lastPress = millis();
    }
    lastButtonCheck = millis();
  }

  // Frame nur zeichnen wenn Display aktiviert ist
  if (millis() - lastFrameUpdate > 50) {
    if (displayEnabled) {
      // #region agent log
      unsigned long frameStart = millis();
      // #endregion
      uint8_t frame[32];
      clearFrame(frame, sizeof(frame));
      currentEffect->draw(frame);
      shiftOutBuffer(frame, sizeof(frame));
      // #region agent log
      unsigned long frameDuration = millis() - frameStart;
      if (frameDuration > 30) { // Nur loggen wenn langsam
        debugLogJson("loop", "Frame draw slow", "B", "{\"duration\":%lu}", frameDuration);
      }
      // #endregion
    }
    lastFrameUpdate = millis();
  }

  if (millis() - lastStatusPrint > 60000) {
    Serial.printf("Uptime: %lus, Free heap: %d bytes, Display: %s, Presence: %s\n",
                  millis() / 1000, ESP.getFreeHeap(),
                  displayEnabled ? "ON" : "OFF",
                  presenceDetected ? "DETECTED" : "NOT DETECTED");
    lastStatusPrint = millis();
  }

  // VERBESSERT: Auto-Brightness mit non-blocking Sampling
  // Wird jetzt in jedem Loop-Durchlauf verarbeitet statt blockierend
  if (autoBrightnessEnabled && displayEnabled) {
    // processLightSensorSample() ist non-blocking und gibt false zurück solange sampling läuft
    processLightSensorSample();
  }

  // Nur einen neuen Sample-Zyklus starten wenn der vorherige abgeschlossen ist
  if (millis() - lastBrightnessUpdate > AUTO_BRIGHTNESS_UPDATE_INTERVAL) {
    updateAutoBrightness();
    lastBrightnessUpdate = millis();
  }

  yield();
  delay(1);
  
  // #region agent log
  unsigned long timeSinceWatchdog = now - lastWatchdogFeed;
  if (timeSinceWatchdog > 5000) { // Warnung wenn länger als 5s
    debugLogJson("loop", "Watchdog feed delayed", "A", "{\"timeSinceWatchdog\":%lu}", timeSinceWatchdog);
  }
  lastWatchdogFeed = now;
  // #endregion
}

// #region agent log
// Debug-Logging Funktion
void debugLog(const char* location, const char* message, const char* hypothesisId, const char* dataJson) {
  unsigned long now = millis();
  int freeHeap = ESP.getFreeHeap();
  
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
// #endregion

void ICACHE_RAM_ATTR watchdogCallback() {
  Serial.println("WATCHDOG TRIGGERED!");
  // #region agent log
  // Watchdog-Callback kann nicht auf SPIFFS zugreifen, daher nur Serial
  // #endregion
}
