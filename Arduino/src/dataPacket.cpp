#include "dataPacket.h"

DataPacket::DataPacket() 
    : crc8(0x39, 0x66, 0x00, false, false), 
      crc16(0x1021, 0x913D, 0x0000, false, false) 
{
    // 可以在这里初始化其他成员变量
}

bool DataPacket::headCheck(){
    byte head[] = {0x3D,sequenceNumber,address,length};
    crc8.restart();
    crc8.add(head,4);
    uint8_t result = crc8.calc();
    return (result == headCRC);
}

bool DataPacket::totalCheck(){
    byte packet[length - 3] = {0x3D,sequenceNumber,address,length,headCRC,commandType};
    memcpy(packet + 6,content,contentlength);
    crc16.restart();
    crc16.add(packet, sizeof(packet)); // 计算前 12 个字节的 CRC16 校验
    uint16_t totalCRC = crc16.calc();
    uint8_t lowCRC = totalCRC & 0xFF; // 低字节
    uint8_t highCRC = (totalCRC >> 8) & 0xFF; // 高字节
    return (lowCRC == totalLowCRC and highCRC == totalHighCRC);
}