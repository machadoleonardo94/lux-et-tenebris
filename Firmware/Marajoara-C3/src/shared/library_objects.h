#if !defined(PROJECT_GLOBAL_OBJECTS)
#define PROJECT_GLOBAL_OBJECTS

#include "shared/dependencies.h"
#include "pinout.h"

//* ---------------------- GLOBAL OBJECT INSTANCES ----------------------

#define NUM_LEDS 144
#define MAJORAS_LEDS 28

Adafruit_NeoPixel strip(NUM_LEDS, strip_out, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel majora(MAJORAS_LEDS, majora_out, NEO_RGB + NEO_KHZ800); // 5mm PTH LEDs on Majora's Mask
Preferences preferences;

#endif // PROJECT_GLOBAL_OBJECTS
