# Ikea Obegraensad

Arduino project for the IKEA OBEGRÄNSAD LED matrix. A small web server
running on a D1 mini (ESP8266) lets you choose between multiple visual
effects. The matrix is updated via SPI using custom display routines.

## Available Effects

* **Snake** – classic crawling snake across the 16x16 matrix
* **Clock** – simple digital clock synced via NTP
* **Rain** – random raindrops falling down the display

Connect to the device's WiFi network and open the root page to switch
effects.

To configure WiFi access, copy `secrets_template.h` to `secrets.h` and replace
the placeholder `ssid` and `password` values with your network credentials
before uploading.
