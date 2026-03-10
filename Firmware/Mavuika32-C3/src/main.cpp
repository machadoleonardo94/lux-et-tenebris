//! Warning: GoHorse Develpment method was used to create this code.
//! It's presented as an "it just works" solution for Arduino users migrating to VSCode
//! and is not intended to be a best practice example of C++ programming.

//? Lux et Portabilis - Mavuika Let it Go firmware
//? Basic devboard firmware for smart LED strips, accelerometer, buttons and servo output.

//* Framework
#include <Arduino.h>
//* Pinout
#include "pinout.h"
//* Dependencies
//* Smart LED strip control
#include <Adafruit_NeoPixel.h>
//* Non-volatile Storage
#include <Preferences.h>
#include <LittleFS.h>
//* WiFi
#include <WiFi.h>
//* Gobal variables
#include <variables.h>

//* Objects
#define NUM_LEDS 40
Adafruit_NeoPixel strip(NUM_LEDS, strip_pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel status_led(1, status_led_pin, NEO_GRB + NEO_KHZ800);
Preferences preferences;

void setup()
{
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nESP32-C3 LED Control Started");
  Serial.println("===========================");

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED strip initialized");
}

int position = 0;
int power = 5;

void loop()
{
  power = (sin(millis() / 1000.0) * 25 + 30);              // Vary power between 5 and 55
  strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
  strip.show();
  position++;
  position %= NUM_LEDS;
  delay(25);
}
