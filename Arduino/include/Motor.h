#include <Arduino.h>
#pragma once

class Motor
{
private:
    int pin1;
    int pin2;
    bool isStop = true;
    String state = "停止";
    unsigned long startTime = 0;
    unsigned long timeout = 0;
    bool banMotor = false;

public:
    Motor(int pin1, int pin2);
    void forward(unsigned long);
    void backforward(unsigned long);
    void stop();
    bool getStopState();
    String getState();
    void update();
    void setBanMotor(bool banMotor);
};
