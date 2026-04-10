# Roadmap - IkeaObegraensad

Diese Roadmap enthält geplante Verbesserungen und Ideen für zukünftige Versionen.

## ✅ Bereits erledigt

### v1.5.x
- [x] **EEPROM-Versionierung & Migration** (v3-Layout mit Checksumme, DST-TZ-Migration)
- [x] **Auto-Brightness mit EMA** (Exponential Moving Average, non-blocking Sampling)
- [x] **NTP + DST-Fix** (`configTime(tz, ntp)` statt getrennte Aufrufe — überlebt NTP-Sync)
- [x] **Web-UI modernisiert** (Dark Mode, Cards, Accordion, Responsive Design)

### v1.6.x
- [x] **Erweiterte MQTT-Integration**
  - Generischer `cmd/state`-Kanal ersetzt Präsenz-Logik (`<baseTopic>/cmd`, `<baseTopic>/state`)
  - Helligkeit über MQTT steuerbar (`brightness:512`)
  - Effekt-Wechsel über MQTT (`effect:clock`)
  - Display ein/aus über MQTT (`display:on`)
  - Auto-Brightness über MQTT (`autobrightness:on`)
  - MQTT State mit `display`, `effect`, `brightness`, `autoBrightness`, `sensorTemp`, `sensorHumi`, Slide-Dauern
- [x] **SensorClock-Effekt** (Temperatur/Luftfeuchtigkeit von Home Assistant via MQTT oder HTTP-API, 3-Folien-Slideshow)
  - Sensordaten per MQTT (`temp:21.5`, `humi:63.0`) oder `POST /api/setSensorData`
  - Foliendauern konfigurierbar (EEPROM-persistiert) via MQTT oder `POST /api/setSlideConfig`
  - WebUI-Kachel und Konfigurationssektion
- [x] **Input-Validierung** (TZ-String vor `setenv()`, MQTT-Topic Wildcards `+#/` blockiert)
- [x] **Security-Fixes** (MQTT-Topic-Validierung, TZ-Zeichensatz-Check)

---

## 🚀 Kurzfristig (v1.6.x – v1.7.x)

### Code-Optimierung
- [ ] **Logging-Makros statt inline-Funktionen**
  - Problem: Bei deaktiviertem Logging werden Parameter trotzdem evaluiert
  - Lösung: Makros verwenden (`#define debugLog(...) ((void)0)`)
  - Datei: `Logging.h` Z. 199–203

- [ ] **Zwei Build-Varianten in GitHub**
  - Variante 1: Mit erweitertem Logging (`DEBUG_LOGGING_ENABLED`)
  - Variante 2: Mit einfachem Serial-Logging
  - Lösung: Conditional Compilation erweitern oder Git Branches

- [ ] **EEPROM-Größe anpassen**
  - Aktuell: `EEPROM.begin(1024)`, genutzt: ~521 Bytes
  - Verbesserung: `EEPROM.begin(528)` spart ~500 Bytes Heap
  - Achtung: `calculateEEPROMChecksum()` iteriert bis `EEPROM_SIZE` → Breaking Change, Migration nötig

### Stabilität
- [ ] **WiFi-Reconnect robuster**
  - Verbesserung: Mehrere Versuche mit Backoff, Status-Überwachung
  - Datei: `IkeaObegraensad.ino` `setupWiFi()` und `loop()`

- [ ] **SPIFFS-Fehlerbehandlung verbessern**
  - Aktuell: Reinitialisierung alle 5 Minuten
  - Datei: `Logging.h` SPIFFS-Checks

- [ ] **Watchdog-Fütterung konsolidieren**
  - Aktuell: Viele manuelle `ESP.wdtFeed()`-Aufrufe
  - Verbesserung: Wrapper-Funktion für kritische Operationen

## 📅 Mittelfristig (v1.7.x – v1.8.x)

### Neue Features
- [ ] **Wetter-Integration (Erweiterung SensorClock)**
  - SensorClock zeigt bereits lokale Sensordaten — Erweiterung um externe Wetterdaten möglich
  - OpenWeatherMap API, Icon-Symbole auf Display
  - Alternativ: Home Assistant liefert Wetterdaten via MQTT (wie Temp/Humi)
  - Datei: `SensorClock.h` erweitern oder neue `WeatherEffect.h`

- [ ] **Scheduler für Effekte**
  - Automatischer Effekt-Wechsel nach Zeitplan (z.B. Uhr tagsüber, SensorClock nachts)
  - Konfigurierbar über Web-UI
  - Datei: Neue `Scheduler.h`

- [ ] **Mehrere Effekte überblenden**
  - Effekt-Layering-System
  - Datei: Neue `EffectManager.h`

- [ ] **WebSocket für Live-Updates**
  - Aktuell: Polling alle X Sekunden
  - Verbesserung: WebSocket für Echtzeit-Updates
  - Datei: `WebInterface.h` erweitern

### Benutzerfreundlichkeit
- [ ] **Effekt-Vorschau im Web-Interface**
  - Mini-Vorschau oder Screenshot jedes Effekts
  - Datei: `WebInterface.h`

- [ ] **Konfigurations-Assistent**
  - Schritt-für-Schritt Setup für neue Benutzer (WiFi, MQTT, NTP)
  - Datei: Neue `SetupWizard.h`

### Performance
- [ ] **Frame-Rate-Optimierung**
  - Aktuell: 50 ms/Frame (20 FPS) → Ziel: 33 ms/Frame (30 FPS)
  - Datei: Alle `Effect.h`-Dateien

- [ ] **Speicher-Optimierung**
  - String-Operationen reduzieren, statische Buffer wiederverwenden
  - `handleStatus()` JSON-Buffer (1536 Bytes) stream-basiert ersetzen
  - Datei: `IkeaObegraensad.ino`

## 🔮 Langfristig (v2.0.x+)

### Hardware-Erweiterungen
- [ ] **Mehrere Buttons**
  - Vor/Zurück, Helligkeit +/-, Menü
  - Datei: Neue `ButtonManager.h`

- [ ] **Lokale Sensoren direkt anschließen**
  - Aktuell: Sensordaten kommen von Home Assistant via MQTT
  - Erweiterung: BME280/DHT22 direkt am ESP8266 (I²C/1-Wire)
  - SensorClock würde direkt vom Sensor lesen statt auf Push zu warten
  - Datei: `SensorClock.h`, neue `LocalSensor.h`

- [ ] **SD-Karte Support**
  - Größere Logs, Custom-Effekte, Backup
  - Datei: Neue `SDCard.h`

### Erweiterte Features
- [ ] **Restore-Feature vervollständigen**
  - `POST /api/restore` parst JSON aktuell nicht vollständig (Stub vorhanden)
  - Datei: `IkeaObegraensad.ino` `handleRestore()`

- [ ] **Home Assistant Auto-Discovery**
  - MQTT Discovery-Topics damit HA die Uhr automatisch erkennt
  - Datei: `IkeaObegraensad.ino` MQTT-Sektion

- [ ] **Effekt-Editor im Web-Interface**
  - Visueller Editor, Code-Generator, Echtzeit-Vorschau
  - Datei: Neue `EffectEditor.h`

- [ ] **Multi-Device-Synchronisation**
  - Mehrere Displays synchronisieren, Master/Slave-Modus
  - Datei: Neue `SyncManager.h`

### Code-Qualität
- [ ] **Unit-Tests**
  - Framework: ArduinoUnit oder PlatformIO Test
  - Tests für EEPROM-Funktionen, Validierungsfunktionen, Effect-Rendering
  - Datei: Neues `tests/`-Verzeichnis

- [ ] **Refactoring `IkeaObegraensad.ino`**
  - Datei ist ~2400 Zeilen → Handler in eigene Module auslagern
  - Design Patterns anwenden, Code-Duplikation reduzieren

- [ ] **PlatformIO Migration**
  - Besseres Dependency-Management, CI/CD
  - Datei: Neue `platformio.ini`

## 🔒 Sicherheit

- [x] **Input-Validierung TZ und MQTT-Topic** (erledigt in v1.6.2)
- [ ] **OTA-Passwort aus EEPROM**
  - Aktuell: Hardcoded in Code
  - Datei: `IkeaObegraensad.ino` `setup()`
- [ ] **API-Authentifizierung**
  - Token-basierte Auth für alle API-Endpoints
  - Datei: `IkeaObegraensad.ino` API-Handler
- [ ] **HTTPS Support**
  - Self-Signed Cert, MQTT TLS
  - Datei: Web-Server Setup

## 📊 Monitoring & Diagnose

- [ ] **Erweiterte Diagnose-Seite**
  - Heap-Visualisierung, Netzwerk-Statistiken, Performance-Metriken
  - Datei: Neue `Diagnostics.h`

- [ ] **Health-Checks**
  - Automatische Selbst-Diagnose, Warnungen bei Problemen
  - Datei: Neue `HealthCheck.h`

## 🎨 Neue Effekte

- [ ] **Text-Scroller** — Custom Text, RSS-Feed
- [ ] **Conway's Game of Life**
- [ ] **Mandelbrot-Set**
- [ ] **Partikel-Systeme**
- [ ] **Snake-Spiel (interaktiv)**

## 📝 Dokumentation

- [ ] **Benutzer-Handbuch** (`docs/USER_MANUAL.md`)
- [ ] **Entwickler-Dokumentation** — Effekt-Entwicklung-Guide, API-Referenz (`docs/DEVELOPER.md`)
- [ ] **CHANGELOG.md** mit automatischer Generierung via GitHub Actions

---

## Priorisierung

**Höchste Priorität:**
1. WiFi-Reconnect robuster (Stabilität)
2. Restore-Feature vervollständigen (Backup/Restore funktioniert aktuell nur halb)
3. Home Assistant Auto-Discovery (größter Nutzen für HA-Nutzer)

**Mittlere Priorität:**
1. Scheduler für Effekte
2. Lokale Sensoren direkt anschließen (SensorClock ohne HA-Abhängigkeit)
3. WebSocket für Live-Updates

**Niedrige Priorität:**
1. Plugin-System / Effekt-Editor
2. Multi-Device-Sync
3. Machine Learning Auto-Brightness

---

*Letzte Aktualisierung: 2026-04-10*
*Aktuelle Version: 1.6.2*
