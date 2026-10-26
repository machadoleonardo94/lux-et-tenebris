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
//* Reset reason. Read once at the top of setup() by main.cpp to report WHY the
//? previous run ended. A brown-out, a panic and a software restart are three
//! different faults with three different fixes, and the boot log is the only
//* place that fact is still available - after one more reset it is gone.
#include <esp_system.h>
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
#include "services/i2c_scan.h"
#include "services/gyros.h"
#include "services/readings.h"
#include "services/led_scripts.h"
#include "services/serial_outputs.h"
#include "services/wifi_settings.h"
#include "services/webpage.h"
//* Firmware upload from the page. Comes after services/webpage.h for the same
//* reason as presets.h below: it needs web_server, the JSON helper and the NVS
//* writer from there, and webpage.h only forward-declares its handlers for the
//* route table and webpage_loop().
#include "components/ESP32/features/update_firmware_web.h"
//* Presets come after the page: their handlers are registered by setup_webpage()
//* and they reuse its JSON helper and configuration writer.
#include "services/presets.h"

#endif