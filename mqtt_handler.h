#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H
#include "wifi_pass.secret.h"

#ifndef WIFI_SECRET
#error("Please create a wifi_pass.secret.h file with wifi credentials. See the project page for informaiton: https://github.com/grnt426/HomeAcDevice")
#else
extern const char* mqtt_server;
#endif

// MQTT Unique self-ID
extern const char* device_id;

extern int wifi_orig_init;

extern WiFiClient espClient;

void callback(char* topic, byte* payload, unsigned int length);

int reconnect();

int checkMqtt();

void mqttSetup();

void syncDeviceState(int powered, int temp, int mode, int fan);

#endif
