#include "File.h"
#include "led.h"
int getModeStatus()
{
    return ReadModeFromConfig();
}

void setModeStatus(int modeNum)
{
    WriteModeConfig(modeNum);
}
void singleFlashLED()
{
    ledPC(2, 200, 200, 200);
    delay(200);
    ledPC(2, 0, 0, 0);
    delay(200);
    ledPC(2, 200, 200, 200);
    delay(200);
}

enum modeType
{
    tow = 0,
    five = 1,
    ten = 2,
    twenty_fifth = 3,
    end = 4
};

void modeLEDSelect(int modeNum)
{
    switch (modeNum)
    {
    case tow:
        singleFlashLED();
        ledPC(2, 255, 128, 0); // 橙色
        break;
    case five:
        singleFlashLED();
        ledPC(2, 200, 200, 200); // 白色
        break;
    case ten:
        singleFlashLED();
        ledPC(2, 128, 255, 0); // 绿色
        break;
    case twenty_fifth:
        singleFlashLED();
        ledPC(2, 127, 0, 255); // 紫色
        break;
    case end:
        singleFlashLED();
        ledPC(2, 0, 0, 0); // 无色
        break;
    }
}

void modeDelay(int modeNum)
{
    switch (modeNum)
    {
    case tow:
        delay(2000);
        break;
    case five:
        delay(5000);
        break;
    case ten:
        delay(10000);
        break;
    case twenty_fifth:
        delay(25000);
        break;
    }
}