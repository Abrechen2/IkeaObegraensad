# CLAUDE.md — IkeaObegraensad

## Projektübersicht

ESP8266 (Wemos D1 Mini) Firmware für die IKEA Obegränsad LED-Matrix (16×16 Pixel).
Aktuelle Version: **1.7.0** (in `IkeaObegraensad.ino`, `#define FIRMWARE_VERSION`)

Funktionen: Animationen/Effekte, Uhr mit NTP+DST, SensorClock (Temp/Humi),
MQTT-Steuerung, Auto-Brightness per LDR, Web-UI, OTA-Update, SPIFFS-Logging.

**Verwandtes Repo (HA-Integration):** `D:\Projekte\ikea-obegraensad-homeassistant`
Python HACS-Integration, Version 1.2.13, `domain: ikea_obegraensad`.
Nutzt HTTP-Polling (alle 30s) + mDNS/Zeroconf — kein MQTT.

---

## Build-Umgebung

- **IDE**: Arduino IDE 2.x (oder `arduino-cli` für CLI-Builds, siehe unten)
- **Board**: ESP8266 (Wemos D1 Mini), 80 MHz, 4 MB Flash, FQBN
  `esp8266:esp8266:d1_mini:xtal=80,eesz=4M2M`
- **Bibliotheken**: `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `PubSubClient`,
  `EEPROM`, `ArduinoOTA`, `FS` (SPIFFS)
- **Optional** (nur wenn `LOCAL_SENSOR_*` aktiv): `Adafruit_BME280`, `DHT sensor library`
- **Kein ArduinoJson** — JSON wird manuell mit `extractJsonInt/Bool/Str` geparst

### CLI-Build + OTA (Geräte-IP `192.168.200.9`)

```bash
ARDUINO_CLI="C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
ARDUINO_CONFIG="C:/Users/Dennis Wittke/.config/arduino-cli.yaml"
ESPOTA="C:/Users/Dennis Wittke/AppData/Local/Arduino15/packages/esp8266/hardware/esp8266/3.1.2/tools/espota.py"

cd "D:/Projekte/Arduino/IkeaObegraensad"

# Compile
"$ARDUINO_CLI" compile --config-file "$ARDUINO_CONFIG" \
  --fqbn esp8266:esp8266:d1_mini:xtal=80,eesz=4M2M \
  --output-dir ./build ./

# OTA-Upload (Port 8266 = ESP8266-ArduinoOTA-Default; Passwort aus secrets.h)
python "$ESPOTA" -i 192.168.200.9 -p 8266 \
  --auth="$OTA_PASSWORD" \
  -f ./build/IkeaObegraensad.ino.bin
```

Verifikation: `curl -s http://192.168.200.9/api/status | jq .firmwareVersion`

---

## Dateistruktur

```
IkeaObegraensad/
├── IkeaObegraensad.ino   Hauptsketch (~2450 Zeilen): globals, setup(), loop(), alle Handler
├── secrets.h             WiFi/OTA-Credentials (gitignored — NIEMALS committen!)
├── secrets_template.h    Template für secrets.h
├── WebInterface.h        Gesamte Web-UI als C++-Raw-String (HTML/CSS/JS inline)
├── LocalSensor.h         Compile-time BME280/DHT22-Abstraktion (v1.7.0)
├── SensorClock.h         SensorClock-Effekt (Uhr+Temp+Humi Slideshow)
├── Logging.h             SPIFFS-basiertes Logging
├── Matrix.h              Low-level Pixel-Operationen (setPixel, clearFrame)
├── Effect.h              Effect-Struct: { init, draw, name }
├── Clock.h / ClockFont.h Uhren-Effekt und 5×7-Font
├── SandClock.h           Sanduhr-Effekt
├── Snake/Rain/Bounce/Stars/Lines/Pulse/Waves/Spiral/Fire/Plasma/Ripple.h  Weitere Effekte
├── Roadmap.md            Feature-Backlog und erledigte Items
└── docs/superpowers/     Specs und Pläne (Brainstorming-Artefakte)
```

**`secrets.h` niemals committen.** Bei neu geklontem Repo aus `secrets_template.h` kopieren.

---

## Effekt-Verzeichnis

| Index | Name | Datei |
|-------|------|-------|
| 0 | snake | `Snake.h` |
| 1 | clock | `Clock.h` |
| 2 | rain | `Rain.h` |
| 3 | bounce | `Bounce.h` |
| 4 | stars | `Stars.h` |
| 5 | lines | `Lines.h` |
| 6 | pulse | `Pulse.h` |
| 7 | waves | `Waves.h` |
| 8 | spiral | `Spiral.h` |
| 9 | fire | `Fire.h` |
| 10 | plasma | `Plasma.h` |
| 11 | ripple | `Ripple.h` |
| 12 | sandclock | `SandClock.h` |
| 13 | sensorclock | `SensorClock.h` |

Neuen Effekt hinzufügen: `.h`-Datei erstellen → `effects[]`-Array in `.ino` erweitern → Index in HA-Integration (`const.py`) nachtragen.

---

## EEPROM-Layout (Version 3 — eingefroren)

Layout darf **nicht** ohne Migration geändert werden. Bestehende Adressen sind fest.
Checksum-Feld bei `EEPROM_CHECKSUM_ADDR = 2` — nach jedem Schreiben neu berechnen.

Letzte belegte Adresse: `519 + 2 = 521 Bytes` (von 1024 allokiert).
**Nächste freie Adresse: 521.**

Neue Felder immer am Ende hinzufügen; `EEPROM_SIZE` ggf. anpassen (Breaking Change → Migration nötig).

Persist-Funktionen — immer mit `commitEEPROMWithWatchdog()`:
- `persistBrightnessToStorage()`
- `persistMqttToStorage()`
- `persistNtpToStorage()`
- `persistSlideConfig()`

---

## Vollständige API-Referenz

### Firmware-Endpunkte

| Methode | Pfad | Funktion |
|---------|------|----------|
| GET | `/` | Web-UI (HTML) |
| GET | `/api/status` | JSON mit allen Zuständen und Einstellungen |
| POST | `/api/setBrightness` | Helligkeit setzen (`brightness=0..1023`) |
| POST | `/api/setAutoBrightness` | Auto-Brightness konfigurieren |
| POST | `/api/setTimezone` | Zeitzone (TZ-String) und NTP-Server setzen |
| POST | `/api/setClockFormat` | 12h/24h umschalten |
| POST | `/api/setMqtt` | MQTT-Konfiguration speichern |
| POST | `/api/setDisplay` | Display ein/aus (`display=on\|off`) |
| POST | `/api/setSensorData` | Temp/Humi manuell pushen |
| POST | `/api/setSlideConfig` | SensorClock Foliendauern setzen |
| POST | `/api/resetRestartCount` | Restart-Zähler zurücksetzen |
| GET | `/api/backup` | Aktuelle Settings als JSON exportieren |
| POST | `/api/restore` | Settings aus Backup-JSON + Reboot |
| GET | `/api/debuglog` | SPIFFS-Logdatei auslesen |
| GET | `/effect/<name>` | Effekt direkt aktivieren |

### MQTT-Steuerung

Topic-Schema: `<mqttBaseTopic>/cmd` (subscribe), `<mqttBaseTopic>/state` (publish)

Befehle auf `/cmd`:
- `brightness:512` — Helligkeit (0–1023)
- `effect:clock` — Effekt wechseln
- `display:on` / `display:off`
- `autobrightness:on` / `autobrightness:off`
- `temp:21.5` / `humi:63.0` — Sensordaten pushen

### HA-Integration (HTTP-Polling)

Nutzt: `GET /api/status` (alle 30s), `POST /api/setDisplay`, `POST /api/setBrightness`,
`POST /api/setAutoBrightness`, `POST /api/setTimezone`, `POST /api/setSensorData`,
`POST /api/setSlideConfig`, `GET /effect/<name>`

Brightness-Umrechnung: HA (0–255) ↔ API (0–1023): `api = ha * 1023 / 255`

---

## Wichtige Regeln

- **NIEMALS `taskkill` ausfuehren** — keine Prozesse beenden. Wenn ein Port belegt ist, den Benutzer fragen.

## Wichtige Konventionen

### `millis()`-Überlauf
Immer `timeDiff(millis(), lastX) > interval` — nicht `millis() - lastX`.
`timeDiff()` ist overflow-sicher. Ausnahme: `LocalSensor.h` (include-Reihenfolge; intentional dokumentiert).

### Effekt-Pattern
Jeder Effekt ist ein globales `Effect`-Struct `{ init, draw, name }` in einer `.h`-Datei.
Namespace-static State → jede `.h` nur einmal includen (ODR-Regel).
`init()` wird beim Effektwechsel aufgerufen, `draw(frame)` jeden Frame.

### Lokaler Sensor (compile-time, kein RAM-Overhead wenn deaktiviert)
```cpp
// IkeaObegraensad.ino, nach FIRMWARE_VERSION:
// #define LOCAL_SENSOR_BME280      // I²C: SDA=D2/GPIO4, SCL=D1/GPIO5
// #define LOCAL_SENSOR_DHT22       // Single-wire, default D5/GPIO14
// #define LOCAL_SENSOR_DHT_PIN 14  // Pin überschreiben (optional)
```
Schreibt in `g_sensorTemp` / `g_sensorHumi` — dieselben Globals wie MQTT-Push.

### WebInterface.h
Gesamte Web-UI ist ein C++-Raw-String (`R"===(...)==="`) — kein Build-System.
Änderungen direkt in der Datei; Firmware danach neu flashen.

### Logging
`debugLog(...)` / `verboseLog(...)` — nur aktiv wenn `DEBUG_LOGGING_ENABLED` definiert.
`Serial.printf(...)` für produktionsrelevante Ausgaben (WiFi, NTP, Sensor).

### Kein `delay()` in `loop()`
Alle zeitgesteuerten Operationen mit `millis()` und State-Variablen.
WiFi- und MQTT-Reconnect sind nicht-blockierend (Exponential Backoff).

---

## Wichtige Globals (`IkeaObegraensad.ino`)

| Variable | Typ | Bedeutung |
|----------|-----|-----------|
| `g_sensorTemp` | `float` | Aktuelle Temperatur (NAN = kein Wert) |
| `g_sensorHumi` | `float` | Aktuelle Luftfeuchtigkeit (NAN = kein Wert) |
| `brightness` | `uint16_t` | PWM-Wert 0–1023 (invertiert: `analogWrite(PIN_ENABLE, PWM_MAX - brightness)`) |
| `ntpConfigured` | `bool` | `false` setzen → NTP-Sync wird getriggert |
| `mqttStateDirty` | `bool` | `true` setzen → MQTT State-Publish ausstehend |
| `wifiReconnecting` | `bool` | WiFi-Reconnect State Machine aktiv |
| `serverStarted` | `bool` | Web-Server läuft |
| `displayEnabled` | `bool` | Display ein/aus |
| `autoBrightnessEnabled` | `bool` | Auto-Brightness aktiv |

---

## Firmware-Version bumpen

1. `#define FIRMWARE_VERSION "x.y.z"` in `IkeaObegraensad.ino`
2. `*Aktuelle Version: x.y.z*` in `Roadmap.md`
3. Commit: `chore: bump firmware to x.y.z`

---

## Verwandtes Repo: HA-Integration

**Pfad:** `D:\Projekte\ikea-obegraensad-homeassistant`
**Domain:** `ikea_obegraensad` | **Version:** 1.2.13 | **Sprache:** Python

Wenn ein neuer API-Endpunkt oder Effekt in der Firmware hinzukommt, muss die
HA-Integration ggf. ebenfalls aktualisiert werden:
- Neue Endpunkte → `const.py` + passende Platform-Datei (`sensor.py`, `switch.py`, etc.)
- Neue Effekte → `EFFECTS`-Liste in `const.py` erweitern
