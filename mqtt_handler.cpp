#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "mqtt_handler.h"
#include "controller_main.h"

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int wifi_orig_init = 0;

const char* device_id = "ac_alpha";
const char* mqtt_server = WIFI_SERV;

void mqttSetup() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/**
 * Used by the MQTT handler to process messages from topics we are subscribed to.
 */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char* str = (char *) payload;
  str[length] = '\0';
  Serial.println(str);
  processIrCommand((int)strtol(str, NULL, 0));
}

/**
 * Used to reconnect to the MQTT server, *NOT* the WiFi.
 * TODO: This will hold up the device for 5 seconds, should instead use a timer to trigger when to attempt reconnecting.
 */
void reconnect() {
  Serial.print("Attempting MQTT connection...");

  if (client.connect(device_id)) {
    Serial.println("connected");
    
    snprintf (msg, 75, "name:%s", device_id);
    client.publish("activate", msg);

    snprintf (msg, 75, "ac/%s", device_id);
    client.subscribe(msg);
  }
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    delay(5000);
  }
}

void checkMqtt() {

  if(wifi_orig_init == 1) {
    if (!client.connected()) {
      reconnect();
    }
    else{
      client.loop();
    }
  }
}

