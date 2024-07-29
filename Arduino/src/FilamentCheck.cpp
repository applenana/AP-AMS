#include "FilamentCheck.h"

FilamentCheck::FilamentCheck(unsigned int IOPIN){
    FilamentIoPin = IOPIN;
    pinMode(FilamentIoPin, INPUT_PULLDOWN_16);
    attachInterrupt(FilamentIoPin,FilamentChange,CHANGE);
}

void FilamentChange(){
    if (digitalRead(FilamentIoPin) == HIGH){
        //低变高-无变有
    }else if(digitalRead(FilamentIoPin) == LOW){
        //低变高-无变有
    }
}