#pragma once
#include <LittleFS.h>

void WriteByteIntoConfig(const byte *content, uint8_t contentLength);
void ReadContentFromConfig(byte *content, size_t length);

// 写入模式配置文件
void WriteModeConfig(bool status);

// 读取模式配置文件
bool ReadModeFromConfig();