from machine import Pin
import time
led = Pin(2,Pin.OUT)
led.value(1)

def spark():
    time.sleep(0.05)
    led.value(0)
    time.sleep(0.05)
    led.value(1)
    time.sleep(0.05)
    led.value(0)
    time.sleep(0.05)
    led.value(1)