#include <Arduino.h>
#pragma once

class Motor
{
private:
    uint8_t pin1;
    uint8_t pin2;
    bool isStop = true;
    String state = "停止";
    unsigned long startTime = 0;
    unsigned long timeout = 0;
    bool banMotor = false;

public:
    Motor(uint8_t pin1, uint8_t pin2);
    void forward(unsigned long);
    void backforward(unsigned long);
    void stop();
    bool getStopState();
    String getState();
    void update();
    void setBanMotor(bool banMotor);
};
