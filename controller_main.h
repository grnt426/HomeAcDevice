#ifndef CONTROLLER_MAIN_H
#define CONTROLLER_MAIN_H
#include "wifi_pass.secret.h"

void drawSplashScreen();

void checkButtons();

void checkIrSensor();

void drawScreen();

void togglePower();

void lowerTemp();

void raiseTemp();

void cycleMode();

void cycleFan();

void processIrCommand(uint64_t code);

void fanUp();

void modeSet(uint64_t fc);

void fanSet(uint64_t fc);

void controlAc(uint64_t fc);

void checkNetworkStatus();

void offAnimRandomVector(int x, int y);

void overwriteAcState(int powered, int temp, int mode, int fanSpeed);

int isWifiConnected();

void callback(char* topic, byte* payload, unsigned int length);

void syncDeviceState(int powered, int temp, int mode, int fan);

void overwriteDeviceState(char* payload);

#endif
