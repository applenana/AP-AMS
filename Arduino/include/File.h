#pragma once
#include <LittleFS.h>

void WriteByteIntoConfig(const byte* content, uint8_t contentLength);
void ReadContentFromConfig(byte* content, size_t length);