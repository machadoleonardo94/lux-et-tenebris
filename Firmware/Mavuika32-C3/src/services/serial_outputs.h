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
        Serial.printf("->| GYRO     | Accel_X: %.2f | Accel_Y: %.2f | Accel_Z: %.2f |\n",
                      a.acceleration.x, a.acceleration.y, a.acceleration.z);
        Serial.printf("->| GYRO     | Gyro_X: %.2f  | Gyro_Y: %.2f  | Gyro_Z: %.2f |\n",
                      g.gyro.x, g.gyro.y, g.gyro.z);
        Serial.printf("\n\n");
    }

    //* Teleplot outputs
    /*
    Serial.printf(">acc_x: %.2f \n", a.acceleration.x);
    Serial.printf(">acc_y: %.2f \n", a.acceleration.y);
    Serial.printf(">acc_z: %.2f \n", a.acceleration.z);
    Serial.printf(">gyro_x: %.2f \n", g.gyro.x);
    Serial.printf(">gyro_y: %.2f \n", g.gyro.y);
    Serial.printf(">gyro_z: %.2f \n", g.gyro.z);
    */
}

#endif // SERVICE_SERIAL_READINGS
