# Ikea Obegraensad

Arduino project for the IKEA OBEGRÄNSAD LED matrix. A small web server
running on a D1 mini (ESP8266) lets you choose between multiple visual
effects and configure the device on the fly. The matrix is updated via SPI
using custom display routines. A global NTP client keeps the device's clock
in sync so the current time can be displayed both on the matrix and in the
web interface.

## Available Effects

* **Snake** – classic crawling snake across the 16x16 matrix
* **Clock** – simple digital clock synced via NTP
* **Rain** – random raindrops falling down the display
* **Bounce** – single pixel bouncing off the display edges
* **Stars** – scattered points that twinkle randomly
* **Lines** – moving vertical stripes sweeping across the matrix
* **Pulse** – pulsing circles emanating from the center
* **Waves** – animated wave patterns
* **Spiral** – rotating spiral animation
* **Fire** – realistic fire effect
* **Plasma** – colorful plasma effect
* **Ripple** – water ripple simulation
* **Sand Clock** – falling sand animation with physics

Connect to the device's WiFi network and open the root page to switch
effects. The page shows the current time and effect and updates without a
full page reload. A dropdown allows changing effects instantly and the
timezone can be adjusted interactively (defaulting to Europe/Berlin with
automatic daylight saving time). A brightness slider lets you dim the LED
matrix remotely. A physical button wired to pin D4 cycles through the
available effects without using the web interface.

## Advanced Features

### Auto-Brightness
The device supports automatic brightness adjustment based on ambient light using an LDR (Light Dependent Resistor) connected to pin A0. The auto-brightness system uses:
- **Exponential Moving Average (EMA)** for smooth brightness transitions (prevents flickering from TV/monitor changes)
- **Non-blocking sensor sampling** to prevent watchdog resets
- **Configurable min/max brightness** and sensor calibration values
- **Adaptive threshold** to reduce unnecessary brightness changes

### MQTT Presence Detection (Aqara Integration)
The device can integrate with Aqara presence sensors (or any MQTT-based presence sensor) to automatically turn the display on/off based on room occupancy:
- **Automatic display control**: Display turns on when presence is detected
- **Configurable timeout**: Set how long the display stays on after the last presence detection (default: 5 minutes)
- **MQTT broker support**: Connect to any MQTT broker (e.g., Zigbee2MQTT, Home Assistant)
- **Energy saving**: Display automatically turns off when room is empty
- **Real-time status**: Web interface shows current presence and display status

#### MQTT Configuration Example
For Zigbee2MQTT with Aqara FP2/FP1 presence sensor:
- **MQTT Broker**: IP address of your Zigbee2MQTT server (e.g., `192.168.1.100`)
- **MQTT Port**: Default `1883`
- **MQTT Topic**: `zigbee2mqtt/[your_sensor_name]` (e.g., `zigbee2mqtt/aqara_fp2`)
- **Presence Timeout**: 300 seconds (5 minutes) - adjustable from 10s to 1 hour

**Finding your MQTT Topic:**
1. Open your Zigbee2MQTT web interface
2. Find your Aqara FP2 sensor in the device list
3. Note the "Friendly name" - this is used in the topic: `zigbee2mqtt/[friendly_name]`
4. Or use an MQTT client (like MQTT Explorer) to see all topics

**Supported Payload Formats:**
The code automatically detects and supports multiple formats:
- Simple values: `true`, `false`, `1`, `0`, `occupied`, `unoccupied`
- JSON (Aqara FP2): `{"presence":true}`, `{"presence":false}`, `{"occupancy":true}`, `{"occupancy":false}`

Note: The display automatically turns ON when MQTT is disabled to ensure normal operation.

### Stability Improvements
- **Non-blocking operations**: All sensor readings and network operations are non-blocking to prevent watchdog timer resets
- **Optimized auto-brightness**: Reduced from 10 to 5 samples with shorter delays (50ms total instead of 200ms)
- **Memory optimization**: EEPROM expanded to 256 bytes for storing all configuration
- **Reliable WiFi**: Automatic reconnection and NTP sync recovery

## Setup

To configure WiFi access, copy `secrets_template.h` to `secrets.h` and replace
the placeholder `ssid` and `password` values with your network credentials
before uploading.

## Dependencies

- ESP8266WiFi
- ESP8266WebServer
- PubSubClient (for MQTT support)
- EEPROM
- SPI
