#if !defined(SERVICE_SERIAL_READINGS)
#define SERVICE_SERIAL_READINGS

#include "shared/dependencies.h"

void serial_outputs()
{
    loopCounter++;

    if (millis() - loopTimer > 10000)
    {
        Serial.printf("\n\nTotal main loops per 10 seconds: %d \n", loopCounter);
        loopTimer = millis();
        loopCounter = 0;

        String tempIP = WiFi.localIP().toString();
        int8_t wifi_rssi = WiFi.RSSI();

        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAdress[0], macAdress[1], macAdress[2],
                 macAdress[3], macAdress[4], macAdress[5]);
        String tempMac = String(macBuf);

        Serial.printf("\n\n");
        Serial.printf("->| HARDWARE | IP: %s | MAC: %s | RSSI: %d | \n", tempIP, tempMac.c_str(), wifi_rssi);
        Serial.printf("\n\n");
    }
}

#endif // SERVICE_SERIAL_READINGS
