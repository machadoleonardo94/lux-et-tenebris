#ifndef PROJECT_GLOBAL_DEPENDENCIES
#define PROJECT_GLOBAL_DEPENDENCIES

//? ---------------------- Framework --------------------------
#include <Arduino.h>

//* ----------------------- SHARED ----------------------------
//* Pinout
#include "pinout.h"

//* ---------------------- Libraries --------------------------
//* WiFi
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
//* Watchdog
#include <esp_task_wdt.h>
//* Non-volatile Storage
#include <Preferences.h>
#include <LittleFS.h>
//* Over the Air update
#include <ArduinoOTA.h>
//* Fancy LED WS2812B
#include <Adafruit_NeoPixel.h>
//* Timezone
#include <time.h>
//* Gyroscope and Accelerometer
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

//* Library Objects
// After libraries, before functions
#include "library_objects.h"
//* Variables
#include "variables.h"

//* ---------------------- PROJECT FIRMWARE IMPLEMENTATIONS --------------------------
//* Utilities:
// If any
//* Components:
#include "components/ESP32/setup.h"

//* Services:
#include "services/gyros.h"
#include "services/led_scripts.h"
#include "services/serial_outputs.h"
#include "services/wifi_settings.h"

#endif