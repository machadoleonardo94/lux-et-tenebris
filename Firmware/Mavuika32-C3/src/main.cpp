//! Warning: GoHorse Develpment method was used to create this code.
//! It's presented as an "it just works" solution for Arduino users migrating to VSCode
//! and is not intended to be a best practice example of C++ programming.

//? Lux et Portabilis - Mavuika Let it Go firmware
//? Basic devboard firmware for smart LED strips, accelerometer, buttons and servo output.

//* Dependencies
#include "shared/dependencies.h"

//? strip: the NeoPixel object for controlling external LED strip
//? led_strip: the struct holding the current state of the LED strip (color, brightness, etc.)
//? onboard_led: the NeoPixel object for controlling the onboard status LED
//? status_led: the struct holding the current state of the onboard status LED

void setup()
{
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nESP32-C3 LED Control Started");
  Serial.println("===========================");

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED strip initialized");

  onboard_led.begin();
  onboard_led.show(); // Initialize all pixels to 'off'
  Serial.println("Status LED initialized");

  Wire.begin(i2c_sda, i2c_scl);
  Serial.printf("I2C initialized (SDA=%d, SCL=%d)\n", i2c_sda, i2c_scl);

  if (!mpu.begin(0x68, &Wire))
  {
    Serial.println("Failed to find MPU6050 chip");
    for (int k = 0; k < 5; k++)
    {
      onboard_led.setPixelColor(0, strip.Color(50 * (k % 2), 0, 0)); // Red
      onboard_led.show();
      delay(200);
    }
  }
  else
  {
    Serial.println("MPU6050 found and initialized");
    gyro_started = true;
  }
  // setup_WIFI();
}

void loop()
{
  // power = (sin(millis() / 200.0) * 15) + 15; // Calculate brightness based on sine wave (0-30 range)

  /* Get new sensor events with the readings */
  if (gyro_started)
    mpu.getEvent(&a, &g, &temp);

  update_onboard_LED();

  update_strip();

  serial_outputs();
}
