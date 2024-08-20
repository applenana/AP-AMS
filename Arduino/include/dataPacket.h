#include "CRC.h"
#include "Motor.h"
#include "ServoMotor.h"
#include "led.h"
#pragma once

class DataPacket {
private:
    CRC8 crc8;
    CRC16 crc16;
public:
    uint8_t index;             // 索引
    int8_t sequenceNumber;     // 序号
    int8_t address;            // 地址
    uint8_t length;            // 长度
    uint8_t commandType;       // 命令号
    byte content[512];         // 内容(至多512字节)
    uint8_t headCRC;           // 头检验
    uint8_t totalLowCRC;       // 总低检验
    uint8_t totalHighCRC;      // 总高检验
    uint8_t contentlength = 0; // 内容长度[作索引用]

    DataPacket();  // 构造函数声明
    bool headCheck();
    bool totalCheck();
    void sendPacket(bool ISIO2OUT = false); 
};