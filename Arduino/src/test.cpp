#include "test.h"

void sendTestPack(){
    uint8_t frameHeader = 0x3D;
    uint8_t packetNumber = 10;// 包序号
    uint8_t address = 10;// 包地址
    uint8_t commandType = 0x01; // 命令号
    uint8_t content[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}; // 包内容
    uint8_t packetLength = sizeof(content) + 8; // 包长度

    // 构建数据包
    uint8_t packet[packetLength]; // 包头部分 6 个字节 + 内容 8 个字节
    packet[0] = frameHeader;
    packet[1] = packetNumber;
    packet[2] = address;
    packet[3] = packetLength;
    packet[5] = commandType;
    memcpy(packet + 6, content, sizeof(content)); // 复制内容

    // 计算头校验
    CRC8 crc8(0x39,0x66,0x00,false,false);
    crc8.add(packet,4);
    packet[4] = crc8.calc(); // 计算前 4 个字节的 CRC8 校验

    // 计算总校验
    CRC16 crc16(0x1021, 0x913D, 0x0000, false, false); // 多项式 0x1021，初始值 0x913D，无反转
    crc16.add(packet, sizeof(packet) - 2); // 计算前 12 个字节的 CRC16 校验
    uint16_t totalCRC = crc16.calc();
    packet[12] = totalCRC & 0xFF; // 低字节
    packet[13] = (totalCRC >> 8) & 0xFF; // 高字节

    // 发送数据包
    Serial.write(packet, sizeof(packet));   
}