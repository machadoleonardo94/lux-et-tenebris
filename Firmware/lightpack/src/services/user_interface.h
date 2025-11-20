#if !defined(SERVICE_USER_INTERFACE)
#define SERVICE_USER_INTERFACE

#include "shared/dependencies.h"

void check_power_button();

void check_power_button()
{
    // Implementation for checking power button state
    if (digitalRead(prog_switch) == LOW)
    {
        uint32_t pressStart = millis();
        while (!digitalRead(prog_switch))
            if (millis() - pressStart > 2000) // Check if it was held for 2 seconds
                break;
        if (millis() - pressStart < 2000) // Short press, ignore
            return;
        // Flash all LEDs red to indicate power saving mode
        for (int j = 0; j < 10; j++)
        {
            for (int i = 0; i < ONBOARD_LEDS; i++)
            {
                onboard_circle.setPixelColor(i, onboard_circle.Color(100 * ((j + 1) % 2), 0, 0)); // Red color
            }
            onboard_circle.show();
            delay(200);
        }
        Serial.println("[MAIN] Program switch detected! Deep sleep mode activated...");
        delay(100);
        esp_deep_sleep_start();
    }
}

#endif