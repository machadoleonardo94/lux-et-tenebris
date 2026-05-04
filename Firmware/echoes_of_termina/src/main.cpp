#include "shared/dependencies.h"

void setup()
{
  //* --- SERIAL BEGIN ---
  //* Begin serial communication with Serial Monitor.
  Serial.begin(115200);

  Serial.println("\n---------------------------------------------------");
  Serial.println("------------------ Void Setup  --------------------");
  Serial.println("---------------------------------------------------\n");

  setup_ESP32(); //* Enables watchdog
  setup_GPIOs(); //* Configures GPIOs

  if (digitalRead(button_input) == HIGH)
  {
    Serial.println("Enabling WiFi AP for debug\n");
    debugWifi = setup_WIFI(); //* Setups AP for control
  }
  else
  {
    WiFi.mode(WIFI_OFF);
  }

  // Streams audio data to the ESP32
  audio.begin();

  // Re-connects to last connected device
  audio.reconnect();

  // Outputs the received data to an I2S DAC MAX98357A
  audio.I2S(I2C_BCLK, I2C_DOUT, I2C_LRC);
  audio.volume(1.0);

  Serial.println("\n---------------------------------------------------");
  Serial.println("----------------  END OF SETUP --------------------");
  Serial.println("---------------------------------------------------\n");
  esp_task_wdt_reset();
}

void loop()
{
  if (debugWifi) // Wifi is disabled unless debug mode is enabled to avoid ADC interference
    ArduinoOTA.handle();

  esp_task_wdt_reset();
}
