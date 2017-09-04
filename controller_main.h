#ifndef CONTROLLER_MAIN_H
#define CONTROLLER_MAIN_H

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

#endif
