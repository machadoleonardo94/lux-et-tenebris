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
    }
    button_states[0].lastState = button_states[0].currentState;

    // Check for long press to enter deep sleep
    if (button_states[0].currentState == LOW && (millis() - button_states[0].lastDebounceTime) > 1000)
    {
        // Indicate deep sleep mode with LED blink
        for (int j = 0; j < 10; j++)
        {
            for (int i = 0; i < ONBOARD_LEDS; i++)
            {
                onboard_circle.setPixelColor(i, onboard_circle.Color(100 * ((j + 1) % 2), 0, 0)); // Red color
            }
            onboard_circle.show();
            delay(150);
        }
        Serial.println("[MAIN] Program switch detected! Deep sleep mode activated...");
        while (digitalRead(prog_switch) == LOW)
            ;
        esp_deep_sleep_start();
    }
}

#endif