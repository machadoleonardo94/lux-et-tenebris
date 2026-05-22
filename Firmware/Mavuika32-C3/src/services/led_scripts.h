#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void run_majoras();
void update_onboard_LED();
void update_strip();
void flame_steps();

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

void update_onboard_LED()
{
    if (millis() - status_led.update_time < 150)
        return;
    status_led.update_time = millis();
    status_led.red = abs(g.gyro.x);
    status_led.green = abs(g.gyro.y);
    status_led.blue = abs(g.gyro.z);

    onboard_led.setPixelColor(0, strip.Color(status_led.red, status_led.green, status_led.blue)); // Green status LED with same brightness
    onboard_led.show();
    /*
    Serial.printf(">Gyro_X: %.2f \n", g.gyro.x);
    Serial.printf(">Gyro_Y: %.2f \n", g.gyro.y);
    Serial.printf(">Gyro_Z: %.2f \n", g.gyro.z);
    Serial.printf(">Accel_X: %.2f \n", a.acceleration.x);
    Serial.printf(">Accel_Y: %.2f \n", a.acceleration.y);
    Serial.printf(">Accel_Z: %.2f \n", a.acceleration.z);
    */
}

void flame_steps()
{
    if (millis() - led_strip.update_time < 50)
        return;
    led_strip.update_time = millis();

    static float magnitude = 0;
    magnitude = max(magnitude, (float)detectSingleStep(a.acceleration.x, a.acceleration.y, a.acceleration.z));
    magnitude *= 0.90f; // Decay over time
    Serial.printf("Step Magnitude: %.2f \n", magnitude);

    if (magnitude > 1)
    {
        lenght = 37 - magnitude;
        power = magnitude * 5;
        if (power > 75)
            power = 75;
        if (lenght > NUM_LEDS)
            lenght = NUM_LEDS;

        led_strip.red = power;
        led_strip.green = 0;
        led_strip.blue = power;
        Serial.printf("Flame Length: %d \n", lenght);
        Serial.printf("Flame Power: %d \n", power);
    }
    else
    {
        lenght = 37; // Dim when stationary
        power = 5;   // Dim when stationary
    }

    for (int i = 0; i < lenght; i++)
    {
        strip.setPixelColor(i, strip.Color(led_strip.red, led_strip.green, led_strip.blue)); // Purple
    }
    for (int i = lenght; i < NUM_LEDS; i++)
    {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();
}
void update_strip()
{
    if (millis() - led_strip.update_time < 50)
        return;
    led_strip.update_time = millis();

    max_gyro_x *= 0.9; // Decay over time
    max_gyro_y *= 0.9;
    max_gyro_z *= 0.9;
    // Update hold values
    max_gyro_x = max(max_gyro_x, abs(g.gyro.x));
    max_gyro_y = max(max_gyro_y, abs(g.gyro.y));
    max_gyro_z = max(max_gyro_z, abs(g.gyro.z));

    if (max_gyro_z > 0.2)
    {
        lenght = 20 * max_gyro_x + 20 * max_gyro_y + 15 * max_gyro_z + 10;
        power = 2 * max_gyro_x + 2 * max_gyro_y + 5;
        led_strip.red = max_gyro_x * 7;
        led_strip.green = max_gyro_y * 7;
        led_strip.blue = max_gyro_z * 7;
    }
    else
    {
        lenght = 5; // Dim when stationary
        power = 5;  // Dim when stationary
    }

    for (int i = 0; i < lenght; i++)
    {
        strip.setPixelColor(i, strip.Color(led_strip.red, led_strip.green, led_strip.blue)); // Purple
    }
    for (int i = lenght; i < NUM_LEDS; i++)
    {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();

    //* Sine wave brightness
    /*
    strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
    strip.show();
    position++;
    position %= NUM_LEDS;
    */
}

#endif // SERVICE_LED_SCRIPTS
