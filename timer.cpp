#include <Arduino.h>

const uint8_t maxTimers = 50;
uint64_t timers[maxTimers];
uint64_t accumulators[maxTimers];
uint8_t freeSlot = 0;

uint64_t oldTime = 0;

int registerTimer(uint64_t delay) {

  // You are using too many timers. Increase array size.
  if (freeSlot >= maxTimers) {
    Serial.println("\n***\nYou are using too many timers, make the array bigger!\n***");
    freeSlot = freeSlot / 0; // force a crash so this error is noticed and fixed
  }

  uint8_t token = freeSlot++;
  timers[token] = delay;
  accumulators[token] = 0;
  return token;
}

int isTimerPassed(uint64_t token) {
  return accumulators[token] >= timers[token];
}

void processTimers() {
  uint64_t time = millis();
  int delta = 0;

  // This will only be true if the millis() function rolled over.
  // For simplicity, do nothing
  if (time < oldTime) {
    Serial.println("Rolling over timer...");
    delta = sizeof(uint64_t) - oldTime;
    delta += time;
    return;
  }
  else {
    delta = time - oldTime;
  }
  oldTime = time;

  for (int i = 0; i < freeSlot; i++) {

    // To avoid overflowing the accumulator (and therefore missing a timer passing), don't keep adding after activating
    if (accumulators[i] < timers[i]) {
      accumulators[i] += delta;
    }
  }
}

void resetTimer(uint64_t token) {
  accumulators[token] = 0;
}

