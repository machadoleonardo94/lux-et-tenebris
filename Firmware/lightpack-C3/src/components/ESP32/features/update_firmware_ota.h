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
    Serial.flush();
    //! The restart that used to live here is gone, and removing it is a fix, not
    //! a tidy-up. ArduinoOTA::_runUpdate() queues its final "OK" for the uploader
    //! and then stops the client, and this callback runs TEN MILLISECONDS later -
    //! so restarting here pulled the radio out from under a reply that had not
    //! left the board yet. Measured on this device: `pio run -e C3-OTA -t upload`
    //! reported "Error receiving result: timed out" on updates that had in fact
    //! been applied and booted, because the host never heard the OK.
    //? 10 ms is a hopeless budget on a weak link. This board's round trip was
    //? 86 ms at -78 dBm, and the whole image had just been written, so the reply
    //? was still queued when the reset wiped the TCP stack.
    //* ArduinoOTA already restarts by itself after this callback returns
    //* (_rebootOnSuccess defaults to true, and it allows 100 ms) - so the board
    //? still reboots, it just does so after a window the reply can survive. The
    //* extra delay below widens that window to about 600 ms for the same reason.
    delay(500); });

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

  //? Called from main.cpp immediately after a successful WiFi join, not from
  //! setup_ESP32() as it used to be. Both of the things begin() starts need a
  //? station interface to exist: the OTA UDP socket, and the mDNS responder that
  //? begin() brings up on its own because _mdnsEnabled defaults to true. Run
  //? before the join they bound to the softAP instead, and the board ended up
  //! deaf to espota invitations and answering to no name at all - see the note in
  //! components/ESP32/setup.h for the measurements.
  //* The explicit hostname still matters for the same reason it always did:
  //! begin() falls back to "esp32-<mac>" when no hostname was set, and that is
  //* the name the board would then answer to at NETWORK_HOSTNAME.local.
  ArduinoOTA.setHostname(NETWORK_HOSTNAME);

  ArduinoOTA.begin();
}

#endif // UPDATE_FIRMWARE_OTA_WIFI
