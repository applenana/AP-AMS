#include <Adafruit_NeoPixel.h>
#pragma once

class Led{
public:
    Led(int ledPixels,int ledPin);
    Adafruit_NeoPixel leds;
    void All(unsigned int r, unsigned int g, unsigned int b);
};
