#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void update_rgb_values(uint8_t colorCode);
void update_channel1_states();
void update_channel2_states();
void update_channel3_states();

void update_rgb_values(uint8_t stripAdress)
{
    strip[stripAdress].red = (strip[stripAdress].color & 0x1) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].green = (strip[stripAdress].color & 0x2) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].blue = (strip[stripAdress].color & 0x4) ? strip[stripAdress].brightness : 0;
    return;
}

void update_channel1_states()
{
    if (millis() - strip[ZONAS].update_time < 100)
        return;
    strip[ZONAS].update_time = millis();

    for (int i = 0; i < 8; i++)
    {
        update_rgb_values(ZONAS);
        led_zonas.setPixelColor(i, led_zonas.Color(strip[ZONAS].red, strip[ZONAS].green, strip[ZONAS].blue));
        led_zonas.show(); // update to the WS2812B Led Strip
        strip[ZONAS].color++;
        if (strip[ZONAS].color > 7) // If last 3 bits are zero, go back to red
            strip[ZONAS].color = 1;
    }
    strip[ZONAS].color %= 8;

    // Brightness oscillates between 20 and 120 with a period of 4 seconds
    strip[ZONAS].brightness = 70 + 50 * sin(((millis() * (2 * PI)) / 4000.0));
    // Serial.printf("Brightness ZONAS: %d \n", strip[ZONAS].brightness);
    //   if (strip[ZONAS].brightness >= 120)
    //       strip[ZONAS].brightness = 5;
}

void update_channel2_states()
{
    if (millis() - strip[BAR1].update_time < 350)
        return;
    strip[BAR1].update_time = millis();
    strip[BAR1].brightness = 50; // Fixed brightness for bar1
    static uint8_t empty_pos = 0;
    static uint8_t direction = 1;
    for (int i = 0; i < 4; i++)
    {
        update_rgb_values(BAR1);
        if (i != empty_pos)
            led_bar1.setPixelColor(i, led_bar1.Color(strip[BAR1].red, strip[BAR1].green, strip[BAR1].blue));
        else
            led_bar1.setPixelColor(i, 0, 0, 0); // turn off the "empty" pixel
        led_bar1.show();                        // update to the WS2812B Led Strip
    }

    empty_pos += direction;
    if (empty_pos == 3 || empty_pos == 0)
    {
        direction = -direction;
        strip[BAR1].color++;
    }

    if (strip[BAR1].color > 15)
        strip[BAR1].color -= 7;
}

void update_channel3_states()
{
    if (millis() - strip[BAR2].update_time < 150)
        return;
    strip[BAR2].update_time = millis();
    strip[BAR2].brightness = 25; // Fixed brightness for bar2
    static uint8_t empty_pos = 0;
    static uint8_t direction = 1;

    for (int i = 0; i < 4; i++)
    {
        update_rgb_values(BAR2);
        if (i != empty_pos)
            led_bar2.setPixelColor(i, led_bar2.Color(strip[BAR2].red, strip[BAR2].green, strip[BAR2].blue));
        else
            led_bar2.setPixelColor(i, 0, 0, 0); // turn off the "empty" pixel
        led_bar2.show();                        // update to the WS2812B Led Strip
    }

    empty_pos += direction;
    if (empty_pos == 3 || empty_pos == 0)
    {
        direction = -direction;
        strip[BAR2].color++;
    }

    // strip[BAR2].color++;
    if (strip[BAR2].color > 15)
        strip[BAR2].color -= 7;
}

#endif // SERVICE_LED_SCRIPTS
