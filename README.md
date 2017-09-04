AC Controller
==============

To control window AC units with a control interface that works with commands from buttons, IR remotes, and MQTT messages.

Talks to the [HomeServer](https://github.com/grnt426/HomeServer)

Building The Project
======================

To build the project from source, you will need an Arduino IDE. There are slight modifications needed to libraries downloaded from the Arduino library manager to build the project.

Likewise, there are some minor steps for the project files themselves.

Dependencies
-------------

You can use the Arduino's builtin library manager to automatically download the below.

* Adafruit_GFX_Library
  * 1.2.2
  * for drawing on screens
* Adafruit_MCP23008_library
  * 1.0.1
  * for controlling an 8-pin port expander
* Adafruit_SSD1306
  * 1.1.2
  * for controlling the OLED
* IRremoteESP8266
  * 2.1.1
  * for sending/receiving IR commands
* PubSubClient
  * 2.6.0
  * for chatting with an MQTT server
* ArduinoJson
  * 5.11.0
  * For parsing JSON payloads from the HomeServer

Library Modifications
---------------------

Adafruit_SSD1306 is, as of version 1.1.2, by default configured to work with a different screen resolution. To correct this, you must modify the Adafruit_SSD1306.h file within the root of the library.

Inside, you will see something like the below:

    #define SSD1306_128_64
    // #define SSD1306_128_32
    // #define SSD1306_96_16
    
Uncomment the line for the OLED the board is using. As shown above, the 128x64 is correctly uncommented, and the others commented out.

Project Setup
-------------

Create a file called wifi_pass.secret.h, which should have something like the below

    #ifndef WIFI_SECRET
    #define WIFI_SECRET
    #define WIFI_SSID "your wifi's SSID"
    #define WIFI_PASS "your wifi's password"
    #define WIFI_SERV "the MQTT server to connect to (IP address/domain name)"
    #endif
    
Notes on the Circuit
====================

Parts List
-----------

* ESP8266 https://www.adafruit.com/product/2471
* Colorful Buttons https://www.adafruit.com/product/1010
* Small Button https://www.adafruit.com/product/1489
* Port Expander https://www.adafruit.com/product/593
* OLED https://www.adafruit.com/product/938
* IR Sensor https://www.adafruit.com/product/157
* Wall power adapter https://www.adafruit.com/product/276
* USB to TTL Serial Cable https://www.adafruit.com/product/954
* Male DC Power Jack http://a.co/fTvGf54 
* IR LED http://a.co/865qUtJ 
* (Lots of) Resistors http://a.co/6ABTFcK 
* Breadboard wires https://www.adafruit.com/product/153
* half-sized breadboard https://www.adafruit.com/product/64

ESP Pin Setup
--------------
Pins #4 and #5 are for the i2c bus, which the MCP and OLED share to communicate with the ESP.

Pin #0 is used for the RST pin on the OLED

Pin 14 is used to read the IR Sensor

Pin 16 is used to send pulses to the IR LED