#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "display/Matrix.h"
#include "effects/Effect.h"
#include "effects/Snake.h"
#include "effects/Clock.h"
#include "effects/Rain.h"



ESP8266WebServer server(80);
Effect *currentEffect = &snakeEffect;

void handleRoot() {
  String html = "<h1>Ikea Obegraensad</h1><ul>";
  html += "<li><a href=\"/effect/snake\">Snake</a></li>";
  html += "<li><a href=\"/effect/clock\">Clock</a></li>";
  html += "<li><a href=\"/effect/rain\">Rain</a></li>";
  html += "</ul>";
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

  server.on("/", handleRoot);
  server.on("/effect/snake", []() { selectEffect(&snakeEffect); });
  server.on("/effect/clock", []() { selectEffect(&clockEffect); });
  server.on("/effect/rain", []() { selectEffect(&rainEffect); });
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
