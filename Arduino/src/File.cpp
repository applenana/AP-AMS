#include "File.h"

void WriteByteIntoConfig(const byte *content, uint8_t contentLength)
{
    File file = LittleFS.open("/config.bin", "w");
    file.write(content, contentLength);
    file.close();
}

void ReadContentFromConfig(byte *content, size_t length)
{
    File file = LittleFS.open("/config.bin", "r");
    file.read(content, length);
    file.close();
}

// 写入模式配置文件
void WriteModeConfig(int modeNum)
{
    File file = LittleFS.open("/config_mod.bin", "w");
    // Serial.println("WriteModeConfig:" + String((uint8_t)status));
    file.write((uint8_t)modeNum);
    file.close();
}
// 读取模式配置文件
int ReadModeFromConfig()
{
    File file = LittleFS.open("/config_mod.bin", "r");
    if (file)
    {
        uint8_t buffer;
        file.read(&buffer, 1);
        // Serial.println("ReadModeFromConfig:" + String(buffer));
        // Serial.println("ReadModeFromConfigbytesRead:" + String(bytesRead));

        file.close();
        return int(buffer);
    }
    WriteModeConfig(1);
    file.close();
    return 1;
}