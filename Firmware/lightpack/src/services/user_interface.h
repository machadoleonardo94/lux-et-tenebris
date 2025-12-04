#if !defined(SERVICE_USER_INTERFACE)
#define SERVICE_USER_INTERFACE

#include "shared/dependencies.h"

void check_power_button();

void check_power_button()
{
    // Implementation for checking power button state
    button_states[0].currentState = digitalRead(prog_switch);
    if (button_states[0].currentState != button_states[0].lastState)
    {
        button_states[0].lastDebounceTime = millis();
        Serial.printf("[UI] Prog button: state changed to %d\n", button_states[0].currentState);
        if (button_states[0].currentState == LOW)
            button_states[0].counter++;
    }
    button_states[0].lastState = button_states[0].currentState;
    if (button_states[0].counter >= 3)
    {
        if (WiFi.getMode() != WIFI_OFF)
        {
            WiFi.mode(WIFI_OFF);
            Serial.println("[UI] WiFi disabled to save power.");
            blink_circle(0x3, 3, 200); // Blink RGB LED in orange 3 times
        }
        else
            setup_WIFI();

        button_states[0].counter = 0;
        return;
    }
    if (millis() - button_states[0].lastDebounceTime > 1000)
        button_states[0].counter = 0;

    // Check for long press to enter deep sleep
    if (button_states[0].currentState == LOW && (millis() - button_states[0].lastDebounceTime) > 1000)
    {
        Serial.println("[MAIN] Program switch detected! Deep sleep mode activated...");
        // Indicate deep sleep mode with LED blink

        blink_circle(0x7, 5, 100); // Blink RGB LED in white 5 times

        for (int i = 0; i < MAJORAS_LEDS; i++)
        {
            majora.setPixelColor(i, 0); // Turn off LEDs
        }
        majora.show();

        while (digitalRead(prog_switch) == LOW)
            ;
        esp_deep_sleep_start();
    }
}

#endif