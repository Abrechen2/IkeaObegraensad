# WiFi-Reconnect, Restore & LocalSensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement robust WiFi reconnect with backoff, a fully working `/api/restore` endpoint, and compile-time local sensor support (BME280/DHT22) that writes directly into the SensorClock globals.

**Architecture:** All three features touch only `IkeaObegraensad.ino` plus a new `LocalSensor.h`. Each feature is independent — implement and commit them in order. No new library dependencies for F1/F2. F3 requires Adafruit BME280 or DHT sensor library only when the corresponding `#define` is uncommented.

**Tech Stack:** Arduino C++11, ESP8266 (Wemos D1 Mini), Arduino IDE / arduino-cli. No ArduinoJson.

**Spec:** `docs/superpowers/specs/2026-04-10-wifi-restore-sensor-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `IkeaObegraensad.ino` | Modify | WiFi backoff vars (line ~97), WiFi block replacement (lines 2190–2229), `persistNtpToStorage()` (after line 585), JSON helpers + restore body (before/at lines 2034–2082), sensor config comment (line ~23), LocalSensor calls in setup/loop, status JSON field |
| `LocalSensor.h` | Create | Compile-time sensor abstraction (BME280 / DHT22 / no-op) |

---

## Build Check Command

After each task, verify compilation:

**Arduino IDE:** Sketch → Verify/Compile (Ctrl+R)

**arduino-cli (if installed):**
```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini "D:/Projekte/Arduino/IkeaObegraensad"
```
Expected: `Used N bytes (M%) of program storage space.` — no errors.

---

## Task 1: WiFi-Reconnect with Exponential Backoff

**Files:**
- Modify: `IkeaObegraensad.ino` — add 5 globals (~line 97), replace WiFi-check block (lines 2190–2229)

### Step 1.1 — Add global variables after the MQTT backoff block

Find the MQTT backoff block (lines 91–96):
```cpp
// MQTT Reconnect mit Exponential Backoff
unsigned long mqttReconnectBackoff = 1000; // Start mit 1 Sekunde
const unsigned long MQTT_MAX_BACKOFF = 60000; // Maximal 60 Sekunden
const unsigned long MQTT_BACKOFF_MULTIPLIER = 2; // Verdoppeln bei jedem Fehlschlag

const unsigned long MQTT_CONNECT_TIMEOUT = 5000; // 5 Sekunden Timeout für Connect-Versuch
```

- [ ] After line 96 (the `MQTT_CONNECT_TIMEOUT` line), add:

```cpp

// WiFi Reconnect mit Exponential Backoff
unsigned long wifiReconnectBackoff = 5000;              // Start: 5 Sekunden
const unsigned long WIFI_RECONNECT_MAX_BACKOFF = 60000; // Maximum: 60 Sekunden
const unsigned long WIFI_RECONNECT_VERIFY_TIMEOUT = 8000; // Timeout für Verbindungsaufbau
bool wifiReconnecting = false;
unsigned long wifiReconnectStartMs = 0;
```

### Step 1.2 — Replace the WiFi-check block in loop()

- [ ] Find this entire block in `loop()` (lines 2190–2229):

```cpp
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
```

Replace with:

```cpp
  {
    unsigned long wifiCheckInterval = wifiReconnecting
      ? 500UL
      : (WiFi.status() == WL_CONNECTED ? 30000UL : wifiReconnectBackoff);

    if (timeDiff(millis(), lastWiFiCheck) > wifiCheckInterval) {
      wl_status_t wifiStatus = WiFi.status();

      if (wifiReconnecting) {
        if (wifiStatus == WL_CONNECTED) {
          wifiReconnecting = false;
          wifiReconnectBackoff = 5000;
          Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
          ntpConfigured = false;
        } else if (timeDiff(millis(), wifiReconnectStartMs) > WIFI_RECONNECT_VERIFY_TIMEOUT) {
          wifiReconnecting = false;
          wifiReconnectBackoff = min(wifiReconnectBackoff * 2UL, WIFI_RECONNECT_MAX_BACKOFF);
          Serial.printf("[WiFi] Reconnect timed out, next attempt in %lus\n",
                        wifiReconnectBackoff / 1000UL);
        }
      } else if (wifiStatus != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost, starting reconnect...");
        strncpy(lastOperation, "WiFi.begin", sizeof(lastOperation) - 1);
        lastOperation[sizeof(lastOperation) - 1] = '\0';
        serverStarted = false;
        ntpConfigured = false;
        ESP.wdtFeed();
        WiFi.begin(ssid, password);
        wifiReconnecting = true;
        wifiReconnectStartMs = millis();
      }

      lastWiFiCheck = millis();
    }
  }
```

### Step 1.3 — Build check

- [ ] Compile. Expected: no errors.

### Step 1.4 — Manual test

- [ ] Flash to device. Open Serial Monitor (115200 baud).
- [ ] Disable the WiFi router or block the SSID.
- [ ] Expected Serial output sequence:
  ```
  [WiFi] Connection lost, starting reconnect...
  [WiFi] Reconnect timed out, next attempt in 10s
  [WiFi] Reconnect timed out, next attempt in 20s
  [WiFi] Reconnect timed out, next attempt in 40s
  [WiFi] Reconnect timed out, next attempt in 60s
  [WiFi] Reconnect timed out, next attempt in 60s   ← stays at 60s
  ```
- [ ] Re-enable WiFi. Expected:
  ```
  [WiFi] Reconnected! IP: 192.168.x.x
  ```
  Display keeps running during the whole process (no freeze).

### Step 1.5 — Commit

- [ ] 
```bash
cd "D:/Projekte/Arduino/IkeaObegraensad"
git add IkeaObegraensad.ino
git commit -m "feat: WiFi reconnect with exponential backoff (non-blocking)"
```

---

## Task 2: persistNtpToStorage()

**Files:**
- Modify: `IkeaObegraensad.ino` — add function after line 585 (end of `persistUptimeHeapStatus`)

### Step 2.1 — Add function

- [ ] Find the closing `}` of `persistUptimeHeapStatus()` (line 585). After it, insert:

```cpp

void persistNtpToStorage() {
  ensureEEPROMInitialized();
  writeStringToEEPROM(EEPROM_NTP_SERVER1_ADDR, ntpServer1, EEPROM_NTP_SERVER_LEN + 1);
  writeStringToEEPROM(EEPROM_NTP_SERVER2_ADDR, ntpServer2, EEPROM_NTP_SERVER_LEN + 1);
  uint16_t checksum = calculateEEPROMChecksum();
  EEPROM.put(EEPROM_CHECKSUM_ADDR, checksum);
  commitEEPROMWithWatchdog("persistNtpToStorage");
}
```

### Step 2.2 — Build check

- [ ] Compile. Expected: no errors.

### Step 2.3 — Commit

- [ ] 
```bash
git add IkeaObegraensad.ino
git commit -m "feat: add persistNtpToStorage() for EEPROM NTP server persistence"
```

---

## Task 3: JSON Helpers + Full handleRestore()

**Files:**
- Modify: `IkeaObegraensad.ino` — 3 static helpers before `setup()` (~line 1773), replace restore stub body

### Step 3.1 — Add JSON helper functions before setup()

- [ ] Find `void setup() {` (line 1773). Directly before it, insert:

```cpp
// JSON-Parsing Helpers für handleRestore() — kein ArduinoJson nötig
static int extractJsonInt(const String& json, const char* key, int defaultVal) {
  String search = String("\"") + key + "\":";
  int pos = json.indexOf(search);
  if (pos < 0) return defaultVal;
  int start = pos + search.length();
  while (start < (int)json.length() && json[start] == ' ') start++;
  int end = start;
  if (end < (int)json.length() && json[end] == '-') end++;
  while (end < (int)json.length() && isDigit(json[end])) end++;
  if (end == start) return defaultVal;
  return json.substring(start, end).toInt();
}

static bool extractJsonBool(const String& json, const char* key, bool defaultVal) {
  String search = String("\"") + key + "\":";
  int pos = json.indexOf(search);
  if (pos < 0) return defaultVal;
  int start = pos + search.length();
  while (start < (int)json.length() && json[start] == ' ') start++;
  if (json.substring(start, start + 4) == "true")  return true;
  if (json.substring(start, start + 5) == "false") return false;
  return defaultVal;
}

static bool extractJsonStr(const String& json, const char* key, char* buf, size_t bufSize) {
  String search = String("\"") + key + "\":\"";
  int pos = json.indexOf(search);
  if (pos < 0) return false;
  int start = pos + search.length();
  int end = json.indexOf('"', start);
  if (end < 0) return false;
  int len = min((int)(end - start), (int)(bufSize - 1));
  json.substring(start, start + len).toCharArray(buf, bufSize);
  buf[bufSize - 1] = '\0';
  return true;
}

```

### Step 3.2 — Replace handleRestore() stub body

- [ ] Inside `setup()`, find the stub block inside the `/api/restore` handler — starting at:
```cpp
    // Parse einzelne Werte (vereinfachte Implementierung)
    // In Produktion sollte eine JSON-Bibliothek verwendet werden
    // Hier nur grundlegende Validierung und Warnung
    Serial.println("Restore request received (basic validation only)");
    Serial.printf("Backup data length: %d\n", jsonData.length());
    
    // Für vollständige Implementierung: JSON-Bibliothek verwenden
    // Hier nur Bestätigung senden
    server.send(200, "application/json", "{\"status\":\"restore_not_fully_implemented\",\"message\":\"Basic validation passed. Full restore requires JSON library.\"}");
```

Replace that block with:

```cpp
    // 1. Felder extrahieren
    int  newBrightness     = extractJsonInt (jsonData, "brightness",      brightness);
    bool newAutoBrightness = extractJsonBool(jsonData, "autoBrightness",  autoBrightnessEnabled);
    int  newMinBrightness  = extractJsonInt (jsonData, "minBrightness",   minBrightness);
    int  newMaxBrightness  = extractJsonInt (jsonData, "maxBrightness",   maxBrightness);
    int  newSensorMin      = extractJsonInt (jsonData, "sensorMin",       sensorMin);
    int  newSensorMax      = extractJsonInt (jsonData, "sensorMax",       sensorMax);
    bool newMqttEnabled    = extractJsonBool(jsonData, "mqttEnabled",     mqttEnabled);
    bool newUse24h         = extractJsonBool(jsonData, "use24HourFormat", use24HourFormat);
    int  newMqttPort       = extractJsonInt (jsonData, "mqttPort",        mqttPort);

    char newMqttServer[INPUT_MQTT_SERVER_MAX] = "";
    char newMqttUser  [INPUT_MQTT_USER_MAX]   = "";
    char newMqttTopic [INPUT_MQTT_TOPIC_MAX]  = "";
    char newTz        [INPUT_TZ_MAX]          = "";
    char newNtp1      [INPUT_NTP_SERVER_MAX]  = "";
    char newNtp2      [INPUT_NTP_SERVER_MAX]  = "";
    extractJsonStr(jsonData, "mqttServer",    newMqttServer, sizeof(newMqttServer));
    extractJsonStr(jsonData, "mqttUser",      newMqttUser,   sizeof(newMqttUser));
    extractJsonStr(jsonData, "mqttBaseTopic", newMqttTopic,  sizeof(newMqttTopic));
    extractJsonStr(jsonData, "tz",            newTz,         sizeof(newTz));
    extractJsonStr(jsonData, "ntpServer1",    newNtp1,       sizeof(newNtp1));
    extractJsonStr(jsonData, "ntpServer2",    newNtp2,       sizeof(newNtp2));

    // 2. Validieren
    newBrightness    = constrain(newBrightness,    0, (int)PWM_MAX);
    newMinBrightness = constrain(newMinBrightness, 0, (int)PWM_MAX);
    newMaxBrightness = constrain(newMaxBrightness, 0, (int)PWM_MAX);
    newSensorMin     = constrain(newSensorMin,     0, (int)PWM_MAX);
    newSensorMax     = constrain(newSensorMax,     0, (int)PWM_MAX);
    newMqttPort      = (newMqttPort > 0 && newMqttPort <= 65535) ? newMqttPort : (int)MQTT_PORT_DEFAULT;

    if (strlen(newMqttTopic) > 0 && !isValidMqttBaseTopic(newMqttTopic)) {
      server.send(400, "application/json", "{\"error\":\"Invalid MQTT topic in backup\"}");
      return;
    }
    if (strlen(newTz) > 0 && !isValidTzString(newTz)) {
      server.send(400, "application/json", "{\"error\":\"Invalid TZ string in backup\"}");
      return;
    }

    // 3. Globals setzen
    brightness            = (uint16_t)newBrightness;
    autoBrightnessEnabled = newAutoBrightness;
    minBrightness         = (uint16_t)newMinBrightness;
    maxBrightness         = (uint16_t)newMaxBrightness;
    sensorMin             = (uint16_t)newSensorMin;
    sensorMax             = (uint16_t)newSensorMax;
    mqttEnabled           = newMqttEnabled;
    use24HourFormat       = newUse24h;
    mqttPort              = (uint16_t)newMqttPort;
    if (strlen(newMqttServer) > 0) { strncpy(mqttServer,   newMqttServer, sizeof(mqttServer)   - 1); mqttServer  [sizeof(mqttServer)   - 1] = '\0'; }
    if (strlen(newMqttUser)   > 0) { strncpy(mqttUser,     newMqttUser,   sizeof(mqttUser)     - 1); mqttUser    [sizeof(mqttUser)     - 1] = '\0'; }
    if (strlen(newMqttTopic)  > 0) { strncpy(mqttBaseTopic,newMqttTopic,  sizeof(mqttBaseTopic)- 1); mqttBaseTopic[sizeof(mqttBaseTopic)- 1] = '\0'; }
    if (strlen(newTz)         > 0) { strncpy(tzString,     newTz,         sizeof(tzString)     - 1); tzString    [sizeof(tzString)     - 1] = '\0'; }
    if (strlen(newNtp1) > 0 && strchr(newNtp1, '.') != nullptr) { strncpy(ntpServer1, newNtp1, sizeof(ntpServer1) - 1); ntpServer1[sizeof(ntpServer1) - 1] = '\0'; }
    if (strlen(newNtp2) > 0 && strchr(newNtp2, '.') != nullptr) { strncpy(ntpServer2, newNtp2, sizeof(ntpServer2) - 1); ntpServer2[sizeof(ntpServer2) - 1] = '\0'; }

    // 4. Persistieren
    persistBrightnessToStorage();
    persistMqttToStorage();
    persistNtpToStorage();

    // 5. Runtime anwenden
    analogWrite(PIN_ENABLE, PWM_MAX - brightness);
    setupTimezone();
    ntpConfigured = false;
    if (mqttEnabled && strlen(mqttServer) > 0) {
      mqttClient.disconnect();
      mqttClient.setServer(mqttServer, mqttPort);
      mqttClient.setCallback(mqttCallback);
    }

    // 6. Antworten und neu starten
    server.send(200, "application/json",
      "{\"status\":\"ok\",\"message\":\"Settings restored, rebooting...\"}");
    delay(500);
    ESP.restart();
```

### Step 3.3 — Build check

- [ ] Compile. Expected: no errors.

### Step 3.4 — Manual test

- [ ] In a browser or curl, call `GET /api/backup`. Save the JSON.
- [ ] Change a setting in the WebUI (e.g., brightness to 200).
- [ ] Call `POST /api/restore` with the saved JSON as body (Content-Type: application/json).
- [ ] Expected: device reboots, brightness is back to the backed-up value.
- [ ] Verify via `GET /api/status` that all fields match the backup.
- [ ] Test invalid input: send backup with `"tz":"INVALID!@#"` → expected: `400 Invalid TZ string in backup`.

### Step 3.5 — Commit

- [ ] 
```bash
git add IkeaObegraensad.ino
git commit -m "feat: implement full /api/restore with JSON helpers and persistNtpToStorage"
```

---

## Task 4: LocalSensor.h + Integration

**Files:**
- Create: `LocalSensor.h`
- Modify: `IkeaObegraensad.ino` — sensor config comment, include, begin/update calls, status field

### Step 4.1 — Create LocalSensor.h

- [ ] Create `D:/Projekte/Arduino/IkeaObegraensad/LocalSensor.h`:

```cpp
// LocalSensor.h — Compile-time sensor selection for SensorClock
//
// In IkeaObegraensad.ino, uncomment ONE of:
//   #define LOCAL_SENSOR_BME280       // BME280 via I2C  (SDA=D2/GPIO4, SCL=D1/GPIO5)
//   #define LOCAL_SENSOR_DHT22        // DHT22 single-wire (default pin: D5/GPIO14)
//   #define LOCAL_SENSOR_DHT_PIN 14   // Optional: override DHT22 pin
//
// When no #define is active all functions compile to empty inlines — zero overhead.
//
// NOTE: Include this file from exactly one translation unit (IkeaObegraensad.ino only).
#ifndef LOCAL_SENSOR_H
#define LOCAL_SENSOR_H

#if defined(LOCAL_SENSOR_BME280)
  #include <Wire.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
  #define _LOCAL_SENSOR_ACTIVE 1
  #define LOCAL_SENSOR_NAME "bme280"
#elif defined(LOCAL_SENSOR_DHT22)
  #include <DHT.h>
  #ifndef LOCAL_SENSOR_DHT_PIN
    #define LOCAL_SENSOR_DHT_PIN 14  // D5 on Wemos D1 Mini
  #endif
  #define _LOCAL_SENSOR_ACTIVE 1
  #define LOCAL_SENSOR_NAME "dht22"
#else
  #define _LOCAL_SENSOR_ACTIVE 0
  #define LOCAL_SENSOR_NAME "none"
#endif

// Globals written by this module — defined in IkeaObegraensad.ino
extern float g_sensorTemp;
extern float g_sensorHumi;

namespace LocalSensor {

#if _LOCAL_SENSOR_ACTIVE

  #if defined(LOCAL_SENSOR_BME280)
    static Adafruit_BME280 _bme;
  #elif defined(LOCAL_SENSOR_DHT22)
    static DHT _dht(LOCAL_SENSOR_DHT_PIN, DHT22);
  #endif
  static bool _available = false;

  inline void begin() {
    #if defined(LOCAL_SENSOR_BME280)
      Wire.begin();
      _available = _bme.begin(0x76);
      if (!_available) _available = _bme.begin(0x77);
      Serial.printf("[LocalSensor] BME280 %s\n", _available ? "found" : "not found");
    #elif defined(LOCAL_SENSOR_DHT22)
      _dht.begin();
      _available = true;
      Serial.printf("[LocalSensor] DHT22 on pin %d\n", LOCAL_SENSOR_DHT_PIN);
    #endif
  }

  // Non-blocking: reads every 10 s, writes g_sensorTemp / g_sensorHumi on success.
  inline void update() {
    if (!_available) return;
    static unsigned long _lastRead = 0;
    if (millis() - _lastRead < 10000UL) return;
    _lastRead = millis();

    #if defined(LOCAL_SENSOR_BME280)
      float t = _bme.readTemperature();
      float h = _bme.readHumidity();
    #elif defined(LOCAL_SENSOR_DHT22)
      float t = _dht.readTemperature();
      float h = _dht.readHumidity();
    #endif

    if (!isnan(t)) g_sensorTemp = t;
    if (!isnan(h)) g_sensorHumi = h;
    Serial.printf("[LocalSensor] T=%.1f H=%.1f\n", g_sensorTemp, g_sensorHumi);
  }

  inline bool isAvailable() { return _available; }

#else  // no sensor configured — everything is a no-op

  inline void begin()           {}
  inline void update()          {}
  inline bool isAvailable()     { return false; }

#endif  // _LOCAL_SENSOR_ACTIVE

}  // namespace LocalSensor

#endif  // LOCAL_SENSOR_H
```

### Step 4.2 — Add sensor config comment + include in IkeaObegraensad.ino

- [ ] After line 22 (`#define FIRMWARE_VERSION "1.6.2"`), add:

```cpp

// === Lokaler Sensor ===
// Eine Zeile einkommentieren, oder beide auskommentiert lassen (MQTT-only):
// #define LOCAL_SENSOR_BME280           // BME280 via I2C (SDA=D2/GPIO4, SCL=D1/GPIO5)
// #define LOCAL_SENSOR_DHT22            // DHT22 single-wire
// #define LOCAL_SENSOR_DHT_PIN 14       // Optional: DHT22 Pin überschreiben (default D5/GPIO14)
```

- [ ] After line 41 (`#include "SensorClock.h"`), add:

```cpp
#include "LocalSensor.h"
```

### Step 4.3 — Call begin() in setup()

- [ ] Find line 2101 in `setup()`:
```cpp
  applyEffect(currentEffectIndex);
}
```

Before `applyEffect(currentEffectIndex);`, add:

```cpp
  LocalSensor::begin();
```

### Step 4.4 — Call update() in loop()

- [ ] Find in `loop()`:
```cpp
  server.handleClient();
  yield();
```

After those two lines, add:

```cpp
  LocalSensor::update();
```

### Step 4.5 — Add localSensor field to /api/status

- [ ] In `handleStatus()`, find the closing of the snprintf format string (line ~1134):
```cpp
    "\"lastUptimeBeforeRestartHours\":%u,\"lastUptimeBeforeRestartMinutes\":%u,\"lastHeapBeforeRestartKB\":%u}",
```

Replace with:
```cpp
    "\"lastUptimeBeforeRestartHours\":%u,\"lastUptimeBeforeRestartMinutes\":%u,\"lastHeapBeforeRestartKB\":%u,"
    "\"localSensor\":\"" LOCAL_SENSOR_NAME "\"}",
```

(No extra format argument needed — `LOCAL_SENSOR_NAME` is a compile-time string literal that gets concatenated.)

### Step 4.6 — Build check (no sensor — default)

- [ ] Ensure both `LOCAL_SENSOR_BME280` and `LOCAL_SENSOR_DHT22` are commented out.
- [ ] Compile. Expected: no errors. Binary size unchanged vs Task 3 result.

### Step 4.7 — Build check (with BME280)

- [ ] Uncomment `#define LOCAL_SENSOR_BME280` in the `.ino`.
- [ ] Ensure **Adafruit BME280 Library** and **Adafruit Unified Sensor** are installed in Arduino IDE (Tools → Manage Libraries).
- [ ] Compile. Expected: no errors, binary slightly larger.
- [ ] Comment `LOCAL_SENSOR_BME280` back out before committing (the default shipped build has no sensor active).

### Step 4.8 — Manual test (with BME280)

- [ ] Uncomment `LOCAL_SENSOR_BME280`, flash to device with BME280 on I2C.
- [ ] Serial Monitor: expected `[LocalSensor] BME280 found`.
- [ ] After 10 s: `[LocalSensor] T=21.5 H=63.0`.
- [ ] SensorClock effect shows temperature and humidity slides.
- [ ] `GET /api/status` → `"localSensor":"bme280"`.
- [ ] Comment out define and reflash to restore default after testing.

### Step 4.9 — Commit

- [ ] (Ensure `LOCAL_SENSOR_*` defines are commented out in the committed version.)
```bash
git add IkeaObegraensad.ino LocalSensor.h
git commit -m "feat: add LocalSensor.h with compile-time BME280/DHT22 support"
```

---

## Task 5: Firmware bump + Roadmap

**Files:**
- Modify: `IkeaObegraensad.ino` line 22, `Roadmap.md`

### Step 5.1 — Bump firmware version

- [ ] Change line 22:
```cpp
#define FIRMWARE_VERSION "1.6.2"
```
to:
```cpp
#define FIRMWARE_VERSION "1.7.0"
```

### Step 5.2 — Update Roadmap.md

- [ ] In `Roadmap.md`, under `## 🚀 Kurzfristig`:
  - Mark `WiFi-Reconnect robuster` as `[x]`
  - Move `Restore-Feature vervollständigen` from `## 🔮 Langfristig` and mark `[x]`
  - Mark `Lokale Sensoren direkt anschließen` as `[x]` in both sections where it appears

- [ ] Update the footer:
```
*Letzte Aktualisierung: 2026-04-10*
*Aktuelle Version: 1.7.0*
```

### Step 5.3 — Final build check

- [ ] Compile with no LOCAL_SENSOR defines active. Expected: clean build.

### Step 5.4 — Commit

- [ ] 
```bash
git add IkeaObegraensad.ino Roadmap.md
git commit -m "chore: bump firmware to 1.7.0, update Roadmap"
```

---

## Self-Review Checklist

- [x] **Spec coverage:** WiFi-Reconnect (Task 1), persistNtpToStorage (Task 2), full Restore (Task 3), LocalSensor (Task 4) — all spec sections covered.
- [x] **No placeholders:** All steps contain exact code.
- [x] **Type consistency:** `wifiReconnecting` bool used in globals (Task 1) and WiFi block (Task 1). `extractJsonInt/Bool/Str` defined in Task 3.1, used in Task 3.2. `LocalSensor::begin/update` defined in Task 4.1, called in Task 4.3/4.4. `LOCAL_SENSOR_NAME` macro defined in `LocalSensor.h` (Task 4.1), used in status JSON (Task 4.5). `persistNtpToStorage()` defined in Task 2, called in Task 3.2. All consistent.
- [x] **Scope:** Three focused features, five tasks total.
