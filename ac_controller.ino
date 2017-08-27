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
#include <PubSubClient.h>
#include "wifi_pass.secret.h"

#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_MCP23008 mcp;

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

const char* device_id = "ac_alpha";

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqtt_server = WIFI_SERV;

uint8_t temp = 72;
uint8_t buttonPressed = 0;
uint8_t screenUpdate = 0;

// Button Map on the MCP
#define B_TEMP_D 0
#define B_TEMP_U 1
#define B_MODE   2
#define B_FAN    3
#define B_POWER  4
#define B_UNDF   5
#define B_UNDF_2 6
#define B_UNDF_3 7

#define P_IR_SEN 14
#define P_IR_LED 16

#define IR_DEAD_CODE 0xFFFFFFFFFFFFFFFF
#define FC_POWER_TOGGLE 0x10AF8877
#define FC_TEMP_UP 0x10AF708F
#define FC_TEMP_DN 0x10AFB04F

const char* mode_map[] = {"cool", "save", "fan"};
const uint8_t mode_len = 3;
uint8_t mode_sel = 1;

const char* fan_map[] = {"auto", "high", "med", "low"};
const uint8_t fan_len = 4;
uint8_t fan_sel = 3;

uint8_t power_state = 1;

IRsend irsend(P_IR_LED);

IRrecv irrecv(P_IR_SEN);
decode_results results;
irparams_t save;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];

void setup()   {                
  Serial.begin(115200);

  Serial.print("Booting as ");
  Serial.println(device_id);

  // initialize with the I2C addr 0x3D (for the 128x64)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display.clearDisplay();
  display.setTextColor(WHITE);

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

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  screenUpdate = 1;
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // TODO: This can prevent the device from coming up if wifi is down, should break up retry
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the AC if a 1 was received as first character
  if ((char)payload[0] == '1') {
    controlAc(FC_POWER_TOGGLE);
  }
}

void reconnect() {
  Serial.print("Attempting MQTT connection...");

  if (client.connect(device_id)) {
    Serial.println("connected");
    
    snprintf (msg, 75, "name:%s", device_id);
    client.publish("activate", msg);

    snprintf (msg, 75, "ac/%s", device_id);
    client.subscribe(msg);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    delay(5000);
  }
}

void loop() {
  checkWifi();
  
  checkButtons();

  checkIrSensor();

  drawScreen();
}

void checkWifi() {
  if (!client.connected()) {
    reconnect();
  }
  else{
    client.loop();
  }
}

void drawScreen(void) {
  if(screenUpdate == 1){
    if(power_state == 1){
      display.clearDisplay(); 
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(4);
      display.setCursor(27,64);
      display.print(temp);
  
      display.setFont(&TomThumb);
      display.setTextSize(2);
      display.setCursor(0, 17);
      display.print(mode_map[mode_sel]);
      display.setCursor(0, 64);
      display.print(fan_map[fan_sel]);
      display.display();
      screenUpdate = 0;
    }
    else{
      display.clearDisplay(); 
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(1);
      display.setCursor(0,20);
      display.print("Off");
      display.display();
      screenUpdate = 0;
    }
  }
}

void checkButtons(void) {

  if(buttonPressed == 1 && mcp.digitalRead(B_TEMP_U) == 0 && mcp.digitalRead(B_TEMP_D) == 0 
    && mcp.digitalRead(B_MODE) == 0 && mcp.digitalRead(B_FAN) == 0 && mcp.digitalRead(B_POWER) == 0){
    buttonPressed = 0;
  }

  if(mcp.digitalRead(B_POWER) == 1 && buttonPressed == 0) {
    Serial.println("B_POWER Button Pressed");
    togglePower();
    buttonPressed = 1;
  }

  if(power_state == 0) {
    return;
  }

  if(mcp.digitalRead(B_TEMP_D) == 1 && buttonPressed == 0 && temp > 59) {
    Serial.println("B_TEMP_D Button Pressed");
    lowerTemp();
    screenUpdate = 1;
  }

  if(mcp.digitalRead(B_TEMP_U) == 1 && buttonPressed == 0 && temp < 90) {
    Serial.println("B_TEMP_U Button Pressed");
    raiseTemp();
    screenUpdate = 1;
  }

  if(mcp.digitalRead(B_MODE) == 1 && buttonPressed == 0) {
    Serial.println("B_MODE Button Pressed");
    mode_sel = (mode_sel + 1) % mode_len;
    buttonPressed = 1;
    screenUpdate = 1;
  }

  if(mcp.digitalRead(B_FAN) == 1 && buttonPressed == 0) {
    Serial.println("B_FAN Button Pressed");
    fan_sel = (fan_sel + 1) % fan_len;
    buttonPressed = 1;
    screenUpdate = 1;
  }
}

void checkIrSensor(void) {
  if (irrecv.decode(&results, &save)) {
    Serial.println("IR Signal seen!");
    dumpCode(&results);
  }
}

void dumpCode(decode_results *results) {

  // Now dump "known" codes
  uint64_t code = results->value;
  if (results->decode_type != UNKNOWN && code != IR_DEAD_CODE) {
    serialPrintUint64(code, 16);
    Serial.println("");
    processIrCommand(code);
  }
}

void processIrCommand(uint64_t code) {
  switch(code) {
    case FC_POWER_TOGGLE : Serial.print("Power flash"); togglePower(); break;
    case FC_TEMP_UP : Serial.print("Temp Up flash"); raiseTemp(); break;
    case FC_TEMP_DN : Serial.print("Temp Down flash"); lowerTemp(); break;
    default: Serial.print("Unknown code:"); serialPrintUint64(code, 16);
  }
}

void togglePower(void) {
  power_state = (power_state + 1) % 2;
  controlAc(FC_POWER_TOGGLE);
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

void controlAc(const uint64_t command) {
  Serial.print("Flashing: ");
  serialPrintUint64(command, 16);
  irsend.sendNEC(command, 32);
}
