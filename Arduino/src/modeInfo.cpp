#include "File.h"
#include "led.h"
#include "modeInfo.h"
uint8_t getBackPullTime()
{
    return ReadModeFromConfig();
}

void setBackPullTime(uint8_t modeNum)
{
    WriteModeConfig(modeNum);
}
void singleFlashLED()
{
    ledPC(2, 200, 200, 200);
    delay(50);
    ledPC(2, 0, 0, 0);
    delay(50);
    ledPC(2, 200, 200, 200);
    delay(50);
    ledPC(2, 0, 0, 0);
}

// enum modeType
// {
//     tow = 0,
//     five = 1,
//     ten = 2,
//     twenty_fifth = 3,
//     end = 4
// };

// void modeLEDSelect(uint8_t modeNum)
// {
//     switch (modeNum)
//     {
//     case tow:
//         singleFlashLED();
//         ledPC(2, 255, 128, 0); // 橙色
//         break;
//     case five:
//         singleFlashLED();
//         ledPC(2, 200, 200, 200); // 白色
//         break;
//     case ten:
//         singleFlashLED();
//         ledPC(2, 128, 255, 0); // 绿色
//         break;
//     case twenty_fifth:
//         singleFlashLED();
//         ledPC(2, 127, 0, 255); // 紫色
//         break;
//     case end:
//         singleFlashLED();
//         ledPC(2, 0, 0, 0); // 无色
//         break;
//     }
// }
// 返回延迟时间

uint16_t backPullDelay(uint8_t modeNum)

{
    return modeNum * 1000;
}
