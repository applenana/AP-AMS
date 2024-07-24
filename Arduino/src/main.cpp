#include <Arduino.h>
#include "test.h"
#include "dataPacket.h"
#include "processData.h"

DataPacket datapacket;

void setup() {
  Serial.begin(112500);
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
      datapacket.content[datapacket.contentlength] = Serial.read();
      datapacket.contentlength += 1;
      datapacket.index += 1;
    }else{
      datapacket.index = 0;
    }
  }else if (Serial.read() != -1 and datapacket.index > 5 and datapacket.index < datapacket.length -2){
    datapacket.content[datapacket.contentlength] = Serial.read();
    datapacket.contentlength += 1;
    datapacket.index += 1;
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