#if !defined(PROJECT_GLOBAL_OBJECTS)
#define PROJECT_GLOBAL_OBJECTS

#include "shared/dependencies.h"
#include "pinout.h"

//* ---------------------- GLOBAL OBJECT INSTANCES ----------------------

//* 0.96" OLED I2C display
Adafruit_SSD1306 display(128, 64, &Wire, -1);

//* Fancy LED WS2812B
// Adafruit_NeoPixel led_onboard(6, WS_channel0, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led_zonas(8, WS_channel1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led_bar1(4, WS_channel2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led_bar2(4, WS_channel3, NEO_GRB + NEO_KHZ800);

//* Preferences (NVS)
Preferences preferences;

#endif // PROJECT_GLOBAL_OBJECTS
