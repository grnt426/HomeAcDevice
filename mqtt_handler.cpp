#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "mqtt_handler.h"
#include "controller_main.h"

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[100];
int wifi_orig_init = 0;

const char* device_id = "ac_alpha";
const char* device_id_topic = "ac/ac_alpha";
const char* device_sync_topic = "ac/sync/ac_alpha";
const char* mqtt_server = WIFI_SERV;

uint64_t time_start_wait = 0;
int awaiting_retry = 0;

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
 */
void reconnect() {
  if(!awaiting_retry || millis() - time_start_wait > 5000) {
    awaiting_retry = 0;
    
    Serial.print("Attempting MQTT connection...");
  
    if (client.connect(device_id)) {
      Serial.println("connected");
      client.subscribe(device_id_topic);
      
      snprintf (msg, 75, "name:%s", device_id);
      client.publish("activate", msg);
    }
    else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      time_start_wait = millis();
      awaiting_retry = 1;
    }
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

void syncDeviceState(int powered, int temp, int mode, int fan) {
  snprintf (msg, 100, "{\"powered\":%d,\"temperature\":%d,\"mode\":%d,\"fanSpeed\":%d}", powered, temp, mode, fan);
  Serial.print("Syncing state: ");
  Serial.println(msg);
  client.publish(device_sync_topic, msg);
}

