# Ikea Obegraensad

Arduino project for the IKEA OBEGRÄNSAD LED matrix. A small web server
running on a D1 mini (ESP8266) lets you choose between multiple visual
effects. The matrix is updated via SPI using custom display routines. A
global NTP client keeps the device's clock in sync so the current time can
be displayed both on the matrix and in the web interface.

## Available Effects

* **Snake** – classic crawling snake across the 16x16 matrix
* **Clock** – simple digital clock synced via NTP
* **Rain** – random raindrops falling down the display
* **Bounce** – single pixel bouncing off the display edges

Connect to the device's WiFi network and open the root page to switch
effects. The page displays the current time from the NTP server and offers
styled buttons for easier navigation.

To configure WiFi access, copy `secrets_template.h` to `secrets.h` and replace
the placeholder `ssid` and `password` values with your network credentials
before uploading.
