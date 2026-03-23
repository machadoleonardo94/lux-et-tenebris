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
//* Global variables
#include <variables.h>

//* Objects
#define NUM_LEDS 196
Adafruit_NeoPixel strip(NUM_LEDS, strip_pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel onboard_led(1, status_led_pin, NEO_GRB + NEO_KHZ800);
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

  onboard_led.begin();
  onboard_led.show(); // Initialize all pixels to 'off'
  Serial.println("Status LED initialized");
}

int position = 0;
int power = 5;

void loop()
{
  power = (sin(millis() / 200.0) * 15) + 15; // Calculate brightness based on sine wave (0-50 range)

  if (millis() - led_strip.update_time >= 5)
  {
    led_strip.update_time = millis();
    strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
    strip.show();
    position++;
    position %= NUM_LEDS;
  }

  if (millis() - status_led.update_time >= 5)
  {
    status_led.update_time = millis();
    int brightness = power / 3;                                                    // Scale down brightness for status LED
    onboard_led.setPixelColor(0, strip.Color(brightness, brightness, brightness)); // White status LED with same brightness
    onboard_led.show();
  }
}
