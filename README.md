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

Connect to the device's WiFi network and open the root page to switch
effects. The page shows the current time and effect and updates without a
full page reload. A dropdown allows changing effects instantly and the
timezone can be adjusted interactively (defaulting to Europe/Berlin with
automatic daylight saving time). A brightness slider lets you dim the LED
matrix remotely. A physical button wired to pin D4 cycles through the
available effects without using the web interface.

To configure WiFi access, copy `secrets_template.h` to `secrets.h` and replace
the placeholder `ssid` and `password` values with your network credentials
before uploading.
