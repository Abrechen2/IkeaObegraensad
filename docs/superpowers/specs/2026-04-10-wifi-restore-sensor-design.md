# WiFi-Reconnect, Restore & LocalSensor — Design Spec

## Overview

Three independent improvements to the IkeaObegraensad ESP8266 firmware (v1.6.2 → v1.7.0):

1. **WiFi-Reconnect robuster** — non-blocking reconnect with exponential backoff
2. **Restore-Feature** — `/api/restore` fully implemented (was a stub)
3. **Lokaler Sensor** — compile-time BME280/DHT22 support via `LocalSensor.h`

**Architecture:** All three are isolated changes. No cross-feature dependencies.
**Target:** Arduino IDE / ESP8266 / Wemos D1 Mini, C++11, no new heavy dependencies.

---

## Feature 1: WiFi-Reconnect robuster

### Problem

Current `loop()` (line ~2190) calls `WiFi.reconnect()` once every 30s when disconnected.
No verification, no backoff — it hammers the WiFi stack at a fixed rate regardless of outcome.

### Design

#### New global variables (`IkeaObegraensad.ino`, near MQTT backoff constants)

```cpp
unsigned long wifiReconnectBackoff = 5000;         // ms, doubles on failure up to max
const unsigned long WIFI_RECONNECT_MAX_BACKOFF = 60000;
const unsigned long WIFI_RECONNECT_VERIFY_TIMEOUT = 8000; // ms to wait for connect
bool wifiReconnecting = false;
unsigned long wifiReconnectStartMs = 0;
```

#### Replaced WiFi-check block in `loop()`

Replace the existing 30s block (starting `if (timeDiff(millis(), lastWiFiCheck) > 30000)`) with:

```cpp
{
  unsigned long wifiCheckInterval = wifiReconnecting
    ? 500UL                                              // poll 2×/s while waiting
    : (WiFi.status() == WL_CONNECTED ? 30000UL : wifiReconnectBackoff);

  if (timeDiff(millis(), lastWiFiCheck) > wifiCheckInterval) {
    wl_status_t wifiStatus = WiFi.status();

    if (wifiReconnecting) {
      if (wifiStatus == WL_CONNECTED) {
        wifiReconnecting = false;
        wifiReconnectBackoff = 5000;
        Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        ntpConfigured = false;                           // trigger NTP re-sync
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
      ESP.wdtFeed();
      WiFi.begin(ssid, password);
      wifiReconnecting = true;
      wifiReconnectStartMs = millis();
    }

    lastWiFiCheck = millis();
  }
}
```

#### Behaviour

| State | Check interval | Action |
|-------|---------------|--------|
| Connected | 30s | Monitor only |
| Reconnecting | 500ms | Poll until connected or 8s timeout |
| Disconnected (idle) | `wifiReconnectBackoff` (5→10→20→40→60s) | `WiFi.begin()` |

No `delay()` — display and web server stay responsive during reconnect.

---

## Feature 2: Restore-Feature vervollständigen

### Problem

`handleRestore()` validates JSON structure but returns `"restore_not_fully_implemented"`.
`persistNtpToStorage()` is missing despite NTP EEPROM addresses being defined.

### Design

#### New `persistNtpToStorage()` (beside the other persist functions, ~line 550)

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

#### Three static helper functions (immediately before `handleRestore()`)

All operate on the `String jsonData` passed from `server.arg("plain")`.

```cpp
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

#### Rewritten `handleRestore()` body

Replace everything after the existing `configStart` check with:

```cpp
// 1. Extract and validate all fields
int  newBrightness      = extractJsonInt (jsonData, "brightness",   brightness);
bool newAutoBrightness  = extractJsonBool(jsonData, "autoBrightness", autoBrightnessEnabled);
int  newMinBrightness   = extractJsonInt (jsonData, "minBrightness", minBrightness);
int  newMaxBrightness   = extractJsonInt (jsonData, "maxBrightness", maxBrightness);
int  newSensorMin       = extractJsonInt (jsonData, "sensorMin",    sensorMin);
int  newSensorMax       = extractJsonInt (jsonData, "sensorMax",    sensorMax);
bool newMqttEnabled     = extractJsonBool(jsonData, "mqttEnabled",  mqttEnabled);
bool newUse24h          = extractJsonBool(jsonData, "use24HourFormat", use24HourFormat);

char newMqttServer[INPUT_MQTT_SERVER_MAX] = "";
char newMqttUser  [INPUT_MQTT_USER_MAX]   = "";
char newMqttTopic [INPUT_MQTT_TOPIC_MAX]  = "";
char newTz        [INPUT_TZ_MAX]          = "";
char newNtp1      [INPUT_NTP_SERVER_MAX]  = "";
char newNtp2      [INPUT_NTP_SERVER_MAX]  = "";
extractJsonStr(jsonData, "mqttServer",   newMqttServer, sizeof(newMqttServer));
extractJsonStr(jsonData, "mqttUser",     newMqttUser,   sizeof(newMqttUser));
extractJsonStr(jsonData, "mqttBaseTopic",newMqttTopic,  sizeof(newMqttTopic));
extractJsonStr(jsonData, "tz",           newTz,         sizeof(newTz));
extractJsonStr(jsonData, "ntpServer1",   newNtp1,       sizeof(newNtp1));
extractJsonStr(jsonData, "ntpServer2",   newNtp2,       sizeof(newNtp2));
int  newMqttPort = extractJsonInt(jsonData, "mqttPort", mqttPort);

// Validate
newBrightness   = constrain(newBrightness,   0, PWM_MAX);
newMinBrightness= constrain(newMinBrightness,0, PWM_MAX);
newMaxBrightness= constrain(newMaxBrightness,0, PWM_MAX);
newSensorMin    = constrain(newSensorMin,    0, PWM_MAX);
newSensorMax    = constrain(newSensorMax,    0, PWM_MAX);
newMqttPort     = (newMqttPort > 0 && newMqttPort <= 65535) ? newMqttPort : MQTT_PORT_DEFAULT;
if (strlen(newMqttTopic) > 0 && !isValidMqttBaseTopic(newMqttTopic)) {
  server.send(400, "application/json", "{\"error\":\"Invalid MQTT topic in backup\"}");
  return;
}
if (strlen(newTz) > 0 && !isValidTzString(newTz)) {
  server.send(400, "application/json", "{\"error\":\"Invalid TZ string in backup\"}");
  return;
}

// 2. Apply to globals
brightness          = newBrightness;
autoBrightnessEnabled = newAutoBrightness;
minBrightness       = newMinBrightness;
maxBrightness       = newMaxBrightness;
sensorMin           = newSensorMin;
sensorMax           = newSensorMax;
mqttEnabled         = newMqttEnabled;
use24HourFormat     = newUse24h;
if (strlen(newMqttServer) > 0) strncpy(mqttServer,  newMqttServer, sizeof(mqttServer)  - 1);
if (strlen(newMqttUser)   > 0) strncpy(mqttUser,    newMqttUser,   sizeof(mqttUser)    - 1);
if (strlen(newMqttTopic)  > 0) strncpy(mqttBaseTopic, newMqttTopic, sizeof(mqttBaseTopic) - 1);
if (strlen(newTz)         > 0) strncpy(tzString,    newTz,         sizeof(tzString)    - 1);
if (strlen(newNtp1) > 0 && strchr(newNtp1, '.') != nullptr)
  strncpy(ntpServer1, newNtp1, sizeof(ntpServer1) - 1);
if (strlen(newNtp2) > 0 && strchr(newNtp2, '.') != nullptr)
  strncpy(ntpServer2, newNtp2, sizeof(ntpServer2) - 1);
mqttPort = (uint16_t)newMqttPort;

// 3. Persist
persistBrightnessToStorage();
persistMqttToStorage();
persistNtpToStorage();

// 4. Apply runtime effects
analogWrite(PIN_ENABLE, PWM_MAX - brightness);
setupTimezone();
ntpConfigured = false;
if (mqttEnabled && strlen(mqttServer) > 0) {
  mqttClient.disconnect();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

// 5. Respond and reboot
server.send(200, "application/json",
  "{\"status\":\"ok\",\"message\":\"Settings restored, rebooting...\"}");
delay(500);
ESP.restart();
```

#### Backup completeness note

The backup JSON does **not** include `mqttPassword` (intentional, security). After restore, the user must re-enter the MQTT password in the WebUI if needed. The `mqttPassword` global keeps its current runtime value during the restore session; after reboot it reads from EEPROM (which `persistMqttToStorage` wrote — but mqttPassword was not overwritten by restore, so existing value is preserved).

---

## Feature 3: Lokaler Sensor — `LocalSensor.h`

### Design

Compile-time sensor selection. No EEPROM, no WebUI changes, zero overhead when disabled.

#### User configuration (top of `IkeaObegraensad.ino`, after `#define FIRMWARE_VERSION`)

```cpp
// === Lokaler Sensor ===
// Eine Zeile einkommentieren (oder beide auskommentiert lassen für MQTT-only):
// #define LOCAL_SENSOR_BME280           // BME280 via I²C (SDA=D2/GPIO4, SCL=D1/GPIO5)
// #define LOCAL_SENSOR_DHT22            // DHT22 single-wire
// #define LOCAL_SENSOR_DHT_PIN 14       // Optional: DHT22 Pin (default: D5/GPIO14)
```

#### New file `LocalSensor.h`

```cpp
#ifndef LOCAL_SENSOR_H
#define LOCAL_SENSOR_H

// Compile-time sensor selection
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
      Serial.printf("[LocalSensor] DHT22 pin %d\n", LOCAL_SENSOR_DHT_PIN);
    #endif
  }

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
    if (!isnan(t)) { g_sensorTemp = t; }
    if (!isnan(h)) { g_sensorHumi = h; }
    Serial.printf("[LocalSensor] T=%.1f H=%.1f\n", g_sensorTemp, g_sensorHumi);
  }

  inline bool isAvailable() { return _available; }

#else  // no sensor configured

  inline void begin()  {}
  inline void update() {}
  inline bool isAvailable() { return false; }

#endif  // _LOCAL_SENSOR_ACTIVE

}  // namespace LocalSensor

#endif  // LOCAL_SENSOR_H
```

#### Changes to `IkeaObegraensad.ino`

- `#include "LocalSensor.h"` after `#include "SensorClock.h"`
- In `setup()`: `LocalSensor::begin();`
- In `loop()`: `LocalSensor::update();`
- In `handleStatus()` JSON: add `"localSensor":"` + `LOCAL_SENSOR_NAME` + `""`

#### Memory impact

| Sensor | Additional flash | Additional RAM |
|--------|-----------------|----------------|
| None   | 0               | 0              |
| BME280 | ~15 KB          | ~0.5 KB        |
| DHT22  | ~3 KB           | ~0.1 KB        |

---

## Files Changed

| File | Type | Change |
|------|------|--------|
| `IkeaObegraensad.ino` | Modify | WiFi-Reconnect vars + logic, `persistNtpToStorage()`, JSON helpers, `handleRestore()`, sensor config comment, `LocalSensor::begin/update` calls, status field |
| `LocalSensor.h` | Create | New sensor abstraction module |
| `Roadmap.md` | Modify | Mark 3 items complete, bump version to v1.7.0 |

---

*Spec written: 2026-04-10*
*Features: WiFi-Reconnect, Restore, LocalSensor*
*Target firmware: v1.7.0*
