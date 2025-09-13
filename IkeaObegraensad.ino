#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

#include "Matrix.h"
#include "Effect.h"
#include "Snake.h"
#include "Clock.h"
#include "Rain.h"
#include "Bounce.h"
#include "Stars.h"
#include "Lines.h"
#include "secrets.h"

ESP8266WebServer server(80);
Effect *currentEffect = &snakeEffect;

void handleRoot() {
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buf[16];
  if (t) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  } else {
    strncpy(buf, "--:--:--", sizeof(buf));
  }

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Ikea Obegraensad</title>";
  html += "<style>body{font-family:sans-serif;text-align:center;background:#111;color:#eee;}";
  html += ".btn{display:inline-block;margin:8px;padding:10px 20px;background:#333;color:#eee;text-decoration:none;border-radius:4px;}";
  html += ".btn:hover{background:#555;}</style></head><body>";
  html += "<h1>Ikea Obegraensad</h1>";
  html += "<p>Current time: ";
  html += buf;
  html += "</p><p>Current effect: ";
  html += currentEffect->name;
  html += "</p><p>Select an effect:</p>";
  html += "<a class='btn' href='/effect/snake'>Snake</a>";
  html += "<a class='btn' href='/effect/clock'>Clock</a>";
  html += "<a class='btn' href='/effect/rain'>Rain</a>";
  html += "<a class='btn' href='/effect/bounce'>Bounce</a>";
  html += "<a class='btn' href='/effect/stars'>Stars</a>";
  html += "<a class='btn' href='/effect/lines'>Lines</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void selectEffect(Effect *e) {
  currentEffect = e;
  currentEffect->init();
  server.send(200, "text/plain", String("Selected ") + currentEffect->name);
}

void setup() {
  Serial.begin(115200);
  matrixSetup();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/", handleRoot);
  server.on("/effect/snake", []() { selectEffect(&snakeEffect); });
  server.on("/effect/clock", []() { selectEffect(&clockEffect); });
  server.on("/effect/rain", []() { selectEffect(&rainEffect); });
  server.on("/effect/bounce", []() { selectEffect(&bounceEffect); });
  server.on("/effect/stars", []() { selectEffect(&starsEffect); });
  server.on("/effect/lines", []() { selectEffect(&linesEffect); });
  server.begin();

  currentEffect->init();
}

void loop() {
  server.handleClient();
  uint8_t frame[32];
  clearFrame(frame, sizeof(frame));
  currentEffect->draw(frame);
  shiftOutBuffer(frame, sizeof(frame));
  delay(50);
}
