#include "File.h"
#include "led.h"
bool getModeStatus()
{
    return ReadModeFromConfig();
}

void setModeStatus(bool status)
{
    WriteModeConfig(status);
}

void modeLEDChange(bool status)
{
    if (status)
    {
        Serial.println("5s回抽模式,白灯");

        ledPC(2, 200, 200, 200);
    }
    else
    {
        Serial.println("微动回抽模式,没灯");
        ledPC(2, 0, 0, 0);
    }
}