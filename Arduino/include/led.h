#pragma once
#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel leds;
void ledAll(unsigned int r,unsigned int g,unsigned int b);
void ledPC(uint pixels,uint r,uint g,uint b);
