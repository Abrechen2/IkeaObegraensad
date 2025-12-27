# Roadmap - IkeaObegraensad

Diese Roadmap enthält geplante Verbesserungen und Ideen für zukünftige Versionen.

## 🚀 Kurzfristig (v1.5.x)

### Code-Optimierung
- [ ] **Logging-Makros statt inline-Funktionen**
  - Problem: Bei deaktiviertem Logging werden Parameter trotzdem evaluiert
  - Lösung: Makros verwenden (`#define debugLog(...) ((void)0)`)
  - Vorteil: Kein unnötiger Code, bessere Performance
  - Datei: `Logging.h` Zeile 199-203

- [ ] **Zwei Build-Varianten in GitHub**
  - Variante 1: Mit erweitertem Logging (`DEBUG_LOGGING_ENABLED`)
  - Variante 2: Mit einfachem Serial-Logging (nur `Serial.println`)
  - Lösung: Conditional Compilation erweitern oder Git Branches mit Auto-Sync
  - Vorteil: Benutzer können Variante wählen

- [ ] **EEPROM-Migration verbessern**
  - Automatische Migration zwischen Versionen testen
  - Validierung der Checksumme robuster machen
  - Datei: `IkeaObegraensad.ino` EEPROM-Funktionen

### Stabilität
- [ ] **WiFi-Reconnect robuster**
  - Aktuell: Einfacher `WiFi.reconnect()` Aufruf
  - Verbesserung: Mehrere Versuche mit Backoff, Status-Überwachung
  - Datei: `IkeaObegraensad.ino` `setupWiFi()` und `loop()`

- [ ] **SPIFFS-Fehlerbehandlung verbessern**
  - Aktuell: Reinitialisierung alle 5 Minuten
  - Verbesserung: Bessere Fehlerdiagnose, alternative Strategien
  - Datei: `Logging.h` SPIFFS-Checks

- [ ] **Watchdog-Fütterung optimieren**
  - Aktuell: Viele manuelle `ESP.wdtFeed()` Aufrufe
  - Verbesserung: Automatisches Watchdog-Management in kritischen Funktionen
  - Datei: `IkeaObegraensad.ino` überall

## 📅 Mittelfristig (v1.6.x - v1.7.x)

### Neue Features
- [ ] **Mehrere Effekte gleichzeitig**
  - Effekte überblenden (z.B. Uhr + Wetter)
  - Effekt-Layering-System
  - Datei: Neue `EffectManager.h`

- [ ] **Wetter-Integration**
  - OpenWeatherMap API Integration
  - Temperatur/Icon auf Display
  - Konfigurierbar über Web-UI
  - Datei: Neue `Weather.h`, `WeatherEffect.h`

- [ ] **Scheduler für Effekte**
  - Automatischer Effekt-Wechsel nach Zeitplan
  - Beispiel: Uhr tagsüber, Effekte abends
  - Konfigurierbar über Web-UI
  - Datei: Neue `Scheduler.h`

- [ ] **Erweiterte MQTT-Integration**
  - Mehrere Topics unterstützen (nicht nur Präsenz)
  - Helligkeit über MQTT steuern
  - Effekt-Wechsel über MQTT
  - Home Assistant Auto-Discovery verbessern
  - Datei: `IkeaObegraensad.ino` MQTT-Sektion

- [ ] **WebSocket für Live-Updates**
  - Aktuell: Polling alle X Sekunden
  - Verbesserung: WebSocket für Echtzeit-Updates
  - Vorteil: Weniger Server-Last, bessere UX
  - Datei: `WebInterface.h` erweitern

### Benutzerfreundlichkeit
- [ ] **Web-UI modernisieren**
  - Aktuell: Basis-HTML
  - Verbesserung: Modernes Framework (z.B. Bootstrap, Tailwind)
  - Dark Mode
  - Responsive Design
  - Datei: `WebInterface.h` komplett überarbeiten

- [ ] **Effekt-Vorschau im Web-Interface**
  - Mini-Vorschau jedes Effekts
  - Screenshots/Animationen
  - Datei: `WebInterface.h`

- [ ] **Konfigurations-Assistent**
  - Schritt-für-Schritt Setup für neue Benutzer
  - WiFi, MQTT, NTP in einem Flow
  - Datei: Neue `SetupWizard.h`

- [ ] **Mehrsprachigkeit**
  - Deutsch/Englisch (und mehr)
  - Konfigurierbar über Web-UI
  - Datei: Neue `Translations.h`

### Performance
- [ ] **Frame-Rate-Optimierung**
  - Aktuell: 50ms pro Frame (20 FPS)
  - Ziel: 30ms pro Frame (33 FPS) für flüssigere Animationen
  - Datei: Alle `Effect.h` Dateien

- [ ] **Speicher-Optimierung**
  - String-Operationen weiter reduzieren
  - Statische Buffers wiederverwenden
  - Heap-Fragmentierung minimieren
  - Datei: `IkeaObegraensad.ino` überall

- [ ] **EEPROM-Komprimierung**
  - Aktuell: 1024 Bytes EEPROM
  - Verbesserung: Komprimierung für mehr Daten
  - Datei: EEPROM-Funktionen

## 🔮 Langfristig (v2.0.x+)

### Hardware-Erweiterungen
- [ ] **Mehrere Buttons**
  - Aktuell: Ein Button (D4) für Effekt-Wechsel
  - Erweiterung: Mehrere Buttons für verschiedene Funktionen
  - Beispiel: Vor/Zurück, Helligkeit +/-, Menü
  - Datei: Neue `ButtonManager.h`

- [ ] **Zusätzliche Sensoren**
  - Temperatur-Sensor (DS18B20)
  - Luftfeuchtigkeit (DHT22)
  - Bewegungssensor (PIR)
  - Datei: Neue `Sensors.h`

- [ ] **SD-Karte Support**
  - Für größere Logs
  - Custom Effekte speichern
  - Backup auf SD-Karte
  - Datei: Neue `SDCard.h`

### Erweiterte Features
- [ ] **Effekt-Editor im Web-Interface**
  - Visueller Editor für eigene Effekte
  - Code-Generator
  - Vorschau in Echtzeit
  - Datei: Neue `EffectEditor.h`

- [ ] **Plugin-System**
  - Dynamisches Laden von Effekten
  - Community-Effekte
  - Plugin-Repository
  - Datei: Neue `PluginSystem.h`

- [ ] **Multi-Device-Synchronisation**
  - Mehrere Displays synchronisieren
  - Master/Slave-Modus
  - Datei: Neue `SyncManager.h`

- [ ] **Machine Learning für Auto-Brightness**
  - Lernen von Benutzer-Präferenzen
  - Adaptive Helligkeits-Kurven
  - Datei: `updateAutoBrightness()` erweitern

### Code-Qualität
- [ ] **Unit-Tests**
  - Framework: ArduinoUnit oder PlatformIO Test
  - Tests für kritische Funktionen (EEPROM, Logging)
  - CI/CD Integration
  - Datei: Neue `tests/` Verzeichnis

- [ ] **Code-Dokumentation**
  - Doxygen-Kommentare
  - API-Dokumentation
  - Architektur-Diagramme
  - Datei: Überall

- [ ] **Refactoring**
  - Große Funktionen aufteilen
  - Design Patterns anwenden
  - Code-Duplikation reduzieren
  - Datei: `IkeaObegraensad.ino` (sehr groß!)

- [ ] **PlatformIO Migration**
  - Aktuell: Arduino IDE
  - Vorteil: Bessere Dependency-Management, CI/CD
  - Datei: Neue `platformio.ini`

## 🔒 Sicherheit

- [ ] **OTA-Passwort aus EEPROM**
  - Aktuell: Hardcoded in Code
  - Verbesserung: Konfigurierbar über Web-UI
  - Datei: `IkeaObegraensad.ino` `setup()`

- [ ] **API-Authentifizierung**
  - Aktuell: Keine Authentifizierung
  - Verbesserung: Token-basierte Auth
  - Datei: `IkeaObegraensad.ino` API-Handler

- [ ] **HTTPS Support**
  - Aktuell: Nur HTTP
  - Verbesserung: HTTPS mit Self-Signed Cert
  - Datei: Web-Server Setup

- [ ] **Input-Validierung erweitern**
  - Aktuell: Basis-Validierung
  - Verbesserung: Sanitization, SQL-Injection-Schutz
  - Datei: Alle API-Handler

## 📊 Monitoring & Diagnose

- [ ] **Erweiterte Diagnose-Seite**
  - Heap-Visualisierung
  - Netzwerk-Statistiken
  - Performance-Metriken
  - Datei: Neue `Diagnostics.h`

- [ ] **Remote-Logging verbessern**
  - Aktuell: HTTP POST zu Server
  - Verbesserung: MQTT-Logging, Syslog
  - Datei: `Logging.h`

- [ ] **Health-Checks**
  - Automatische Selbst-Diagnose
  - Warnungen bei Problemen
  - Datei: Neue `HealthCheck.h`

## 🎨 Neue Effekte

- [ ] **Text-Scroller**
  - Custom Text anzeigen
  - RSS-Feed Integration
  - Datei: Neue `TextScroll.h`

- [ ] **Visualisierer**
  - Audio-Visualisierung (wenn Audio-Input verfügbar)
  - FFT-basierte Effekte
  - Datei: Neue `Visualizer.h`

- [ ] **Spiele**
  - Snake-Spiel (interaktiv)
  - Pong
  - Tetris
  - Datei: Neue `Games/` Verzeichnis

- [ ] **Kunst-Effekte**
  - Mandelbrot-Set
  - Conway's Game of Life
  - Partikel-Systeme
  - Datei: Neue Effekt-Dateien

## 📝 Dokumentation

- [ ] **Benutzer-Handbuch**
  - Schritt-für-Schritt Anleitung
  - Troubleshooting-Guide
  - FAQ
  - Datei: `docs/USER_MANUAL.md`

- [ ] **Entwickler-Dokumentation**
  - API-Referenz
  - Effekt-Entwicklung-Guide
  - Architektur-Übersicht
  - Datei: `docs/DEVELOPER.md`

- [ ] **Video-Tutorials**
  - Setup-Anleitung
  - Effekt-Erstellung
  - MQTT-Konfiguration

## 🔧 Wartbarkeit

- [ ] **Versionierung verbessern**
  - Semantic Versioning strikt einhalten
  - Changelog automatisch generieren
  - Datei: `CHANGELOG.md`

- [ ] **GitHub Actions**
  - Automatische Builds
  - Tests ausführen
  - Releases erstellen
  - Datei: `.github/workflows/`

- [ ] **Code-Formatierung**
  - Clang-Format Konfiguration
  - Automatische Formatierung bei Commit
  - Datei: `.clang-format`

---

## Priorisierung

**Höchste Priorität:**
1. Logging-Makros (Performance)
2. Zwei Build-Varianten (Benutzerfreundlichkeit)
3. WiFi-Reconnect robuster (Stabilität)

**Mittlere Priorität:**
1. Web-UI modernisieren
2. Wetter-Integration
3. Erweiterte MQTT-Integration

**Niedrige Priorität:**
1. Plugin-System
2. Machine Learning
3. Multi-Device-Sync

---

*Letzte Aktualisierung: 2024*
*Version: 1.4.1*