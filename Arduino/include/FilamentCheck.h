#include <Arduino.h>

class FilamentCheck
{
public:
    String state = "None";
    FilamentCheck(unsigned int IOPIN);
};


unsigned int FilamentIoPin;
void FilamentChange();
