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
  pinMode(button_pin, INPUT_PULLDOWN);
  // pinMode(servo_pin, OUTPUT);
  pinMode(latch_enable, OUTPUT);
  digitalWrite(latch_enable, HIGH); // Ensure latch is high at startup
  onboard_led.begin();
  onboard_led.setPixelColor(0, 100, 0, 0);
  onboard_led.show();
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nESP32-C3 Mavuika Let it Go ");
  Serial.println("===========================");

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED strip initialized");

  Serial.println("Status LED initialized");

  Wire.begin(i2c_sda, i2c_scl);
  Serial.printf("I2C initialized (SDA=%d, SCL=%d)\n", i2c_sda, i2c_scl);

  if (mpu.begin(0x68, &Wire))
  {
    gyro_started = true;
    Serial.println("MPU6050 found and initialized");
  }
  else
    Serial.println("Failed to find MPU6050 chip");

  for (int k = 0; k < 5; k++)
  {
    if (!gyro_started)
      onboard_led.setPixelColor(0, strip.Color(50 * (k % 2), 0, 0)); // Red
    else
      onboard_led.setPixelColor(0, strip.Color(0, 50 * (k % 2), 0)); // Green
    onboard_led.show();
    delay(200);
  }

  // setup_WIFI();
  WiFi.mode(WIFI_OFF);
}

void loop()
{
  // power = (sin(millis() / 200.0) * 15) + 15; // Calculate brightness based on sine wave (0-30 range)

  /* Get new sensor events with the readings */
  if (gyro_started)
    mpu.getEvent(&a, &g, &temp);

  update_onboard_LED();

  // update_strip();
  // run_majoras();
  flame_steps();

  serial_outputs();

  if ((millis() > 5000) && (digitalRead(button_pin) == HIGH))
  {
    Serial.println("Button pressed!");
    onboard_led.setPixelColor(0, strip.Color(55, 0, 30));
    onboard_led.show();
    delay(1000);                     // Debounce delay
    digitalWrite(latch_enable, LOW); // Deactivate latch to shutdown
  }
}
