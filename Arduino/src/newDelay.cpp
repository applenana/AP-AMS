#include "newDelay.h"

void NewDelay::setDelay(unsigned long delayMs, CallbackFunction callbackFunction)
{
    startTime = millis();
    delayDuration = delayMs;
    callback = callbackFunction;
    isDelayOver = false;
}

void NewDelay::checkDelay()
{
    if (!isDelayOver && millis() - startTime >= delayDuration)
    {
        if (callback != nullptr)
        {
            callback();
            isDelayOver = true;
        }
    }
}