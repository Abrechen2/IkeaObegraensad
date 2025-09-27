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

ESP8266WebServer server(80);
bool serverStarted = false;
bool ntpConfigured = false;
const uint16_t DEFAULT_BRIGHTNESS = 512;
uint16_t brightness = DEFAULT_BRIGHTNESS; // 0..1023

const uint8_t EEPROM_MAGIC = 0xB7;
const uint16_t EEPROM_SIZE = 8;
const uint16_t EEPROM_MAGIC_ADDR = 0;
const uint16_t EEPROM_BRIGHTNESS_ADDR = 1;

const uint8_t BUTTON_PIN = D4;

Effect *effects[] = {&snakeEffect, &clockEffect, &rainEffect, &bounceEffect, &starsEffect, &linesEffect};
const uint8_t effectCount = sizeof(effects) / sizeof(effects[0]);
uint8_t currentEffectIndex = 1; // start with clock
Effect *currentEffect = effects[currentEffectIndex];
String tzString = "CET-1CEST,M3.5.0,M10.5.0/3"; // default Europe/Berlin

void loadBrightnessFromStorage() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC) {
    uint16_t stored = DEFAULT_BRIGHTNESS;
    EEPROM.get(EEPROM_BRIGHTNESS_ADDR, stored);
    brightness = constrain(stored, 0, 1023);
  } else {
    brightness = DEFAULT_BRIGHTNESS;
  }
}

void persistBrightnessToStorage() {
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
  EEPROM.commit();
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
  String json = String("{\"time\":\"") + buf +
                "\",\"effect\":\"" + currentEffect->name +
                "\",\"tz\":\"" + tzString +
                "\",\"brightness\":" + String(brightness) + "}";
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
  setenv("TZ", tzString.c_str(), 1);
  tzset();

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
  server.on("/effect/snake", []() { selectEffect(0); });
  server.on("/effect/clock", []() { selectEffect(1); });
  server.on("/effect/rain", []() { selectEffect(2); });
  server.on("/effect/bounce", []() { selectEffect(3); });
  server.on("/effect/stars", []() { selectEffect(4); });
  server.on("/effect/lines", []() { selectEffect(5); });
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

  yield();
  delay(1);
  ESP.wdtFeed();
}

void ICACHE_RAM_ATTR watchdogCallback() {
  Serial.println("WATCHDOG TRIGGERED!");
}
