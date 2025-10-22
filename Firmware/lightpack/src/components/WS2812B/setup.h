#if !defined(SETUP_WS2812B)
#define SETUP_WS2812B

#include "shared/dependencies.h"
#include "components/WS2812B/features/WS_functions.h"

void setup_WS2812B()
{
    Serial.println("[WS2812B] SETUP STARTED!");

    led_zonas.begin();
    led_zonas.clear();

    led_bar1.begin();
    led_bar1.clear();
    led_bar2.begin();
    led_bar2.clear();

    for (int zone = 0; zone < 8; zone++)
    {
        uint8_t red = (zone & 0x1) ? 50 : 0;
        uint8_t green = (zone & 0x2) ? 50 : 0;
        uint8_t blue = (zone & 0x4) ? 50 : 0;
        led_zonas.setPixelColor(zone, led_zonas.Color(red, green, blue)); // it only takes effect if pixels.show() is called
        led_zonas.show();                                                 // update to the WS2812B Led Strip
        delay(50);
    }

    for (int bar = 0; bar < 4; bar++)
    {
        led_bar1.setPixelColor(bar, led_bar1.Color(0, 50, 0)); // it only takes effect if pixels.show() is called
        led_bar1.show();                                       // update to the WS2812B Led Strip
        delay(50);
    }
    for (int bar = 0; bar < 4; bar++)
    {
        led_bar2.setPixelColor(bar, led_bar2.Color(0, 0, 50)); // it only takes effect if pixels.show() is called
        led_bar2.show();                                       // update to the WS2812B Led Strip
        delay(50);
    }

    Serial.println("[WS2812B] SETUP FINISHED!");
}

#endif // SETUP_WS2812B
