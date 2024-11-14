#include "dataPacket.h"
#include "Motor.h"
#include "ServoMotor.h"
#pragma once

extern Motor mc;
extern ServoMotor sv;
extern String FilamentState;
extern bool Is5sPull;
void processData(DataPacket data);