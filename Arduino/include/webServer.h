#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "modeInfo.h"

void recWebServer();
extern const char *version;
extern uint8_t backStopTime;
extern AsyncWebServer server;