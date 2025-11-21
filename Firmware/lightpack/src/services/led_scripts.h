#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void update_rgb_values(uint8_t colorCode);
void update_channel0_states();
void update_channel1_states();
void update_channel2_states();
void update_channel3_states();

void light_circle();

void update_strip_color(uint8_t stripAdress)
{
    strip[stripAdress].red = (strip[stripAdress].color & 0x1) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].green = (strip[stripAdress].color & 0x2) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].blue = (strip[stripAdress].color & 0x4) ? strip[stripAdress].brightness : 0;
    return;
}

void update_channel0_states()
{
    if (millis() - onboard_led.update_time < 5)
        return;
    onboard_led.update_time = millis();

    light_circle();
}

void light_circle()
{
    float brightness = 50;
    // brightness = sin(((millis() * (2 * PI)) / 1500.0)) * 30 + 50;
    onboard_led.brightness = constrain(brightness, 0, 150);
    onboard_led.color = millis() / 1000 % 7 + 1; // Change color every 1500ms
    bool hasRed = onboard_led.color & 0x1;
    bool hasGreen = onboard_led.color & 0x2;
    bool hasBlue = onboard_led.color & 0x4;
    uint8_t numColors = hasRed + hasGreen + hasBlue; // Normalizes intensity based on number of active colors
    onboard_led.red = hasRed ? onboard_led.brightness / numColors : 0;
    onboard_led.green = hasGreen ? onboard_led.brightness / numColors : 0;
    onboard_led.blue = hasBlue ? onboard_led.brightness / numColors : 0;
    uint8_t fade = 15;

    onboard_led.index = millis() / 50 % ONBOARD_LEDS + ONBOARD_LEDS; // Tagets a pixel between 0 and ONBOARD_LEDS every 150ms, plus offset

    onboard_circle.setPixelColor(onboard_led.index % ONBOARD_LEDS, onboard_circle.Color(onboard_led.red, onboard_led.green, onboard_led.blue));
    onboard_circle.setPixelColor((onboard_led.index - 1) % ONBOARD_LEDS, onboard_circle.Color(onboard_led.red / fade, onboard_led.green / fade, onboard_led.blue / fade));
    onboard_circle.setPixelColor((onboard_led.index - 2) % ONBOARD_LEDS, onboard_circle.Color(0, 0, 0));

    onboard_circle.show(); // update to the WS2812B Led Strip
}

void update_channel1_states()
{
    if (millis() - strip[ZONAS].update_time < 100)
        return;
    strip[ZONAS].update_time = millis();

    for (int i = 0; i < 8; i++)
    {
        update_strip_color(ZONAS);
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
    strip[BAR1].brightness = 255; // Fixed brightness for bar1
    static uint8_t empty_pos = 0;
    static uint8_t direction = 1;
    for (int i = 0; i < 4; i++)
    {
        update_strip_color(BAR1);
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
        update_strip_color(BAR2);
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
