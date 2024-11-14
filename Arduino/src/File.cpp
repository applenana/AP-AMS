#include "File.h"

void WriteByteIntoConfig(const byte* content, uint8_t contentLength){
    File file = LittleFS.open("/config.bin", "w");
    file.write(content, contentLength);
    file.close();
}

void ReadContentFromConfig(byte* content, size_t length){
    File file = LittleFS.open("/config.bin", "r");
    file.read(content, length);
    file.close();
}