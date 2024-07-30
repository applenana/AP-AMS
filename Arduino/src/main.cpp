#include <Arduino.h>
#include "dataPacket.h"
#include "processData.h"
#include "ServoMotor.h"
#include "Motor.h"

DataPacket datapacket;
ServoMotor sv(13); //构建舵机对象(选择13引脚为pwm输出引脚)
Motor mc(4,5); //构建电机对象(选择4和5引脚控制电机驱动)
unsigned int FilamentIoPin = A0; //断料检测引脚(ADC=>A0)
String FilamentState = "none"; //断料状态

void FilamentChange();//断料检测中断函数

void setup() {
  Serial.begin(112500);

  //设置断料检测
  pinMode(FilamentIoPin, INPUT_PULLDOWN_16);
  attachInterrupt(FilamentIoPin,FilamentChange,CHANGE);
}

void loop() {
  if (datapacket.index == 0 and Serial.read() == 0x3D){
    datapacket.index += 1;
  }else if (Serial.read() != -1 and datapacket.index > 0 and datapacket.index <= 4){
    switch (datapacket.index){
    case 1:
      datapacket.sequenceNumber = Serial.read();
      break;
    case 2:
      datapacket.address = Serial.read();
      break;
    case 3:
      datapacket.length = Serial.read();
      break;
    case 4:
      datapacket.headCRC = Serial.read();
      break;
    }
    datapacket.index += 1;
  }else if (Serial.read() != -1 and datapacket.index == 5){
    if (datapacket.totalCheck()){
      datapacket.content = Serial.read();
      datapacket.contentlength += 1;
      datapacket.index += 1;
    }else{
      datapacket.index = 0;
    }
  }else if (Serial.read() != -1 and datapacket.index >= datapacket.length - 2 and datapacket.index < datapacket.length){
    datapacket.index += 1;
    if (datapacket.index == datapacket.length - 2){
      datapacket.totalLowCRC = Serial.read();
    }else if (datapacket.index == datapacket.length -1){
      datapacket.totalHighCRC = Serial.read();
    }
  }else if (datapacket.index == datapacket.length){
    if (datapacket.totalCheck()){
      processData(datapacket);
      datapacket.index = 0;
    }else{
      datapacket.index = 0;
    }
  }
}

void FilamentChange(){  
  if (digitalRead(FilamentIoPin) == HIGH){
      //低变高-无变有
      if (FilamentState == "none" or FilamentState == "inexist"){
          FilamentState = "exist";
      }else if (FilamentState == "busy"){
          mc.stop();
          sv.pull();
          FilamentState = "exist";
      }
  }else if(digitalRead(FilamentIoPin) == LOW){
      //高变低-有变无
      if (FilamentState == "none" or FilamentState == "exist"){
          FilamentState = "inexist";
      }else if (FilamentState == "busy"){
          mc.forward();
      }
  }
}