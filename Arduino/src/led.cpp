#include "led.h"

Led::Led(int ledPixels,int ledPin){
    Adafruit_NeoPixel leds(ledPixels, ledPin, NEO_GRB + NEO_KHZ800);
}

void Led::All(unsigned int r, unsigned int g, unsigned int b){
    leds.fill(leds.Color(r,g,b));
    leds.show();
}