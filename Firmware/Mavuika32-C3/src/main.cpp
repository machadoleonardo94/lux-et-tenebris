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
//* Gyroscope and Accelerometer
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
//* Global variables
#include <variables.h>

//* Objects
#define NUM_LEDS 144
Adafruit_NeoPixel strip(NUM_LEDS, strip_pin, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel onboard_led(1, status_led_pin, NEO_GRB + NEO_KHZ800);
Preferences preferences;
Adafruit_MPU6050 mpu;

//? strip: the NeoPixel object for controlling external LED strip
//? led_strip: the struct holding the current state of the LED strip (color, brightness, etc.)
//? onboard_led: the NeoPixel object for controlling the onboard status LED
//? status_led: the struct holding the current state of the onboard status LED

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

  Wire.begin(i2c_sda, i2c_scl);
  Serial.printf("I2C initialized (SDA=%d, SCL=%d)\n", i2c_sda, i2c_scl);

  if (!mpu.begin(0x68, &Wire))
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
      ;
  }
}

void loop()
{
  // power = (sin(millis() / 200.0) * 15) + 15; // Calculate brightness based on sine wave (0-30 range)

  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  if (millis() - led_strip.update_time >= 5)
  {
    led_strip.update_time = millis();

    /* Print out the values */
    Serial.print("Acceleration X: ");
    Serial.print(a.acceleration.x);
    Serial.print(", Y: ");
    Serial.print(a.acceleration.y);
    Serial.print(", Z: ");
    Serial.print(a.acceleration.z);
    Serial.println(" m/s^2");
    if (a.acceleration.x > 1.0 || a.acceleration.x < -1.0)
    {
      power = 10 + abs(a.acceleration.x); // Full brightness if there's significant movement
    }
    else
    {
      power = 5; // Dim when stationary
    }

    for (int i = 0; i < 20 * abs(a.acceleration.x); i++)
    {
      strip.setPixelColor(i, strip.Color(power, 0, power)); // Red
    }
    for (int i = 20 * abs(a.acceleration.x); i < NUM_LEDS; i++)
    {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();

    //* Sine wave brightness
    /*
    strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
    strip.show();
    position++;
    position %= NUM_LEDS;
    */
  }

  if (millis() - status_led.update_time >= 5)
  {
    status_led.update_time = millis();
    int brightness = abs(a.acceleration.z);                      // Scale down brightness for status LED
    onboard_led.setPixelColor(0, strip.Color(0, brightness, 0)); // Green status LED with same brightness
    onboard_led.show();
  }
}
