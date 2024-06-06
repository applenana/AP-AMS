#include <unity_config.h>

#if !defined(UNITY_WEAK_ATTRIBUTE) && !defined(UNITY_WEAK_PRAGMA)
#   if defined(__GNUC__) || defined(__ghs__) /* __GNUC__ includes clang */
#       if !(defined(__WIN32__) && defined(__clang__)) && !defined(__TMS470__)
#           define UNITY_WEAK_ATTRIBUTE __attribute__((weak))
#       endif
#   endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef UNITY_WEAK_ATTRIBUTE
    UNITY_WEAK_ATTRIBUTE void setUp(void) { }
    UNITY_WEAK_ATTRIBUTE void tearDown(void) { }
    UNITY_WEAK_ATTRIBUTE void suiteSetUp(void) { }
    UNITY_WEAK_ATTRIBUTE int suiteTearDown(int num_failures) { return num_failures; }
#elif defined(UNITY_WEAK_PRAGMA)
    #pragma weak setUp
    void setUp(void) { }
    #pragma weak tearDown
    void tearDown(void) { }
    #pragma weak suiteSetUp
    void suiteSetUp(void) { }
    #pragma weak suiteTearDown
    int suiteTearDown(int num_failures) { return num_failures; }
#endif

#ifdef __cplusplus
}
#endif /* extern "C" */


#include <Arduino.h>
void unityOutputStart(unsigned long baudrate) { Serial.begin(baudrate); }
void unityOutputChar(unsigned int c) { Serial.write(c); }
void unityOutputFlush(void) { Serial.flush(); }
void unityOutputComplete(void) { Serial.end(); }
