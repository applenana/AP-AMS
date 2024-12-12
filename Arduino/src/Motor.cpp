#include "Motor.h"


Motor::Motor(uint8_t pin1, uint8_t pin2)

{
    this->pin1 = pin1;
    this->pin2 = pin2;
    pinMode(pin1, OUTPUT);
    pinMode(pin2, OUTPUT);
}

void Motor::forward(unsigned long timeout)
{
    if (!banMotor && isStop)
    {
        digitalWrite(pin1, HIGH);
        digitalWrite(pin2, LOW);
        isStop = false;
        state = "前进";
        this->startTime = millis(); // 记录当前时间
        this->timeout = timeout;    // 设置超时时间
    }
}

void Motor::backforward(unsigned long timeout)
{
    if (!banMotor && isStop)
    {
        digitalWrite(pin1, LOW);
        digitalWrite(pin2, HIGH);
        isStop = false;
        state = "后退";
        this->startTime = millis(); // 记录当前时间
        this->timeout = timeout;    // 设置超时时间
    }
}

void Motor::stop()
{
    digitalWrite(pin1, HIGH);
    digitalWrite(pin2, HIGH);
    isStop = true;
    state = "停止";
}

bool Motor::getStopState()
{
    return isStop;
}

String Motor::getState()
{
    return state;
}

void Motor::setBanMotor(bool banMotor)
{
    this->banMotor = banMotor;
}

void Motor::update()
{
    if (!isStop && (millis() - startTime >= timeout))
    {
        banMotor = true;
        // Serial.println("超时停止");
        stop();
    }
}