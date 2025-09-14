#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <stdlib.h>

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
Effect *currentEffect = &clockEffect;
String tzString = "CET-1CEST,M3.5.0,M10.5.0/3"; // default Europe/Berlin

void handleRoot() {
  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Ikea Obegraensad</title>"
    "<style>body{font-family:sans-serif;text-align:center;background:#111;color:#eee;}"
    "select,input,button{margin:5px;padding:5px;border-radius:4px;border:1px solid #333;background:#222;color:#eee;}"
    "button{cursor:pointer;}</style></head><body>"
    "<h1>Ikea Obegraensad</h1>"
    "<p>Current time: <span id='time'>--:--:--</span></p>"
    "<p>Current effect: <span id='currentEffect'></span></p>"
    "<div><label for='effectSelect'>Effect:</label>"
    "<select id='effectSelect'>"
    "<option value='snake'>Snake</option>"
    "<option value='clock'>Clock</option>"
    "<option value='rain'>Rain</option>"
    "<option value='bounce'>Bounce</option>"
    "<option value='stars'>Stars</option>"
    "<option value='lines'>Lines</option>"
    "</select></div>"
    "<div><label for='tz'>Timezone:</label><input id='tz' size='30'><button id='setTz'>Set</button></div>"
    "<script>"
    "async function update(){const r=await fetch('/api/status');const d=await r.json();"
    "document.getElementById('time').textContent=d.time;"
    "document.getElementById('currentEffect').textContent=d.effect;"
    "document.getElementById('effectSelect').value=d.effect;"
    "document.getElementById('tz').value=d.tz;}"
    "setInterval(update,1000);update();"
    "document.getElementById('effectSelect').addEventListener('change',async e=>{await fetch('/effect/'+e.target.value);update();});"
    "document.getElementById('setTz').addEventListener('click',async()=>{const tz=document.getElementById('tz').value;await fetch('/api/setTimezone?tz='+encodeURIComponent(tz));update();});"
    "</script></body></html>";
  server.send(200, "text/html", html);
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
  String json = String("{\"time\":\"") + buf + "\",\"effect\":\"" + currentEffect->name + "\",\"tz\":\"" + tzString + "\"}";
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

void selectEffect(Effect *e) {
  currentEffect = e;
  currentEffect->init();
  server.send(200, "application/json", String("{\"effect\":\"") + currentEffect->name + "\"}");
}

void setup() {
  Serial.begin(115200);
  matrixSetup();

uint8_t frame[32];
  clearFrame(frame, sizeof(frame));
  setPixel(frame, 0, 0, true);      // first pixel
  setPixel(frame, 15, 15, true);    // last pixel
  shiftOutBuffer(frame, sizeof(frame));
  delay(5000);                      // Anzeige für 5 Sekunden

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", tzString.c_str(), 1);
    tzset();

    server.on("/", handleRoot);
    server.on("/api/status", handleStatus);
    server.on("/api/setTimezone", handleSetTimezone);
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
