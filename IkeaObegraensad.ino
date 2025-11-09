#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include <stdlib.h>
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
const uint16_t DEFAULT_BRIGHTNESS = 512;
uint16_t brightness = DEFAULT_BRIGHTNESS; // 0..1023

// Auto-Brightness Konfiguration
bool autoBrightnessEnabled = false;
uint16_t minBrightness = 100;   // Minimale Helligkeit (0-1023)
uint16_t maxBrightness = 1023;  // Maximale Helligkeit (0-1023)
uint16_t sensorMin = 5;         // Minimaler Sensorwert (dunkel) - LDR-spezifisch
uint16_t sensorMax = 450;       // Maximaler Sensorwert (hell) - LDR-spezifisch
const uint8_t LIGHT_SENSOR_PIN = A0; // Analoger Pin für Phototransistor

// Auto-Brightness Konstanten
const uint8_t LIGHT_SENSOR_SAMPLES = 10;    // Anzahl der Sensor-Messungen für Mittelwertbildung (erhöht für Stabilität)
const uint8_t LIGHT_SENSOR_SAMPLE_DELAY = 20; // ms zwischen Sensor-Messungen (erhöht für bessere Mittelung)
const uint16_t BRIGHTNESS_CHANGE_THRESHOLD = 80; // Minimale Änderung (~8%) bevor Update erfolgt (erhöht gegen TV-Flackern)
const unsigned long AUTO_BRIGHTNESS_UPDATE_INTERVAL = 5000; // ms zwischen Auto-Brightness Updates (5s statt 1s für Raumlichtstabilität)

// Gleitender Durchschnitt für sanfte Helligkeitsanpassung (gegen TV-Flackern)
const uint8_t SENSOR_HISTORY_SIZE = 6;      // Anzahl der letzten Messwerte für gleitenden Durchschnitt
uint16_t sensorHistory[SENSOR_HISTORY_SIZE] = {0}; // Ringpuffer für Sensorwerte
uint8_t sensorHistoryIndex = 0;             // Aktueller Index im Ringpuffer
bool sensorHistoryFilled = false;           // Ist der Ringpuffer vollständig gefüllt?

const uint8_t EEPROM_MAGIC = 0xB7;
const uint16_t EEPROM_SIZE = 16; // Erweitert von 8 auf 16 Bytes
const uint16_t EEPROM_MAGIC_ADDR = 0;
const uint16_t EEPROM_BRIGHTNESS_ADDR = 1;
const uint16_t EEPROM_AUTO_BRIGHTNESS_ADDR = 3;    // bool (1 byte)
const uint16_t EEPROM_MIN_BRIGHTNESS_ADDR = 4;     // uint16_t (2 bytes)
const uint16_t EEPROM_MAX_BRIGHTNESS_ADDR = 6;     // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MIN_ADDR = 8;         // uint16_t (2 bytes)
const uint16_t EEPROM_SENSOR_MAX_ADDR = 10;        // uint16_t (2 bytes)

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

    // Werte validieren
    minBrightness = constrain(minBrightness, 0, 1023);
    maxBrightness = constrain(maxBrightness, 0, 1023);
    sensorMin = constrain(sensorMin, 0, 1023);
    sensorMax = constrain(sensorMax, 0, 1023);
  } else {
    brightness = DEFAULT_BRIGHTNESS;
    autoBrightnessEnabled = false;
    minBrightness = 100;
    maxBrightness = 1023;
    sensorMin = 5;
    sensorMax = 450;
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

// Auto-Brightness: Sensor auslesen und Helligkeit anpassen
uint16_t readLightSensor() {
  // Mehrere Messungen für stabilere Werte (Mittelwertbildung reduziert Rauschen)
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LIGHT_SENSOR_SAMPLES; i++) {
    sum += analogRead(LIGHT_SENSOR_PIN);
    delay(LIGHT_SENSOR_SAMPLE_DELAY);
  }
  return sum / LIGHT_SENSOR_SAMPLES;
}

// Gleitender Durchschnitt über die letzten N Sensorwerte (reduziert TV-Flackern)
uint16_t getSmoothedSensorValue(uint16_t newValue) {
  // Neuen Wert in Ringpuffer speichern
  sensorHistory[sensorHistoryIndex] = newValue;
  sensorHistoryIndex = (sensorHistoryIndex + 1) % SENSOR_HISTORY_SIZE;

  if (!sensorHistoryFilled && sensorHistoryIndex == 0) {
    sensorHistoryFilled = true;
  }

  // Durchschnitt berechnen
  uint32_t sum = 0;
  uint8_t count = sensorHistoryFilled ? SENSOR_HISTORY_SIZE : sensorHistoryIndex;

  if (count == 0) {
    return newValue; // Fallback beim ersten Aufruf
  }

  for (uint8_t i = 0; i < count; i++) {
    sum += sensorHistory[i];
  }

  return sum / count;
}

void updateAutoBrightness() {
  if (!autoBrightnessEnabled) {
    return;
  }

  // Sensor auslesen und durch gleitenden Durchschnitt glätten (gegen TV-Flackern)
  uint16_t rawSensorValue = readLightSensor();
  uint16_t smoothedSensorValue = getSmoothedSensorValue(rawSensorValue);

  // Map Sensorwert (sensorMin..sensorMax) auf Helligkeit (minBrightness..maxBrightness)
  uint16_t newBrightness;
  if (smoothedSensorValue <= sensorMin) {
    newBrightness = minBrightness;
  } else if (smoothedSensorValue >= sensorMax) {
    newBrightness = maxBrightness;
  } else {
    // Lineare Interpolation zwischen Min und Max
    newBrightness = map(smoothedSensorValue, sensorMin, sensorMax, minBrightness, maxBrightness);
  }

  // Nur aktualisieren wenn sich die Helligkeit signifikant ändert (Hysterese verhindert Flackern)
  if (abs((int)newBrightness - (int)brightness) > BRIGHTNESS_CHANGE_THRESHOLD) {
    brightness = newBrightness;
    analogWrite(PIN_ENABLE, 1023 - brightness);
    Serial.printf("Auto-Brightness: Raw=%d, Smoothed=%d -> Brightness=%d\n",
                  rawSensorValue, smoothedSensorValue, brightness);
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
                ",\"sandEnabled\":" + (SandClockEffect::sandEffectEnabled ? "true" : "false") +
                ",\"autoBrightness\":" + (autoBrightnessEnabled ? "true" : "false") +
                ",\"minBrightness\":" + String(minBrightness) +
                ",\"maxBrightness\":" + String(maxBrightness) +
                ",\"sensorMin\":" + String(sensorMin) +
                ",\"sensorMax\":" + String(sensorMax) +
                ",\"sensorValue\":" + String(sensorValue) + "}";
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

  EEPROM.begin(EEPROM_SIZE);
  loadBrightnessFromStorage();

  matrixSetup();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  startAnimation();

  bool wifiConnected = setupWiFi();
  ntpConfigured = wifiConnected;
  if (wifiConnected) {
    setupNTP();
  }

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/setTimezone", handleSetTimezone);
  server.on("/api/setBrightness", handleSetBrightness);
  server.on("/api/setAutoBrightness", handleSetAutoBrightness);
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
  server.on("/api/toggleSand", []() {
    toggleSandEffect();
    server.send(200, "application/json",
                String("{\"sandEnabled\":") +
                (SandClockEffect::sandEffectEnabled ? "true" : "false") + "}");
  });
  if (wifiConnected) {
    server.begin();
    serverStarted = true;
  } else {
    Serial.println("Web server not started because WiFi is not connected.");
  }

  applyEffect(currentEffectIndex);
}

void loop() {
  static unsigned long lastFrameUpdate = 0;
  static unsigned long lastButtonCheck = 0;
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastStatusPrint = 0;
  static unsigned long lastBrightnessUpdate = 0;

  server.handleClient();
  yield();

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
    setupNTP();
    ntpConfigured = true;
  }

  if (millis() - lastButtonCheck > 50) {
    static unsigned long lastPress = 0;
    if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > 300) {
      nextEffect();
      lastPress = millis();
    }
    lastButtonCheck = millis();
  }

  if (millis() - lastFrameUpdate > 50) {
    uint8_t frame[32];
    clearFrame(frame, sizeof(frame));
    currentEffect->draw(frame);
    shiftOutBuffer(frame, sizeof(frame));
    lastFrameUpdate = millis();
  }

  if (millis() - lastStatusPrint > 60000) {
    Serial.printf("Uptime: %lus, Free heap: %d bytes\n",
                  millis() / 1000, ESP.getFreeHeap());
    lastStatusPrint = millis();
  }

  // Auto-Brightness regelmäßig aktualisieren
  if (millis() - lastBrightnessUpdate > AUTO_BRIGHTNESS_UPDATE_INTERVAL) {
    updateAutoBrightness();
    lastBrightnessUpdate = millis();
  }

  yield();
  delay(1);
  ESP.wdtFeed();
}

void ICACHE_RAM_ATTR watchdogCallback() {
  Serial.println("WATCHDOG TRIGGERED!");
}
