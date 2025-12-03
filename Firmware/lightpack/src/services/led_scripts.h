#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void setup_12V_sources();
void update_rgb_values(uint8_t colorCode);
void light_circle();
void update_channel0_states();
void update_channel1_states();
void update_channel2_states();
void update_channel3_states();
void run_majoras();

void setup_12V_sources()
{
    pinMode(CC_output, OUTPUT);
    digitalWrite(CC_output, LOW); // Disables 12V power controled current source (up to 600mA)
    pinMode(openDrain_output1, OUTPUT);
    digitalWrite(openDrain_output1, LOW); // Disable open drain output 1 (R)
    pinMode(openDrain_output2, OUTPUT);
    digitalWrite(openDrain_output2, LOW); // Disable open drain output 2 (G)
    pinMode(openDrain_output3, OUTPUT);
    digitalWrite(openDrain_output3, LOW); // Disable open drain output 3 (B)
}

void update_rgb_values(uint8_t stripAdress)
{
    strip[stripAdress].red = (strip[stripAdress].color & 0x1) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].green = (strip[stripAdress].color & 0x2) ? strip[stripAdress].brightness : 0;
    strip[stripAdress].blue = (strip[stripAdress].color & 0x4) ? strip[stripAdress].brightness : 0;
    return;
}

void update_channel0_states()
{
    if (millis() - onboard_led.update_time < 10)
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

    for (int i = 0; i < MAJORAS_LEDS - 2; i++)
    {
        float sin1 = sin(((millis() * (2 * PI)) / 1500.0));
        float sin2 = sin(((millis() * (2 * PI)) / 4000.0));
        float sin3 = sin(((millis() * (2 * PI)) / 1200.0));

        red = 30 + 40 * (sin1 > 0 ? sin1 : 0);
        green = 10 + 70 * (sin2 > 0 ? sin2 : 0);
        blue = 20 + 70 * (sin3 > 0 ? sin3 : 0);

        majora.setPixelColor(i, majora.Color(red, green, blue));
    }
    majora.setPixelColor(MAJORAS_LEDS - 2, red, 0, 0);
    majora.setPixelColor(MAJORAS_LEDS - 1, red, 0, 0);
    majora.show();
    // Serial.printf("[MAJORA] Step %d executed.\n", step);

    step = (step + 1) % 127;
}

void update_channel1_states()
{
    if (millis() - strip[ZONAS].update_time < 100)
        return;
    strip[ZONAS].update_time = millis();
}

void update_channel2_states()
{
    if (millis() - strip[BAR1].update_time < 350)
        return;
    strip[BAR1].update_time = millis();
}

void update_channel3_states()
{
    if (millis() - strip[BAR2].update_time < 150)
        return;
    strip[BAR2].update_time = millis();
}

#endif // SERVICE_LED_SCRIPTS
