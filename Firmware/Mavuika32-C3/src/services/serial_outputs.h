#if !defined(SERVICE_SERIAL_READINGS)
#define SERVICE_SERIAL_READINGS

#include "shared/dependencies.h"

void serial_outputs()
{
    loopCounter++;

    //? Previous detector sample count, so the report below can turn the counter into
    //? a rate over the same 10 s window the loop count uses.
    static uint32_t impact_samples_last = 0;

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

        //* Detector health. The rate the loop actually achieved is not something the
        //* firmware can assume: with WiFi, a web request or an OTA transfer in
        //* flight, loop() can run far slower than the 500 Hz the detector targets,
        //! and a rate below ~100 Hz is what makes impacts get missed or undersized.
        const uint32_t samples = impact_samples - impact_samples_last;
        impact_samples_last = impact_samples;
        if (impact_count == 0)
            Serial.printf("->| IMPACT   | %lu samples/s | none detected yet |\n",
                          (unsigned long)(samples / 10UL));
        else
            Serial.printf("->| IMPACT   | %lu samples/s | %lu total | last magnitude %u, %lu ms ago |\n",
                          (unsigned long)(samples / 10UL), (unsigned long)impact_count,
                          impact_last_magnitude, (unsigned long)(millis() - impact_last_ms));
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
