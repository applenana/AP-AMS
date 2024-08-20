#include <Arduino.h>
#include "dataPacket.h"
#include "processData.h"
#include "ServoMotor.h"
#include "Motor.h"
#include <Adafruit_NeoPixel.h>
#include "led.h"

DataPacket datapacket;
ServoMotor sv(13); //构建舵机对象(选择13引脚为pwm输出引脚)
Motor mc(4,5); //构建电机对象(选择4和5引脚控制电机驱动)
Adafruit_NeoPixel leds(3, 12, NEO_GRB + NEO_KHZ800);//构建leds对象(3个led,12号引脚)
unsigned int FilamentIoPin = A0; //断料检测引脚(ADC=>A0)
String FilamentState = "none"; //断料状态
bool IsFPinHigh;//ADC是否高电平

void FilamentChange(bool ToStateIsHigh);

void setup() {
	Serial.begin(115200,SERIAL_8E1);
    Serial1.begin(115200,SERIAL_8E1);
    leds.begin();

    IsFPinHigh = (analogRead(A0) > 1000);
    if (IsFPinHigh){FilamentState = "exist";}
    else{FilamentState = "inexist";}
    leds.setBrightness(100);
    leds.clear();
    leds.show();
}

void loop() {
    if ((analogRead(A0) > 1000) != IsFPinHigh){
        FilamentChange(not IsFPinHigh);
        IsFPinHigh = not IsFPinHigh;
    }

    if (datapacket.index == 0 and Serial.read() == 0x3D){
        //Serial.println("帧头");
        datapacket.index += 1;
        datapacket.contentlength = 0;
        ledPC(0,0,255,0);
    }else if (Serial.available() and datapacket.index > 0 and datapacket.index <= 4){
        //Serial.println("读取序号,地址,长度,headCRC");
        switch (datapacket.index){
        case 1:
            datapacket.sequenceNumber = Serial.read();
            //Serial.println(datapacket.sequenceNumber,HEX);
            break;
            ledPC(0,0,0,0);
        case 2:
            datapacket.address = Serial.read();
            //Serial.println(datapacket.address,HEX);
            ledPC(0,0,255,0);
            break;
        case 3:
            datapacket.length = Serial.read();
            //Serial.println(datapacket.length,HEX);
            ledPC(0,0,0,0);
            break;
        case 4:
            datapacket.headCRC = Serial.read();
            //Serial.println(datapacket.headCRC,HEX);
            ledPC(0,0,255,0);
            break;
        }
        datapacket.index += 1;
    }else if (Serial.available() and datapacket.index == 5){
        //Serial.println("检验头");
        if (datapacket.headCheck()){
            datapacket.commandType = Serial.read();
            datapacket.index += 1;
            //Serial.println("检验头success|读取命令号");
            //Serial.println(datapacket.commandType,HEX);
            ledPC(0,0,0,0);
        }else{
            datapacket.index = 0;
            //Serial.println("检验头失败");
            ledPC(0,255,0,0);
        }
    }else if (Serial.available() and datapacket.index >= 6 and datapacket.index <= datapacket.length - 3){
        //Serial.println("读取内容");
        ledPC(0,0,255,0);
        datapacket.content[datapacket.contentlength] = Serial.read();
        //Serial.println(datapacket.content[datapacket.contentlength],HEX);
        datapacket.index += 1;
        datapacket.contentlength += 1;
    }else if (Serial.available() and datapacket.index >= datapacket.length - 2 and datapacket.index < datapacket.length){
        //Serial.println("读取总CRC");
        if (datapacket.index == datapacket.length - 2){
            datapacket.totalLowCRC = Serial.read();
            //Serial.println("读取低CRC");
            //Serial.println(datapacket.totalLowCRC,HEX);
            ledPC(0,0,0,0);
        }else if (datapacket.index == datapacket.length -1){
            datapacket.totalHighCRC = Serial.read();
            //Serial.println("读取高CRC");
            //Serial.println(datapacket.totalHighCRC,HEX);
            ledPC(0,0,255,0);
        }
        datapacket.index += 1;
    }else if (datapacket.index == datapacket.length and datapacket.index != 0){
        //Serial.println("进入最后处理");
        //Serial.println(FilamentState);
        if (datapacket.totalCheck()){
            //Serial.println("进入最后处理，success");
            ledPC(0,0,0,0);
            processData(datapacket);
            datapacket.index = 0;
        }else{
            //Serial.println("进入最后处理，fail");
            datapacket.index = 0;
            ledPC(0,255,0,0);
        }
    }
}

void FilamentChange(bool ToStateIsHigh){
    if (ToStateIsHigh){
        //低变高-无变有
        if (FilamentState == "none" or FilamentState == "inexist"){
            FilamentState = "exist";
        }else if (FilamentState == "busy"){
            mc.stop();
            sv.pull();
            FilamentState = "exist";
      }
    }else if(not ToStateIsHigh){
        //高变低-有变无
        if (FilamentState == "none" or FilamentState == "exist"){
            FilamentState = "inexist";
        }else if (FilamentState == "busy"){
            mc.forward();
        }
    }
}