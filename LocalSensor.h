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
    if (millis() - _lastRead < 10000UL) return;  // unsigned wrap is safe; can't use timeDiff() (include order)
    _lastRead = millis();

    #if defined(LOCAL_SENSOR_BME280)
      float t = _bme.readTemperature();
      float h = _bme.readHumidity();
    #elif defined(LOCAL_SENSOR_DHT22)
      float t = _dht.readTemperature();
      float h = _dht.readHumidity();
    #endif

    bool updated = false;
    if (!isnan(t)) { g_sensorTemp = t; updated = true; }
    if (!isnan(h)) { g_sensorHumi = h; updated = true; }
    if (updated) Serial.printf("[LocalSensor] T=%.1f H=%.1f\n", g_sensorTemp, g_sensorHumi);
  }

  inline bool isAvailable() { return _available; }

#else  // no sensor configured — everything is a no-op

  inline void begin()           {}
  inline void update()          {}
  inline bool isAvailable()     { return false; }

#endif  // _LOCAL_SENSOR_ACTIVE

}  // namespace LocalSensor

#endif  // LOCAL_SENSOR_H
