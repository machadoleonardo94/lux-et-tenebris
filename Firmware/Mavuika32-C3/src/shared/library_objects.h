#if !defined(PROJECT_GLOBAL_OBJECTS)
#define PROJECT_GLOBAL_OBJECTS

#include "shared/dependencies.h"
#include "pinout.h"

//* ---------------------- GLOBAL OBJECT INSTANCES ----------------------

#define NUM_LEDS 37
#define MAJORAS_LEDS 28

Adafruit_NeoPixel strip(NUM_LEDS, strip_pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel onboard_led(1, status_led_pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel majora(MAJORAS_LEDS, freeGPIOA, NEO_RGB + NEO_KHZ800); // 5mm PTH LEDs on Majora's Mask
Preferences preferences;
Adafruit_MPU6050 mpu;

#endif // PROJECT_GLOBAL_OBJECTS
