#if !defined(SETUP_WS2812B)
#define SETUP_WS2812B

#include "shared/dependencies.h"
#include "components/WS2812B/features/WS_functions.h"

void setup_WS2812B()
{
    Serial.println("[WS2812B] SETUP STARTED!");

    onboard_circle.begin();
    onboard_circle.clear();

    for (int j = 0; j < 6; j++)
    {
        for (int i = 0; i < ONBOARD_LEDS; i++)
        {
            onboard_circle.setPixelColor(i, onboard_circle.Color(0, 100 * ((j + 1) % 2), 0)); // Green color
        }
        onboard_circle.show();
        delay(200);
    }

    Serial.println("[WS2812B] SETUP FINISHED!");
}

#endif // SETUP_WS2812B
