#include "shared/dependencies.h"

void update_rgb_values(uint8_t colorCode);

void setup()
{
  //* --- SERIAL BEGIN ---
  //* Begin serial communication with Serial Monitor.
  Serial.begin(115200);

  Serial.println("\n---------------------------------------------------");
  Serial.println("------------------ Void Setup  --------------------");
  Serial.println("---------------------------------------------------\n");

  setup_ESP32();   //* Enables WiFi modem, disables BLE, enables watchdog
  setup_DISPLAY(); //* Enables debug display
  setup_WS2812B(); //* Enables debug WS2812B LED
  // setup_WIFI();    //* Setups AP for control
  WiFi.mode(WIFI_OFF); //* Disable WiFi for lower power consumption

  Serial.println("\n---------------------------------------------------");
  Serial.println("----------------  END OF SETUP --------------------");
  Serial.println("---------------------------------------------------\n");
  esp_task_wdt_reset();
}

void loop()
{
  ArduinoOTA.handle();

  update_channel0_states();
  // update_channel1_states();
  // update_channel2_states();
  // update_channel3_states();
  serial_outputs();
  esp_task_wdt_reset();

  if (digitalRead(prog_switch) == LOW)
  {
    uint32_t pressStart = millis();
    while (!digitalRead(prog_switch))
      if (millis() - pressStart > 2000) // Check if it was held for 2 seconds
        break;
    if (millis() - pressStart < 2000) // Check if it was held for 2 seconds
      return;
    Serial.println("[MAIN] Program switch detected! Restarting...");
    ESP.restart();
  }
}
