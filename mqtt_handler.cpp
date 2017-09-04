#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "mqtt_handler.h"
#include "controller_main.h"
#include "timer.h"

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[100];
int wifiOrigInit = 0;

const char* deviceId = "ac_alpha";
const char* deviceIdTopic = "ac/ac_alpha";
const char* deviceSyncTopic = "ac/sync/ac_alpha";
const char* mqttServer = WIFI_SERV;

int firstConn = 1;

int mqttRetryT = registerTimer(30000);

void mqttSetup() {
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
}

/**
   Used by the MQTT handler to process messages from topics we are subscribed to.
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
   Used to reconnect to the MQTT server, *NOT* the WiFi.
*/
int reconnect() {
  if (firstConn || isTimerPassed(mqttRetryT)) {
    firstConn = 0;

    Serial.print("Attempting MQTT connection...");

    // Unfortunately, the below call within the if-statement is blocking. As the
    // server is on the local network, timeouts are fast if the host is on, but
    // not listening on that port. Will test host off to see how long it takes.
    if (client.connect(deviceId)) {
      Serial.println("connected");
      client.subscribe(deviceIdTopic);

      snprintf (msg, 75, "name:%s", deviceId);
      client.publish("activate", msg);
      return 2;
    }
    else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" trying again in 30 seconds");
      resetTimer(mqttRetryT);
      return -1;
    }
  }

  return 0;
}

int checkMqtt() {

  if (wifiOrigInit == 1) {
    if (!client.connected()) {
      return reconnect();
    }
    else {
      client.loop();
      return 1;
    }
  }

  return 0;
}

void syncDeviceState(int powered, int temp, int mode, int fan) {
  snprintf (msg, 100, "{\"powered\":%d,\"temperature\":%d,\"mode\":%d,\"fanSpeed\":%d}", powered, temp, mode, fan);
  Serial.print("Syncing state: ");
  Serial.println(msg);
  client.publish(deviceSyncTopic, msg);
}

