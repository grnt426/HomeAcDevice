#ifndef TIMER_H
#define TIMER_H

int registerTimer(uint64_t delay);

int isTimerPassed(uint64_t token);

void processTimers();

void resetTimer(uint64_t token);
#endif

