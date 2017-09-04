#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23008.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/TomThumb.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>
#include <ESP8266WiFi.h>
#include "controller_main.h"
#include "wifi_pass.secret.h"
#include "granite_logo.h"
#include "mqtt_handler.h"
#include "timer.h"

#define OLED_RESET 0

// Button Map on the MCP (8-bit address)
#define B_TEMP_D 0
#define B_TEMP_U 1
#define B_MODE   2
#define B_FAN    3
#define B_POWER  4
#define B_UNDF   5
#define B_UNDF_2 6
#define B_UNDF_3 7

// Pins on the ESP
#define P_IR_SEN 14
#define P_IR_LED 16

// Known IR flash codes for the AC
#define IR_DEAD_CODE 0xFFFFFFFFFFFFFFFF
#define FC_POWER_T 0x10AF8877
#define FC_TEMP_UP 0x10AF708F
#define FC_TEMP_DN 0x10AFB04F
#define FC_FAN_DN  0x10AF20DF
#define FC_FAN_UP  0x10AF807F
#define FC_COOL    0x10AF906F
#define FC_SAVE    0x10AF40BF
#define FC_FAN_O   0x10AFE01F
#define FC_FAN_A   0x10AFF00F

/**
   Screen Setup
*/
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/**
   Wifi Setup
*/

#ifndef WIFI_SECRET
#error("Please create a wifi_pass.secret.h file with wifi credentials. See the project page for informaiton: https://github.com/grnt426/HomeAcDevice")
#else
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
#endif

/**
   IR LED/Sensor Setup
*/
IRsend irsend(P_IR_LED);
IRrecv irrecv(P_IR_SEN);
decode_results results;
irparams_t save;

/**
   MCP (Port Expander) Setup
*/
Adafruit_MCP23008 mcp;

/**
   Device State Information
*/
uint8_t temp = 72;

// 0 indicates no button is currently held down, 1 means at least one button is held down
uint8_t buttonPressed = 0;

// 0 means don't redraw, 1 means redraw the screen
uint8_t screenUpdate = 0;

// The AC has three modes
// 1) cool - actively uses the compressor to cool the air.
// 2) save - Energy Save will turn the compressor on and off.
// 3) fan - only the fan will run
const char* modeMap[] = {"cool", "save", "fan"};
int modeFcMap[] = {FC_COOL, FC_SAVE, FC_FAN_O};
const uint8_t modeLen = 3;
int modeSel = 1;

// The fan on the AC has four settings; auto will let the AC choose which of the three speeds to use
const char* fanMap[] = {"auto", "high", "med", "low"};
const uint8_t fanLen = 4;
int fanSel = 3;

// Whether or not the AC is on, and therefore whether this interface will respond to controls
uint8_t powerState = 1;

int mqttOrigInit = 0;
int wifiFinallyConn = 0;
int splashOn = 1;

int mqttState = 0;
uint8_t mqttStateAnimT = registerTimer(500);
int mqttAnimP = 0;

int wifiState = 0;

uint8_t offAnimT = registerTimer(50);
int offXv;
int offYv;
int offX = 55;
int offY = 25;

void setup() {
  Serial.begin(115200);

  // Analog pin 2 is unused, so we can use it as a pseudo-random source for a seed
  randomSeed(analogRead(2));
  offAnimRandomVector(1, 1);

  Serial.print("Booting as ");
  Serial.println(deviceId);

  // initialize with the I2C addr 0x3D (for the OLED)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawSplashScreen();

  // use the default address of 0 on the i2c bridge
  mcp.begin();

  mcp.pinMode(B_TEMP_D, INPUT);
  mcp.pinMode(B_TEMP_U, INPUT);
  mcp.pinMode(B_MODE, INPUT);
  mcp.pinMode(B_FAN, INPUT);
  mcp.pinMode(B_POWER, INPUT);

  mcp.pullUp(B_TEMP_D, HIGH);
  mcp.pullUp(B_TEMP_U, HIGH);
  mcp.pullUp(B_MODE, HIGH);
  mcp.pullUp(B_FAN, HIGH);
  mcp.pullUp(B_POWER, HIGH);

  pinMode(P_IR_LED, OUTPUT);
  irrecv.enableIRIn();

  mqttSetup();
}

void drawSplashScreen() {
  display.clearDisplay();
  display.drawBitmap(0, 0, granite_logo, 128, 64, 1);
  display.display();
}

void setupWifi() {

  if (wifiOrigInit == 0 && millis() > 10000) {
    wifiOrigInit = 1;

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
  }
  else {
    if (wifiFinallyConn == 0 && WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      wifiState = 1;
      wifiFinallyConn = 1;
      screenUpdate = 1;
    }
  }
}

/**
   Core device loop, which checks inputs across all channels, applies changes, and then redraws.
*/
void loop() {
  checkNetworkStatus();

  processTimers();

  checkButtons();

  checkIrSensor();

  drawScreen();
}

void checkNetworkStatus() {
  setupWifi();

  if (WiFi.status() == WL_CONNECTED) {
    if (wifiState != 1) {
      screenUpdate = 1;
    }
    wifiState = 1;
  }
  else {
    if (wifiState != 0) {
      screenUpdate = 1;
    }
    wifiState = 0;
  }

  mqttState = checkMqtt();
  switch (mqttState) {
    case -1: // Retrying
      break;
    case 0: // Unstarted
      break;
    case 1: // Still connected
      break;
    case 2: // Reconnected
      screenUpdate = 1;
      syncDeviceState(powerState, temp, modeSel, fanSel);
      break;
    default: Serial.println("Unknown MQTT network state");
  }
}

void drawScreen(void) {

  if (mqttState != 1 && mqttState != 2 && splashOn == 0) {
    if (isTimerPassed(mqttStateAnimT)) {
      screenUpdate = 1;
      resetTimer(mqttStateAnimT);
      mqttAnimP = 1 - mqttAnimP;
    }
  }

  if (powerState == 0) {
    if (isTimerPassed(offAnimT)) {
      resetTimer(offAnimT);
      screenUpdate = 1;

      offX += offXv;
      offY += offYv;

      if (offX <= 0 || offX >= 95) {
        offAnimRandomVector(1, 0);
        if (offX <= 0) {
          offX = 1;
        }
        else {
          offX = 95;
          offXv *= -1;
        }
      }

      if (offY <= 20 || offY >= 64) {
        offAnimRandomVector(0, 1);
        if (offY <= 20) {
          offY = 20;
        }
        else {
          offY = 64;
          offYv *= -1;
        }
      }
    }
  }

  // Only update if we need to. We want the splash screen to display at startup for a while, too.
  if (screenUpdate == 1 || (millis() > 5000 && splashOn == 1)) {
    splashOn = 0;
    display.clearDisplay();

    if (powerState == 1) {
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(4);
      display.setCursor(27, 64);
      display.print(temp);

      display.setFont(&TomThumb);
      display.setTextSize(2);
      display.setCursor(0, 17);
      display.print(modeMap[modeSel]);
      display.setCursor(0, 64);
      display.print(fanMap[fanSel]);
    }
    else {
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(1);
      display.setCursor(offX, offY);
      display.print("Off");
    }

    if (mqttState != 1 && mqttState != 2) {
      display.setFont(&TomThumb);
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.print("no mqtt");
      if (mqttAnimP) {
        display.print("*");
      }
    }

    if (wifiState == 0) {
      display.setFont(&TomThumb);
      display.setTextSize(1);
      display.setCursor(0, 40);
      display.print("no wifi");
    }

    screenUpdate = 0;
    display.display();
  }
}

/**
   Check all the buttons on the device, not allowing a button to be held down for repeat commands.
   When the AC is off, this will only allow power button to be pressed.
*/
void checkButtons(void) {

  if (buttonPressed == 1 && mcp.digitalRead(B_TEMP_U) == 0 && mcp.digitalRead(B_TEMP_D) == 0
      && mcp.digitalRead(B_MODE) == 0 && mcp.digitalRead(B_FAN) == 0 && mcp.digitalRead(B_POWER) == 0) {
    buttonPressed = 0;
  }

  if (mcp.digitalRead(B_POWER) == 1 && buttonPressed == 0) {
    Serial.println("B_POWER Button Pressed");
    togglePower();
  }

  if (powerState == 0) {
    return;
  }

  if (mcp.digitalRead(B_TEMP_D) == 1 && buttonPressed == 0 && temp > 59) {
    Serial.println("B_TEMP_D Button Pressed");
    lowerTemp();
  }

  if (mcp.digitalRead(B_TEMP_U) == 1 && buttonPressed == 0 && temp < 90) {
    Serial.println("B_TEMP_U Button Pressed");
    raiseTemp();
  }

  if (mcp.digitalRead(B_MODE) == 1 && buttonPressed == 0) {
    Serial.println("B_MODE Button Pressed");
    cycleMode();
  }

  if (mcp.digitalRead(B_FAN) == 1 && buttonPressed == 0) {
    Serial.println("B_FAN Button Pressed");
    cycleFan();
  }
}

void checkIrSensor(void) {
  if (irrecv.decode(&results, &save)) {
    uint64_t code = results.value;
    if (results.decode_type != UNKNOWN && code != IR_DEAD_CODE) {
      serialPrintUint64(code, 16);
      Serial.println("");
      processIrCommand(code);
    }
  }
}

void processIrCommand(uint64_t code) {
  switch (code) {
    case FC_POWER_T : Serial.println("Power flash"); togglePower(); break;
    case FC_TEMP_UP : Serial.println("Temp Up flash"); raiseTemp(); break;
    case FC_TEMP_DN : Serial.println("Temp Down flash"); lowerTemp(); break;
    case FC_FAN_UP  : Serial.println("Fan Up flash"); fanUp(); break;
    case FC_FAN_DN  : Serial.println("Fan Down flash"); cycleFan(); break;
    case FC_COOL    : Serial.println("Cool flash"); modeSet(FC_COOL); break;
    case FC_SAVE    : Serial.println("Energy Save flash"); modeSet(FC_SAVE); break;
    case FC_FAN_O   : Serial.println("Fan Only flash"); modeSet(FC_FAN_O); break;
    case FC_FAN_A   : Serial.println("Fan Auto flash"); fanSet(FC_FAN_A); break;
    default: Serial.println("Unknown code: "); serialPrintUint64(code, 16); Serial.println("");
  }
}

void togglePower(void) {
  powerState = (powerState + 1) % 2;
  controlAc(FC_POWER_T);
  buttonPressed = 1;
  screenUpdate = 1;
}

void raiseTemp(void) {
  temp++;
  controlAc(FC_TEMP_UP);
  buttonPressed = 1;
  screenUpdate = 1;
}

void lowerTemp(void) {
  temp--;
  controlAc(FC_TEMP_DN);
  buttonPressed = 1;
  screenUpdate = 1;
}

void cycleMode(void) {
  modeSel = (modeSel + 1) % modeLen;
  controlAc(modeFcMap[modeSel]);
  buttonPressed = 1;
  screenUpdate = 1;
}

void cycleFan(void) {
  fanSel = (fanSel + 1) % fanLen;
  controlAc(FC_FAN_DN);
  buttonPressed = 1;
  screenUpdate = 1;
}

void fanUp(void) {
  fanSel--;
  if (fanSel < 0) {
    fanSel = fanLen - 1;
  }
  controlAc(FC_FAN_UP);
  buttonPressed = 1;
  screenUpdate = 1;
}

void modeSet(uint64_t mode) {
  if (mode == FC_COOL) {
    modeSel = 0;
  }
  else if (mode == FC_SAVE) {
    modeSel = 1;
  }
  else {
    modeSel = 2;
  }
  screenUpdate = 1;
  controlAc(mode);
}

void fanSet(uint64_t setting) {
  fanSel = 0;
  controlAc(setting);
  screenUpdate = 1;
}

void controlAc(const uint64_t command) {
  Serial.print("Flashing: ");
  serialPrintUint64(command, 16);
  Serial.println("");
  irsend.sendNEC(command, 32);
  syncDeviceState(powerState, temp, modeSel, fanSel);
}

void offAnimRandomVector(int changeX, int changeY) {
  offXv = changeX ? random(1, 4) : offXv;
  offYv = changeY ? random(1, 4) : offYv;
}

