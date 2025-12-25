# Anleitung: Home Assistant Integration für Ikea Obegraensad erstellen

Diese Anleitung beschreibt genau, was in einem neuen GitHub-Repository für die Home Assistant Custom Integration erstellt werden muss. Eine KI kann diese Anleitung nutzen, um die komplette Integration zu generieren.

## Repository-Struktur

Das neue Repository sollte folgende Struktur haben:

```
ikea-obegraensad-homeassistant/
├── custom_components/
│   └── ikea_obegraensad/
│       ├── __init__.py
│       ├── manifest.json
│       ├── config_flow.py
│       ├── switch.py
│       ├── const.py
│       └── strings.json
├── .github/
│   └── workflows/
│       └── validate.yml (optional)
├── hacs.json
├── README.md
├── LICENSE
└── .gitignore
```

## Dateien und deren Inhalt

### 1. `hacs.json` (Repository-Root)

```json
{
  "name": "Ikea Obegraensad",
  "domains": ["switch"],
  "homeassistant": "2023.1.0",
  "iot_class": "Local Polling",
  "render_readme": true
}
```

**Zweck**: HACS-Metadaten für die Integration.

**Wichtig**: 
- `domains` kann später erweitert werden (z.B. `["switch", "sensor", "light"]`)
- `homeassistant` gibt die minimale HA-Version an

---

### 2. `custom_components/ikea_obegraensad/manifest.json`

```json
{
  "domain": "ikea_obegraensad",
  "name": "Ikea Obegraensad",
  "version": "1.0.0",
  "documentation": "https://github.com/DEIN_USERNAME/ikea-obegraensad-homeassistant",
  "issue_tracker": "https://github.com/DEIN_USERNAME/ikea-obegraensad-homeassistant/issues",
  "codeowners": ["@DEIN_USERNAME"],
  "requirements": ["aiohttp>=3.8.0"],
  "config_flow": true,
  "dependencies": [],
  "iot_class": "Local Polling"
}
```

**Zweck**: Integration-Metadaten für Home Assistant.

**Anpassungen nötig**:
- `DEIN_USERNAME` durch den tatsächlichen GitHub-Username ersetzen
- `version` bei Updates erhöhen

---

### 3. `custom_components/ikea_obegraensad/const.py`

```python
"""Constants for the Ikea Obegraensad integration."""
from typing import Final

DOMAIN: Final = "ikea_obegraensad"

# Default values
DEFAULT_PORT: Final = 80
DEFAULT_TIMEOUT: Final = 5
DEFAULT_SCAN_INTERVAL: Final = 30

# API endpoints
API_STATUS: Final = "/api/status"
API_SET_DISPLAY: Final = "/api/setDisplay"

# Configuration keys
CONF_HOST: Final = "host"
CONF_PORT: Final = "port"
CONF_NAME: Final = "name"
```

**Zweck**: Konstanten für die Integration.

**Hinweise**:
- `DEFAULT_SCAN_INTERVAL` = 30 Sekunden Polling-Intervall
- API-Endpoints müssen mit der Arduino-Implementierung übereinstimmen

---

### 4. `custom_components/ikea_obegraensad/__init__.py`

```python
"""The Ikea Obegraensad integration."""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_HOST, CONF_PORT, Platform
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import ConfigEntryNotReady
import aiohttp

from .const import DOMAIN, DEFAULT_PORT, DEFAULT_TIMEOUT
from .coordinator import IkeaObegraensadDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)

PLATFORMS: list[Platform] = [Platform.SWITCH]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Ikea Obegraensad from a config entry."""
    host = entry.data[CONF_HOST]
    port = entry.data.get(CONF_PORT, DEFAULT_PORT)
    
    coordinator = IkeaObegraensadDataUpdateCoordinator(hass, host, port)
    
    try:
        await coordinator.async_config_entry_first_refresh()
    except Exception as err:
        raise ConfigEntryNotReady(f"Error connecting to device: {err}") from err
    
    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = coordinator
    
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        hass.data[DOMAIN].pop(entry.entry_id)
    
    return unload_ok
```

**Zweck**: Haupt-Setup-Logik der Integration.

**Wichtig**: 
- Nutzt einen Coordinator für Daten-Updates (siehe nächste Datei)
- `PLATFORMS` Liste kann erweitert werden (z.B. `[Platform.SWITCH, Platform.SENSOR]`)

---

### 5. `custom_components/ikea_obegraensad/coordinator.py` (NEU erstellen)

```python
"""DataUpdateCoordinator for Ikea Obegraensad."""
from __future__ import annotations

import logging
from datetime import timedelta
from typing import Any

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed
import aiohttp

from .const import API_STATUS, DEFAULT_TIMEOUT, DEFAULT_SCAN_INTERVAL

_LOGGER = logging.getLogger(__name__)


class IkeaObegraensadDataUpdateCoordinator(DataUpdateCoordinator):
    """Class to manage fetching data from the Ikea Obegraensad device."""

    def __init__(self, hass: HomeAssistant, host: str, port: int) -> None:
        """Initialize the coordinator."""
        super().__init__(
            hass,
            _LOGGER,
            name="Ikea Obegraensad",
            update_interval=timedelta(seconds=DEFAULT_SCAN_INTERVAL),
        )
        self.host = host
        self.port = port
        self.base_url = f"http://{host}:{port}"

    async def _async_update_data(self) -> dict[str, Any]:
        """Fetch data from the device."""
        try:
            async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=DEFAULT_TIMEOUT)) as session:
                async with session.get(f"{self.base_url}{API_STATUS}") as response:
                    if response.status == 200:
                        data = await response.json()
                        return data
                    else:
                        raise UpdateFailed(f"HTTP {response.status}: {response.reason}")
        except aiohttp.ClientError as err:
            raise UpdateFailed(f"Error communicating with device: {err}") from err
        except Exception as err:
            raise UpdateFailed(f"Unexpected error: {err}") from err

    async def async_set_display(self, enabled: bool) -> bool:
        """Set display on/off."""
        try:
            async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=DEFAULT_TIMEOUT)) as session:
                async with session.get(
                    f"{self.base_url}/api/setDisplay",
                    params={"enabled": "true" if enabled else "false"}
                ) as response:
                    if response.status == 200:
                        await self.async_request_refresh()
                        return True
                    else:
                        _LOGGER.error(f"Failed to set display: HTTP {response.status}")
                        return False
        except Exception as err:
            _LOGGER.error(f"Error setting display: {err}")
            return False
```

**Zweck**: Koordiniert Daten-Updates und API-Aufrufe.

**Funktionen**:
- `_async_update_data()`: Ruft `/api/status` auf und cached die Daten
- `async_set_display()`: Setzt Display Ein/Aus über `/api/setDisplay`

---

### 6. `custom_components/ikea_obegraensad/config_flow.py`

```python
"""Config flow for Ikea Obegraensad integration."""
from __future__ import annotations

import logging
from typing import Any

import aiohttp
import voluptuous as vol

from homeassistant import config_entries
from homeassistant.const import CONF_HOST, CONF_PORT, CONF_NAME
from homeassistant.data_entry_flow import FlowResult
from homeassistant.exceptions import HomeAssistantError
import homeassistant.helpers.config_validation as cv

from .const import DOMAIN, DEFAULT_PORT, DEFAULT_TIMEOUT, API_STATUS

_LOGGER = logging.getLogger(__name__)

STEP_USER_DATA_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_HOST): str,
        vol.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        vol.Optional(CONF_NAME, default="Ikea Clock"): str,
    }
)


async def validate_input(data: dict[str, Any]) -> dict[str, Any]:
    """Validate the user input allows us to connect."""
    host = data[CONF_HOST]
    port = data.get(CONF_PORT, DEFAULT_PORT)
    
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=DEFAULT_TIMEOUT)) as session:
        try:
            async with session.get(f"http://{host}:{port}{API_STATUS}") as response:
                if response.status != 200:
                    raise CannotConnect
                result = await response.json()
                return {"title": data.get(CONF_NAME, "Ikea Clock"), "device_info": result}
        except aiohttp.ClientError as err:
            raise CannotConnect from err


class ConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Ikea Obegraensad."""

    VERSION = 1

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """Handle the initial step."""
        if user_input is None:
            return self.async_show_form(
                step_id="user", data_schema=STEP_USER_DATA_SCHEMA
            )

        errors = {}

        try:
            info = await validate_input(user_input)
        except CannotConnect:
            errors["base"] = "cannot_connect"
        except Exception:
            _LOGGER.exception("Unexpected exception")
            errors["base"] = "unknown"
        else:
            await self.async_set_unique_id(f"{user_input[CONF_HOST]}:{user_input.get(CONF_PORT, DEFAULT_PORT)}")
            self._abort_if_unique_id_configured()
            return self.async_create_entry(title=info["title"], data=user_input)

        return self.async_show_form(
            step_id="user", data_schema=STEP_USER_DATA_SCHEMA, errors=errors
        )


class CannotConnect(HomeAssistantError):
    """Error to indicate we cannot connect."""
```

**Zweck**: Setup-Assistent für die Integration.

**Funktionen**:
- `validate_input()`: Prüft ob das Gerät erreichbar ist
- `async_step_user()`: Zeigt Eingabeformular und speichert Konfiguration

---

### 7. `custom_components/ikea_obegraensad/switch.py`

```python
"""Switch platform for Ikea Obegraensad integration."""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import IkeaObegraensadDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the switch platform."""
    coordinator: IkeaObegraensadDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]
    
    async_add_entities([IkeaObegraensadDisplaySwitch(coordinator, entry)])


class IkeaObegraensadDisplaySwitch(CoordinatorEntity, SwitchEntity):
    """Representation of a Display Switch."""

    def __init__(
        self,
        coordinator: IkeaObegraensadDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the switch."""
        super().__init__(coordinator)
        self._attr_unique_id = f"{entry.entry_id}_display"
        self._attr_name = f"{entry.data.get('name', 'Ikea Clock')} Display"
        self._attr_icon = "mdi:led-on"

    @property
    def is_on(self) -> bool | None:
        """Return true if the display is on."""
        if self.coordinator.data is None:
            return None
        return self.coordinator.data.get("displayEnabled", False)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on the display."""
        success = await self.coordinator.async_set_display(True)
        if not success:
            _LOGGER.error("Failed to turn on display")

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the display."""
        success = await self.coordinator.async_set_display(False)
        if not success:
            _LOGGER.error("Failed to turn off display")
```

**Zweck**: Switch-Entity für Display Ein/Aus.

**Funktionen**:
- `is_on`: Liest `displayEnabled` aus Coordinator-Daten
- `async_turn_on/off`: Ruft Coordinator-Methode zum Setzen auf

---

### 8. `custom_components/ikea_obegraensad/strings.json` (Optional)

```json
{
  "config": {
    "step": {
      "user": {
        "title": "Ikea Obegraensad einrichten",
        "description": "Bitte gib die IP-Adresse oder den Hostnamen deines Geräts ein.",
        "data": {
          "host": "Host / IP-Adresse",
          "port": "Port",
          "name": "Name"
        }
      }
    },
    "error": {
      "cannot_connect": "Konnte nicht mit dem Gerät verbinden. Bitte prüfe die IP-Adresse und ob das Gerät eingeschaltet ist.",
      "unknown": "Ein unbekannter Fehler ist aufgetreten."
    },
    "abort": {
      "already_configured": "Dieses Gerät ist bereits konfiguriert."
    }
  }
}
```

**Zweck**: Übersetzungen für die Konfigurations-UI.

---

### 9. `README.md` (Repository-Root)

```markdown
# Home Assistant Integration für Ikea Obegraensad

[![hacs_badge](https://img.shields.io/badge/HACS-Custom-orange.svg)](https://github.com/custom-components/hacs)

Home Assistant Integration für die Ikea Obegraensad LED Matrix Uhr.

## Features

- **Display Ein/Aus**: Steuere das Display über Home Assistant
- **Automatisierungen**: Nutze das Display in HA-Automatisierungen
- **Parallel zu MQTT**: Funktioniert parallel zur bestehenden MQTT-Präsenz-Integration

## Installation

### Über HACS (Empfohlen)

1. HACS öffnen
2. Integrations → Drei-Punkte-Menü → "Custom repositories"
3. Repository URL: `https://github.com/DEIN_USERNAME/ikea-obegraensad-homeassistant`
4. Kategorie: "Integration"
5. "Add" klicken
6. Integration installieren
7. Home Assistant neu starten

### Manuelle Installation

1. Kopiere den `custom_components/ikea_obegraensad/` Ordner in dein `config/custom_components/` Verzeichnis
2. Home Assistant neu starten
3. Gehe zu Settings → Devices & Services → "+" → "Ikea Obegraensad"

## Konfiguration

1. Öffne Home Assistant
2. Gehe zu Settings → Devices & Services
3. Klicke auf "+ ADD INTEGRATION"
4. Suche nach "Ikea Obegraensad"
5. Gib die IP-Adresse deines Geräts ein (z.B. `192.168.1.100`)
6. Optional: Port anpassen (Standard: 80)
7. Optional: Name vergeben

## Verwendung

Nach der Installation erscheint ein Switch mit dem Namen "Display" in Home Assistant. Dieser kann zum Ein- und Ausschalten des Displays verwendet werden.

### Automatisierung Beispiel

```yaml
automation:
  - alias: "Wohnzimmer Licht an → Display an"
    trigger:
      - platform: state
        entity_id: light.wohnzimmer
        to: "on"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.ikea_clock_display
```

## API-Voraussetzungen

Das Arduino-Gerät muss folgende API-Endpoints unterstützen:

- `GET /api/status` - Gibt JSON mit `displayEnabled` zurück
- `GET /api/setDisplay?enabled=true|false` - Setzt Display Ein/Aus

## Voraussetzungen

- Home Assistant 2023.1.0 oder höher
- Ikea Obegraensad Gerät mit Firmware, die `/api/setDisplay` unterstützt

## Bekannte Probleme / Einschränkungen

- Die Integration nutzt Polling (alle 30 Sekunden) für Status-Updates
- Display-Steuerung funktioniert parallel zur MQTT-Präsenz-Integration

## Unterstützung

Bei Problemen bitte ein Issue auf GitHub erstellen.
```

**Anpassungen nötig**:
- `DEIN_USERNAME` durch den tatsächlichen GitHub-Username ersetzen

---

### 10. `LICENSE`

Kopiere die MIT-Lizenz aus dem Arduino-Projekt oder verwende die gleiche Lizenz.

---

### 11. `.gitignore`

```gitignore
# Python
__pycache__/
*.py[cod]
*$py.class
*.so
.Python
env/
venv/
ENV/
build/
develop-eggs/
dist/
downloads/
eggs/
.eggs/
lib/
lib64/
parts/
sdist/
var/
wheels/
*.egg-info/
.installed.cfg
*.egg

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db
```

---

## Arduino-Seite: Benötigte Änderungen

**WICHTIG**: Bevor die HA-Integration funktioniert, muss die Arduino-Firmware erweitert werden!

### Neuer API-Endpoint hinzufügen

In `IkeaObegraensad.ino` muss folgender Handler hinzugefügt werden:

```cpp
void handleSetDisplay() {
  if (!checkRateLimit()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  if (server.hasArg("enabled")) {
    String val = server.arg("enabled");
    bool newState = (val == "true" || val == "1");
    
    displayEnabled = newState;
    
    if (displayEnabled) {
      analogWrite(PIN_ENABLE, 1023 - brightness);
      Serial.println("Display ENABLED via API");
    } else {
      analogWrite(PIN_ENABLE, 1023);
      Serial.println("Display DISABLED via API");
    }
    
    server.send(200, "application/json", 
                String("{\"displayEnabled\":") + (displayEnabled ? "true" : "false") + "}");
  } else {
    server.send(400, "text/plain", "Missing enabled parameter");
  }
}
```

### Endpoint registrieren

In der `setup()` Funktion, nach den anderen `server.on()` Aufrufen:

```cpp
server.on("/api/setDisplay", handleSetDisplay);
```

### Logik-Anpassung (Optional, für bessere Integration)

Damit HA die Präsenz-Logik überschreiben kann, kann ein Flag hinzugefügt werden:

```cpp
bool haDisplayControlled = false;  // Wird gesetzt wenn HA das Display steuert
```

In `handleSetDisplay()`:

```cpp
haDisplayControlled = true;  // HA hat das Display gesteuert
```

In `updatePresenceTimeout()`:

```cpp
// Wenn HA das Display steuert, Präsenz-Logik überspringen
if (haDisplayControlled) {
  return;  // HA hat Kontrolle, Präsenz ignoriert
}
```

---

## Zusammenfassung für KI

Eine KI sollte folgendes tun:

1. **Repository erstellen**: Neues GitHub-Repository `ikea-obegraensad-homeassistant` erstellen

2. **Verzeichnisstruktur anlegen**: Alle oben genannten Ordner und Dateien erstellen

3. **Dateien befüllen**: Jede Datei mit dem entsprechenden Inhalt aus dieser Anleitung befüllen

4. **Anpassungen vornehmen**:
   - In `manifest.json`: GitHub-Username ersetzen
   - In `README.md`: GitHub-Username ersetzen
   - In `strings.json`: Optional deutsche Übersetzungen hinzufügen

5. **LICENSE hinzufügen**: MIT-Lizenz kopieren

6. **Erstes Release erstellen**: GitHub Release `v1.0.0` erstellen

7. **Arduino-Code aktualisieren**: Die genannten Änderungen in `IkeaObegraensad.ino` hinzufügen

**Wichtig für die KI**: 
- Alle Pfade müssen exakt sein (z.B. `custom_components/ikea_obegraensad/__init__.py`)
- Python-Code muss syntaktisch korrekt sein
- JSON-Dateien müssen gültig sein
- Die API-Endpoints müssen mit der Arduino-Implementierung übereinstimmen

---

## Test-Checkliste

Nach Erstellung der Integration sollten folgende Dinge getestet werden:

- [ ] Integration kann in HA über UI hinzugefügt werden
- [ ] IP-Adresse und Port können eingegeben werden
- [ ] Verbindung zum Gerät wird geprüft
- [ ] Switch-Entity erscheint nach Setup
- [ ] Switch kann Display ein- und ausschalten
- [ ] Status-Updates funktionieren (alle 30 Sekunden)
- [ ] Fehlerbehandlung funktioniert (z.B. Gerät ausgeschaltet)

---

## Erweiterungsmöglichkeiten (Optional)

Die Integration kann später erweitert werden um:

- **Light-Entity**: Für Helligkeitssteuerung
- **Sensor-Entities**: Für Zeit, Effekt, Präsenz-Status
- **Select-Entity**: Für Effekt-Wechsel
- **Binary-Sensor**: Für Präsenz-Status

Diese können in späteren Versionen hinzugefügt werden.

