#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include <FS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <string.h>

// Externe Variablen (werden in IkeaObegraensad.ino definiert)
extern char logServerUrl[];
extern unsigned long logServerInterval;
extern unsigned long lastLogUpload;
extern WiFiClient espClient;

// Externe Variablen für Restart-Diagnose (werden in IkeaObegraensad.ino definiert)
extern unsigned long lastUptimeBeforeRestart;
extern uint32_t lastHeapBeforeRestart;
extern char lastOperation[];

// Funktion zum automatischen Speichern von Restart-Logs (immer aktiv)
// Diese Funktion wird bei jedem Systemstart automatisch aufgerufen
void logRestart() {
  if (!SPIFFS.exists("/")) {
    return; // SPIFFS nicht verfügbar
  }
  
  String resetReason = ESP.getResetReason();
  int freeHeap = ESP.getFreeHeap();
  int maxFreeBlock = ESP.getMaxFreeBlockSize();
  unsigned long uptime = millis();
  
  // Uptime und Heap vor Restart aus externen Variablen (werden in setup() geladen)
  unsigned long uptimeBeforeRestart = lastUptimeBeforeRestart;
  uint32_t heapBeforeRestart = lastHeapBeforeRestart;
  
  File logFile = SPIFFS.open("/debug.log", "a");
  if (logFile) {
    // Erweitertes Restart-Log mit Uptime/Heap vor Restart
    logFile.printf("{\"id\":\"restart_%lu\",\"timestamp\":%lu,\"location\":\"setup\",\"message\":\"System restart detected\",\"data\":{\"resetReason\":\"%s\",\"freeHeap\":%d,\"maxFreeBlock\":%d,\"uptime\":%lu,\"uptimeBeforeRestart\":%lu,\"heapBeforeRestart\":%u,\"lastOperation\":\"%s\"},\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\"}\n",
                   uptime, uptime, resetReason.c_str(), freeHeap, maxFreeBlock, uptime, 
                   uptimeBeforeRestart, heapBeforeRestart, lastOperation);
    logFile.close();
  }
}

#ifdef DEBUG_LOGGING_ENABLED
// Debug-Logging Konstanten
const size_t MAX_LOG_SIZE = 32768; // 32KB maximale Log-Größe
const size_t LOG_ROTATE_SIZE = 16384; // Bei 16KB rotieren (ältere Hälfte löschen)

// Debug-Logging Funktion mit Rotation
void debugLog(const char* location, const char* message, const char* hypothesisId, const char* dataJson) {
  unsigned long now = millis();
  int freeHeap = ESP.getFreeHeap();
  
  // Prüfe ob SPIFFS verfügbar ist
  // Verwende statische Variablen, um den Status zu cachen und nicht bei jedem Aufruf zu prüfen
  static bool spiffsAvailable = false;
  static unsigned long lastCheck = 0;
  static unsigned long lastReinitAttempt = 0;
  static bool firstCheck = true;
  
  // Beim ersten Aufruf sofort prüfen, danach nur alle 10 Sekunden
  if (firstCheck || (now - lastCheck > 10000)) {
    if (firstCheck) {
      firstCheck = false;
    }
    lastCheck = now;
    
    // Teste ob SPIFFS funktioniert, indem wir versuchen, eine Testdatei zu erstellen
    File testFile = SPIFFS.open("/.spiffstest", "w");
    if (testFile) {
      testFile.close();
      SPIFFS.remove("/.spiffstest");
      spiffsAvailable = true;
    } else {
      // SPIFFS funktioniert nicht - versuche Reinitialisierung alle 5 Minuten
      spiffsAvailable = false;
      if (now - lastReinitAttempt > 300000 || lastReinitAttempt == 0) {
        Serial.println("[DEBUG] SPIFFS nicht verfügbar, versuche Reinitialisierung...");
        SPIFFS.end();
        delay(50);
        bool reinitOk = SPIFFS.begin();
        if (reinitOk) {
          // Teste erneut
          File testFile2 = SPIFFS.open("/.spiffstest", "w");
          if (testFile2) {
            testFile2.close();
            SPIFFS.remove("/.spiffstest");
            Serial.println("[DEBUG] SPIFFS erfolgreich reinitialisiert!");
            spiffsAvailable = true;
            lastReinitAttempt = 0; // Reset
          } else {
            Serial.println("[DEBUG] SPIFFS Reinitialisierung fehlgeschlagen (kann keine Datei erstellen)");
            lastReinitAttempt = now;
          }
        } else {
          Serial.println("[DEBUG] SPIFFS Reinitialisierung fehlgeschlagen (begin() failed)");
          lastReinitAttempt = now;
        }
      }
    }
  }
  
  // Wenn SPIFFS nicht verfügbar ist, nur Serial ausgeben
  if (!spiffsAvailable) {
    Serial.printf("[DEBUG] %s:%s [%s] %s freeHeap=%d\n", location, message, hypothesisId, dataJson ? dataJson : "{}", freeHeap);
    // Warnung alle 60 Sekunden ausgeben
    static unsigned long lastWarning = 0;
    if (now - lastWarning > 60000 || lastWarning == 0) {
      Serial.println("[DEBUG] WARNING: SPIFFS nicht verfügbar - Logs werden nicht in Datei geschrieben!");
      lastWarning = now;
    }
    return;
  }
  
  // Prüfe Log-Dateigröße und rotiere bei Bedarf
  if (SPIFFS.exists("/debug.log")) {
    File logCheck = SPIFFS.open("/debug.log", "r");
    if (logCheck) {
      size_t logSize = logCheck.size();
      logCheck.close();
      
      if (logSize > LOG_ROTATE_SIZE) {
        // Log-Rotation: Lese Datei, behalte nur die zweite Hälfte
        File logRead = SPIFFS.open("/debug.log", "r");
        if (logRead) {
          // Springe zur Mitte der Datei
          logRead.seek(logSize / 2, SeekSet);
          
          // Lese ab der Mitte und schreibe in temporäre Datei
          File logTemp = SPIFFS.open("/debug.tmp", "w");
          if (logTemp) {
            while (logRead.available()) {
              logTemp.write(logRead.read());
            }
            logTemp.close();
          }
          logRead.close();
          
          // Ersetze alte Datei durch rotierte Version
          SPIFFS.remove("/debug.log");
          if (SPIFFS.exists("/debug.tmp")) {
            File logOld = SPIFFS.open("/debug.tmp", "r");
            File logNew = SPIFFS.open("/debug.log", "w");
            if (logOld && logNew) {
              while (logOld.available()) {
                logNew.write(logOld.read());
              }
            }
            if (logOld) logOld.close();
            if (logNew) logNew.close();
            SPIFFS.remove("/debug.tmp");
          }
        }
      }
    }
  }
  
  // Prüfe verfügbaren SPIFFS-Speicher
  FSInfo fs_info;
  if (SPIFFS.info(fs_info)) {
    if (fs_info.totalBytes - fs_info.usedBytes < 1024) {
      // Weniger als 1KB frei, lösche Log komplett
      SPIFFS.remove("/debug.log");
      Serial.println("[DEBUG] SPIFFS fast voll, Log gelöscht");
    }
  }
  
  // Log in Datei schreiben
  File logFile = SPIFFS.open("/debug.log", "a");
  if (logFile) {
    logFile.printf("{\"id\":\"log_%lu\",\"timestamp\":%lu,\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"%s\",\"freeHeap\":%d,\"uptime\":%lu}\n",
                   now, now, location, message, dataJson ? dataJson : "{}", hypothesisId, freeHeap, now);
    logFile.close();
  } else {
    // Fehler beim Öffnen der Datei - nur gelegentlich ausgeben
    static unsigned long lastError = 0;
    if (now - lastError > 60000 || lastError == 0) {
      Serial.printf("[DEBUG] ERROR: Konnte /debug.log nicht öffnen zum Schreiben! (Heap: %d)\n", freeHeap);
      lastError = now;
    }
  }
  
  // Auch über Serial ausgeben (für sofortige Sichtbarkeit)
  Serial.printf("[DEBUG] %s:%s [%s] %s freeHeap=%d\n", location, message, hypothesisId, dataJson ? dataJson : "{}", freeHeap);
}

// Hilfsfunktion für JSON-String-Erstellung
void debugLogJson(const char* location, const char* message, const char* hypothesisId, const char* format, ...) {
  char dataJson[256];
  va_list args;
  va_start(args, format);
  vsnprintf(dataJson, sizeof(dataJson), format, args);
  va_end(args);
  debugLog(location, message, hypothesisId, dataJson);
}
#else
// Stub-Funktionen wenn Debug-Logging deaktiviert
inline void debugLog(const char* location, const char* message, const char* hypothesisId, const char* dataJson) {}
inline void debugLogJson(const char* location, const char* message, const char* hypothesisId, const char* format, ...) {}
#endif // DEBUG_LOGGING_ENABLED

// Funktion zum Senden von Logs an einen Server
// Sendet alle Logs aus /debug.log als JSON-Array per HTTP POST
// Löscht die Datei nach erfolgreichem Senden (HTTP 200-299)
// Konfiguration: Definiere LOG_SERVER_URL in secrets.h oder über Web-UI
// Deaktiviert wenn LOG_SERVER_URL leer oder nicht definiert ist
bool sendLogsToServer() {
  // Prüfe ob WiFi verbunden ist
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOG_SERVER] WiFi nicht verbunden");
    return false;
  }
  
  // Prüfe ob URL gesetzt und gültig ist (muss mit http:// oder https:// beginnen)
  if (strlen(logServerUrl) == 0 || 
      (strncmp(logServerUrl, "http://", 7) != 0 && strncmp(logServerUrl, "https://", 8) != 0)) {
    Serial.println("[LOG_SERVER] URL nicht gesetzt oder ungültig");
    return false;
  }
  
  Serial.printf("[LOG_SERVER] Versuche Logs zu senden an: %s\n", logServerUrl);
  
  // Prüfe ob Log-Datei existiert
  if (!SPIFFS.exists("/debug.log")) {
    Serial.println("[LOG_SERVER] Keine Log-Datei vorhanden");
    return true; // Keine Logs zum Senden ist kein Fehler
  }
  
  File logFile = SPIFFS.open("/debug.log", "r");
  if (!logFile) {
    Serial.println("[LOG_SERVER] Fehler beim Öffnen der Log-Datei");
    return false;
  }
  
  // Prüfe Dateigröße (max 32KB zum Senden auf einmal)
  size_t fileSize = logFile.size();
  if (fileSize == 0) {
    logFile.close();
    Serial.println("[LOG_SERVER] Log-Datei ist leer");
    return true; // Leere Datei ist kein Fehler
  }
  
  if (fileSize > 32768) {
    Serial.printf("[LOG_SERVER] Log-Datei zu groß (%d bytes), überspringe Sendung\n", fileSize);
    logFile.close();
    return false;
  }
  
  Serial.printf("[LOG_SERVER] Lese Log-Datei (%d bytes)...\n", fileSize);
  
  // Lese gesamte Datei und konvertiere NDJSON zu JSON-Array
  // Verwende reserve() um Speicher-Allokationen zu reduzieren
  String payload;
  payload.reserve(fileSize + 100); // Reserve Speicher für Array-Klammern und Kommas
  payload = "[";
  bool firstLine = true;
  uint16_t lineCount = 0;
  
  while (logFile.available()) {
    String line = logFile.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      // Prüfe ob die Zeile gültiges JSON ist (sollte mit { beginnen)
      if (line.charAt(0) == '{') {
        if (!firstLine) {
          payload += ",";
        }
        payload += line;
        firstLine = false;
        lineCount++;
      } else {
        Serial.printf("[LOG_SERVER] WARNUNG: Ungültige Zeile übersprungen (beginnt nicht mit '{'): %s\n", line.substring(0, min(50, (int)line.length())).c_str());
      }
      yield(); // Wichtig: yield für Watchdog
    }
  }
  payload += "]";
  logFile.close();
  
  Serial.printf("[LOG_SERVER] JSON-Array erstellt (%d bytes, %d Zeilen), sende an Server...\n", payload.length(), lineCount);
  
  // Debug: Zeige ersten Teil des Payloads
  if (payload.length() > 0) {
    String preview = payload.substring(0, min(100, (int)payload.length()));
    Serial.printf("[LOG_SERVER] Payload-Start: %s\n", preview.c_str());
  }
  
  // Erstelle HTTP-Client und sende
  HTTPClient http;
  http.begin(espClient, logServerUrl);
  http.addHeader("Content-Type", "application/json"); // JSON-Format statt NDJSON
  http.setTimeout(15000); // 15 Sekunden Timeout (für Connect und Read)
  
  ESP.wdtFeed(); // Watchdog vor blockierender Operation
  int httpCode = http.POST(payload);
  ESP.wdtFeed(); // Watchdog nach blockierender Operation
  
  // Lese Antwort vom Server (für Debugging)
  String response = http.getString();
  
  http.end();
  
  // Prüfe Antwort-Code
  if (httpCode >= 200 && httpCode < 300) {
    // Erfolgreich gesendet - lösche Log-Datei
    SPIFFS.remove("/debug.log");
    Serial.printf("[LOG_SERVER] Logs erfolgreich gesendet (%d bytes, HTTP %d)\n", fileSize, httpCode);
    return true;
  } else {
    Serial.printf("[LOG_SERVER] Fehler beim Senden: HTTP %d\n", httpCode);
    if (response.length() > 0) {
      Serial.printf("[LOG_SERVER] Server-Antwort: %s\n", response.c_str());
    }
    // Zeige ersten Teil des Payloads für Debugging (max 200 Zeichen)
    if (payload.length() > 0) {
      String payloadPreview = payload.substring(0, min(200, (int)payload.length()));
      Serial.printf("[LOG_SERVER] Payload-Vorschau (erste 200 Zeichen): %s\n", payloadPreview.c_str());
    }
    return false;
  }
}

#endif // LOGGING_H

