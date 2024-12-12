#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ElegantOTA.h>
#include <Adafruit_NeoPixel.h>

#include "dataPacket.h"
#include "processData.h"
#include "ServoMotor.h"
#include "Motor.h"
#include <LittleFS.h>
#include "led.h"
#include "modeInfo.h"

#include "newDelay.h"
#include "webServer.h"
// ota相关
IPAddress local_IP(10, 10, 0, 1);    // 设置本地IP地址
IPAddress gateway(10, 10, 0, 1);     // 设置网关IP地址，通常和本地IP相同
IPAddress subnet(255, 255, 255, 0);  // 设置子网掩码
const char *password = "1234567890"; // 热点密码
unsigned long SysStartTime;          // 系统启动时间
bool serverRunning = true;           // 是否开启服务
const char *version = "1.4.0.0.7";

// 1修复amcu发包异常导致的断料状态错误（错误发送送出结束，和回抽结束）

// 业务相关

DataPacket datapacket;
ServoMotor sv(13);                                   // 构建舵机对象(选择13引脚为pwm输出引脚)
Motor mc(4, 5);                                      // 构建电机对象(选择4和5引脚控制电机驱动)
Adafruit_NeoPixel leds(3, 12, NEO_GRB + NEO_KHZ800); // 构建leds对象(3个led,12号引脚)
unsigned int FilamentIoPin = A0;                     // 断料检测引脚(ADC=>A0)
String FilamentState = "none";                       // 断料状态
bool IsFPinHigh;                                     // ADC是否高电平

// uint8_t modeNum = 1;                                 // 默认模式1，5s回抽
uint8_t backStopTime = 5;        // 退料时间 单位秒
unsigned long LastBootPressTime; // 上一次Boot按键被按下的时间
bool AllowBootPress = true;      // 允许按下Boot按键
bool FirstBootPress = false;     // 首次按下Boot按键
NewDelay backDelay;              // 回抽停止延时对象
bool lastBackStopStatus = false;

void FilamentChange(bool ToStateIsHigh);

// void bootPressRun()
// {
//     // *** boot切换相关
//     if (digitalRead(0) == LOW)
//     {
//         unsigned long nowTime = millis();
//         if (!FirstBootPress)
//         {
//             LastBootPressTime = nowTime;
//             FirstBootPress = true;
//         }
//         if ((nowTime - LastBootPressTime) > 2000 && AllowBootPress)
//         { // 大于2s就是切换
//             modeNum++;
//             if (modeNum > 4)
//             {
//                 modeNum = 0;
//             }
//             setBackPullTime(modeNum);
//             modeLEDSelect(modeNum);
//             AllowBootPress = false;
//         }
//     }
//     else
//     {
//         FirstBootPress = false;
//         AllowBootPress = true;
//     }
// }

void run()

{
    if ((analogRead(A0) > 1000) != IsFPinHigh)
    {
        FilamentChange(not IsFPinHigh);
        IsFPinHigh = not IsFPinHigh;
    }

    if (datapacket.index == 0 and Serial.read() == 0x3D)
    {
        // Serial.println("帧头");
        datapacket.index += 1;
        datapacket.contentlength = 0;
        ledPC(0, 0, 255, 0);
    }
    else if (Serial.available() and datapacket.index > 0 and datapacket.index <= 4)
    {
        // Serial.println("读取序号,地址,长度,headCRC");
        switch (datapacket.index)
        {
        case 1:
            datapacket.sequenceNumber = Serial.read();
            // Serial.println(datapacket.sequenceNumber,HEX);
            break;
            ledPC(0, 0, 0, 0);
        case 2:
            datapacket.address = Serial.read();
            // Serial.println(datapacket.address,HEX);
            ledPC(0, 0, 255, 0);
            break;
        case 3:
            datapacket.length = Serial.read();
            // Serial.println(datapacket.length,HEX);
            ledPC(0, 0, 0, 0);
            break;
        case 4:
            datapacket.headCRC = Serial.read();
            // Serial.println(datapacket.headCRC,HEX);
            ledPC(0, 0, 255, 0);
            break;
        }
        datapacket.index += 1;
    }
    else if (Serial.available() and datapacket.index == 5)
    {
        // Serial.println("检验头");
        if (datapacket.headCheck())
        {
            datapacket.commandType = Serial.read();
            datapacket.index += 1;
            // Serial.println("检验头success|读取命令号");
            // Serial.println(datapacket.commandType,HEX);
            ledPC(0, 0, 0, 0);
        }
        else
        {
            datapacket.index = 0;
            // Serial.println("检验头失败");
            ledPC(0, 255, 0, 0);
        }
    }
    else if (Serial.available() and datapacket.index >= 6 and datapacket.index <= datapacket.length - 3)
    {
        // Serial.println("读取内容");
        ledPC(0, 0, 255, 0);
        datapacket.content[datapacket.contentlength] = Serial.read();
        // Serial.println(datapacket.content[datapacket.contentlength],HEX);
        datapacket.index += 1;
        datapacket.contentlength += 1;
    }
    else if (Serial.available() and datapacket.index >= datapacket.length - 2 and datapacket.index < datapacket.length)
    {
        // Serial.println("读取总CRC");
        if (datapacket.index == datapacket.length - 2)
        {
            datapacket.totalLowCRC = Serial.read();
            // Serial.println("读取低CRC");
            // Serial.println(datapacket.totalLowCRC,HEX);
            ledPC(0, 0, 0, 0);
        }
        else if (datapacket.index == datapacket.length - 1)
        {
            datapacket.totalHighCRC = Serial.read();
            // Serial.println("读取高CRC");
            // Serial.println(datapacket.totalHighCRC,HEX);
            ledPC(0, 0, 255, 0);
        }
        datapacket.index += 1;
    }
    else if (datapacket.index == datapacket.length and datapacket.index != 0)
    {
        // Serial.println("进入最后处理");
        // Serial.println(FilamentState);
        if (datapacket.totalCheck())
        {
            // Serial.println("进入最后处理，success");
            ledPC(0, 0, 0, 0);
            processData(datapacket);
            datapacket.index = 0;
        }
        else
        {
            // Serial.println("进入最后处理，fail");
            datapacket.index = 0;
            ledPC(0, 255, 0, 0);
        }
    }
    // 电机超时判断
    if (!mc.getStopState())
    {
        mc.update();
    }
}

void FilamentChange(bool ToStateIsHigh)
{

    if (backStopTime != 255)
    {
        if (ToStateIsHigh)
        {
            FilamentState = "exist";
        }
        else if (!ToStateIsHigh)
        {

            FilamentState = "inexist";
        }
    }
    else

    { // 低变高，无变有
        if (ToStateIsHigh)
        {
            // 之前是空或者无料，无变有
            if (FilamentState == "none" || FilamentState == "inexist")
            {
                FilamentState = "exist";
            }
            // 之前电机运转，无变有

            else if (FilamentState == "busy")
            {
                mc.stop();
                sv.pull();
                FilamentState = "exist";
            }

        }
        // 高变低，有变无
        else if (!ToStateIsHigh)
        { // 之前是空或者有料，有变无
            mc.forward(300000);
            unsigned long start = millis();
            while (millis() - start < 20)
            {
            }
            mc.stop();
            sv.pull();
            FilamentState = "inexist";
            // if (FilamentState == "none" || FilamentState == "exist")
            // {

            //     FilamentState = "inexist";
            // }
            // // 之前电机运转，有变无
            // else if (FilamentState == "busy")
            // {

            //     mc.forward(300000);
            //     unsigned long start = millis();
            //     while (millis() - start < 20)
            //     {
            //     }
            //     mc.stop();
            //     sv.pull();
            //     FilamentState = "inexist";
            // }
        }
    }
}

void setup()
{
    Serial.begin(115200, SERIAL_8E1);
    Serial1.begin(115200, SERIAL_8E1);
    LittleFS.begin();
    leds.begin();
    IsFPinHigh = (analogRead(A0) > 1000);
    if (IsFPinHigh)
    {
        FilamentState = "exist";
    }
    else
    {
        FilamentState = "inexist";
    }
    leds.setBrightness(100);
    leds.clear();
    leds.show();
    backStopTime = getBackPullTime();
    // 热点相关
    byte mac[6];
    WiFi.macAddress(mac);
    char macStr[18];
    sprintf(macStr, "APAMS-%02X%02X%02X", mac[3], mac[4], mac[5]);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(macStr, password);
    recWebServer();
    ElegantOTA.begin(&server);
    ElegantOTA.setAutoReboot(true);

    SysStartTime = millis();

    server.begin();

    // 检查5秒内是否收到0x3D
    unsigned long startTime = millis();
    bool receivedChar = false;

    while (millis() - startTime < 5000)
    { // 5秒内
        if (Serial.available() > 0)
        {
            if (Serial.read() == 0x3D)
            {
                receivedChar = true;
                break;

            }
        }
    }

    if (!receivedChar)
    {
        // 进入OTA模式
        while (true)
        {
            ElegantOTA.loop();

            if (WiFi.softAPgetStationNum() > 0)
            {
                ledPC(0, 0, 255, 0);
            }
            else
            {
                ledPC(0, 0, 0, 255);
            }
            // bootPressRun();
            delay(1);
        }
    }
}

void loop()
{
    if (needBackStop != lastBackStopStatus)
    {
        lastBackStopStatus = needBackStop;
        if (needBackStop)
        {
            backDelay.setDelay(backPullDelay(backStopTime), backStop);
        }
    }

    backDelay.checkDelay();

    run();
}