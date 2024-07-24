#include "ServoMotor.h"

ServoMotor::ServoMotor(int servoPin){
    servo.attach(servoPin,500,2500);}

void ServoMotor::push(){
    servo.write(0);
    angle = 0;
    state = "推";
}

void ServoMotor::pull(){
    servo.write(180);
    angle = 180;
    state = "拉";
}

void ServoMotor::writeAngle(int angle){
    servo.write(angle);
    angle = angle;
    state = "自定义角度";
}

int ServoMotor::getAngle(){
    return angle;
}

String ServoMotor::getState(){
    return state;
}