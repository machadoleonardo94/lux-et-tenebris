#if !defined(UPDATE_FIRMWARE_OTA_WIFI)
#define UPDATE_FIRMWARE_OTA_WIFI

#include "shared/dependencies.h"

#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <DNSServer.h>

void setup_OTA()
{
  // Initialize OTA
  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_FS
      type = "filesystem";

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type); });

  ArduinoOTA.onEnd([]()
                   {
    Serial.println("\nEnd");
    ESP.restart(); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  //! This is called from setup_ESP32(), i.e. BEFORE the WiFi join, so ArduinoOTA
  //! has no interface hostname to inherit and falls back to "esp32-<mac>" in
  //! begin(). Worse, begin() starts the mDNS responder right there, and
  //! mdns_init() can only succeed once per boot - so this call, not the one in
  //! setup_webpage(), is what decides the name the board answers to. Without an
  //! explicit hostname the board lives at esp32-xxxxxxxxxxxx.local and
  //! NETWORK_HOSTNAME.local never resolves.
  ArduinoOTA.setHostname(NETWORK_HOSTNAME);

  ArduinoOTA.begin();
}

#endif // UPDATE_FIRMWARE_OTA_WIFI
