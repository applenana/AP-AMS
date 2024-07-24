#include <Arduino.h>
#pragma once

class Motor{
private:
    int pin1;
    int pin2;
    bool isStop = true;
    String state = "停止";
public:
    Motor(int pin1, int pin2);
    void forward();
    void backforward();
    void stop();
    bool getStopState();
    String getState();
};
