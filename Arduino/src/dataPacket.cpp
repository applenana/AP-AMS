#include "dataPacket.h"

DataPacket::DataPacket() 
    : crc8(0x39, 0x66, 0x00, false, false), 
      crc16(0x1021, 0x913D, 0x0000, false, false) 
{
}

bool DataPacket::headCheck(){
    byte head[] = {0x3D,sequenceNumber,address,length};
    crc8.restart();
    crc8.add(head,4);
    uint8_t result = crc8.calc();
    return (result == headCRC);
}

bool DataPacket::totalCheck(){
    byte* packet = new byte[length - 2];
    packet[0] = 0x3D;
    packet[1] = sequenceNumber;
    packet[2] = address;
    packet[3] = length;
    packet[4] = headCRC;
    packet[5] = commandType;
    memcpy(packet + 6,content,length - 8);
    crc16.restart();
    crc16.add(packet, length - 2);
    uint16_t totalCRC = crc16.calc();
    uint8_t lowCRC = totalCRC & 0xFF; // 低字节
    uint8_t highCRC = (totalCRC >> 8) & 0xFF; // 高字节
    delete[] packet;
    return (lowCRC == totalLowCRC and highCRC == totalHighCRC);
}

void DataPacket::sendPacket(bool ISIO2OUT){
    byte head[] = {0x3D,sequenceNumber,address,length};
    crc8.restart();
    crc8.add(head,4);
    headCRC = crc8.calc();
    byte* packet = new byte[length];
    packet[0] = 0x3D;
    packet[1] = sequenceNumber;
    packet[2] = address;
    packet[3] = length;
    packet[4] = headCRC;
    packet[5] = commandType;
    memcpy(packet + 6,content,length - 8);
    crc16.restart();
    crc16.add(packet, length - 2);
    uint16_t totalCRC = crc16.calc();
    totalLowCRC = totalCRC & 0xFF; // 低字节
    totalHighCRC = (totalCRC >> 8) & 0xFF; // 高字节
    packet[length - 2] = totalLowCRC;
    packet[length - 1] = totalHighCRC;
    if (ISIO2OUT){
        pinMode(2,SPECIAL);
        Serial1.write(packet, length);
        //delay(1);
        Serial1.flush();
        pinMode(2,INPUT);
        ledPC(1,0,255,0);
    }else{
        Serial.write(packet, length);
        ledPC(1,0,0,255);
        }
    delay(1);
    delete[] packet;
    ledPC(1,0,0,0);
}