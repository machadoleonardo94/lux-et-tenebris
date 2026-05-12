#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void run_majoras();
void update_onboard_LED();
void update_strip();

void run_majoras()
{
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 10)
        return;
    lastUpdate = millis();
    static uint8_t step = 0; // From 0 to 127 positions
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    float sin1 = sin(((millis() * (2 * PI)) / 1500.0));
    float sin2 = sin(((millis() * (2 * PI)) / 4000.0));
    float sin3 = sin(((millis() * (2 * PI)) / 1200.0));

    red = (40 + 80 * sin1 > 0 ? 40 + 80 * sin1 : 0);
    green = (40 + 80 * sin2 > 0 ? 40 + 80 * sin2 : 0);
    blue = (40 + 80 * sin3 > 0 ? 40 + 80 * sin3 : 0);
    uint16_t total = red + green + blue;

    for (int i = 0; i < MAJORAS_LEDS - 2; i++)
    {
        if (i == 10 || i == 11) // Removes offset and dims for first 16 LEDs
            majora.setPixelColor(i, majora.Color(red / 5, green / 10, blue / 10));
        else
            majora.setPixelColor(i, majora.Color(red, green, blue));
    }
    majora.setPixelColor(MAJORAS_LEDS - 2, red, 0, 0);
    majora.setPixelColor(MAJORAS_LEDS - 1, red, 0, 0);
    majora.show();
}

void update_strip()
{
    if (millis() - led_strip.update_time < 50)
        return;
    led_strip.update_time = millis();

    /*

    for (int i = 0; i < lenght; i++)
    {
        strip.setPixelColor(i, strip.Color(led_strip.red, led_strip.green, led_strip.blue)); // Purple
    }
    for (int i = lenght; i < NUM_LEDS; i++)
    {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();
    */
    static int led_brightness[NUM_LEDS + 1] = {0}; // Double size to do some shenanigans

    //* Sine wave brightness
    float sin1 = sin(((millis() * (2 * PI)) / 1500.0));

    for (int i = NUM_LEDS; i > 0; i--)
    {
        led_brightness[i] = led_brightness[i - 1];
        strip.setPixelColor(i, strip.Color(led_brightness[i], 0, 0));
    }
    if (sin1 * 15 + 10 > 0)
        led_brightness[0] = (sin1 * 15) + 10; // Calculate brightness based on sine wave (0-30 range)
    strip.setPixelColor(0, strip.Color(led_brightness[0], 0, 0));

    strip.show();
}

#endif // SERVICE_LED_SCRIPTS
