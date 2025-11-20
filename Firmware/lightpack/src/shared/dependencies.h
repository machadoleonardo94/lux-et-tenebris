//? ------------------------------------------------------------------------------------------
//*! This file contains all the common definitions and includes for the project.
//? ------------------------------------------------------------------------------------------

#ifndef PROJECT_GLOBAL_DEPENDENCIES
#define PROJECT_GLOBAL_DEPENDENCIES

#define firmware_version 1

//? ---------------------- Framework --------------------------
#include <Arduino.h>

//* ----------------------- SHARED ----------------------------
//* Constants
#include "constants.h"
//* Pinout
#include "pinout.h"

//* ---------------------- Libraries --------------------------
//* OLED I2C display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//* WiFi
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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
#include "components/DISPLAY/setup.h"
#include "components/WS2812B/setup.h"

//* Services:
#include "services/led_scripts.h"
#include "services/serial_outputs.h"
#include "services/wifi_settings.h"
#include "services/user_interface.h"

#endif