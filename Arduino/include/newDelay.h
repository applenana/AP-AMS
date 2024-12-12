#ifndef NEWDELAY_H
#define NEWDELAY_H

#include <Arduino.h>

typedef void (*CallbackFunction)();

class NewDelay
{
public:
    NewDelay() : isDelayOver(false), startTime(0), delayDuration(0), callback(nullptr) {}

    void setDelay(unsigned long delayMs, CallbackFunction callbackFunction);
    void checkDelay();

private:
    bool isDelayOver;

    unsigned long startTime;
    unsigned long delayDuration;
    CallbackFunction callback;
};

#endif // NEWDELAY_H