#include "led.h"

void ledAll(unsigned int r,unsigned int g,unsigned int b){
    leds.fill(leds.Color(r,g,b));
    leds.show();
}

void ledPC(uint pixels,uint r,uint g,uint b){
    leds.setPixelColor(pixels,leds.Color(r,g,b));
    leds.show();
}