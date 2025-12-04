#if !defined(SETUP_WS2812B)
#define SETUP_WS2812B

#include "shared/dependencies.h"
#include "components/WS2812B/features/WS_functions.h"

void setup_WS2812B()
{
    Serial.println("[WS2812B] SETUP STARTED!");

    onboard_circle.begin();
    onboard_circle.clear();

    blink_circle(0x7, 2, 100); // Blink RGB LED in white 2 times to indicate WS2812B setup

    Serial.println("[WS2812B] SETUP FINISHED!");
}

#endif // SETUP_WS2812B
