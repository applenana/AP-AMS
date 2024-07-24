#include <Servo.h>
#include <Arduino.h>
#pragma once

class ServoMotor
{
private:
    int angle = -1;
    String state = "自定义角度";
    Servo servo;
public:
    ServoMotor(int servoPin);
    void push();
    void pull();
    void writeAngle(int angle);
    int getAngle();
    String getState();
};