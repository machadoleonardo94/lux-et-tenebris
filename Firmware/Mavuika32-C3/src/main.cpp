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
  setup_ESP32();
  onboard_led.begin();
  onboard_led.setPixelColor(0, 100, 0, 0);
  onboard_led.show();
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nESP32-C3 Mavuika Let it Go ");
  Serial.println("===========================");

  //* Restore the saved mode, palette and brightness before anything renders.
  //! This used to happen inside setup_webpage(), which only runs after a
  //! successful WiFi join. A board that could not reach WiFi therefore ran on
  //! compile-time defaults and silently ignored every setting the user had
  //! saved - including the palette. Configuration is not a network feature.
  webpage_config_load();
  strip_report_config();

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED strip initialized");

#if STRIP_SELFTEST_ON_BOOT
  //? Runs before the network comes up, so it works even when WiFi fails.
  strip_channel_test();
#endif

  Serial.println("Status LED initialized");

  //? The return value used to be discarded, which made a bus that could not be claimed at all
  //! indistinguishable from a sensor that is simply not answering.
  if (!Wire.begin(i2c_sda, i2c_scl, 100000))
    Serial.printf("I2C FAILED to initialize (SDA=%d, SCL=%d)\n", i2c_sda, i2c_scl);
  else
    Serial.printf("I2C initialized (SDA=%d, SCL=%d)\n", i2c_sda, i2c_scl);

  //? A slave stuck holding SDA low must not hang the scan. The ESP32 Wire default is already
  //? 50 ms per transaction; it is set explicitly so a future change here is visible in the log.
  Wire.setTimeOut(50);

  //* Bus scan first: it is what separates "no sensor" from "sensor on another address" from
  //* "sensor present but not an MPU6050". See services/i2c_scan.h.
  i2c_scan_report();

  //* MPU6050: AD0 picks 0x68 or 0x69 and this board does not route AD0, so both are probed.
  uint8_t mpu_address = 0;
  gyro_started = mpu_begin_auto(mpu_address);

  if (gyro_started)
    Serial.printf("IMU initialized at 0x%02X\n", mpu_address);
  else
  {
    Serial.println("Failed to find MPU6050 chip");
    Serial.println("[IMU] The scan above says which of the causes it was:");
    Serial.println("[IMU]   empty scan           -> wiring, power or pull-ups");
    Serial.println("[IMU]   ACK at 0x68/0x69 only -> address was never the problem");
    Serial.println("[IMU]   unsupported WHO_AM_I  -> neither the MPU6050 nor the MPU6500 family");
  }

  for (int k = 0; k < 5; k++)
  {
    if (!gyro_started)
      onboard_led.setPixelColor(0, strip.Color(50 * (k % 2), 0, 0)); // Red
    else
      onboard_led.setPixelColor(0, strip.Color(0, 50 * (k % 2), 0)); // Green
    onboard_led.show();
    delay(200);
  }

  //* Network bring-up, then the device configuration webpage.
  //? setup_WIFI() runs WiFiManager (config hotspot for WIFI_PORTAL_TIMEOUT_S on
  //? first boot or when the saved network is unreachable). setup_webpage() then
  //? binds port 80 for the persistent configuration page and restores the
  //? saved mode/config.
  //? Set enable_network to false to keep the radio off and run fully offline.
  const bool enable_network = true;

  if (!enable_network)
  {
    Serial.println("[NET] Network disabled at compile time - running offline");
    WiFi.mode(WIFI_OFF);
  }
  else if (setup_WIFI(true))
  {
    setup_webpage();
  }
  else
  {
    //! Distinguish a failed join from a deliberate shutdown. This used to print
    //! "Network disabled", which sent you looking for a compile-time flag when
    //! the real cause was that the board could not join its saved network.
    Serial.println("[NET] WiFi FAILED - no AP and no webpage from here on.");
    Serial.printf("[NET] The config hotspot was only up for %d s from boot and has now closed.\n",
                  WIFI_PORTAL_TIMEOUT_S);
    Serial.println("[NET] Reset the board to reopen it, or read the [WIFI] scan above.");
    WiFi.mode(WIFI_OFF);
  }
}

void loop()
{
  // power = (sin(millis() / 200.0) * 15) + 15; // Calculate brightness based on sine wave (0-30 range)

  ArduinoOTA.handle();
  read_adc();

  /* Get new sensor events with the readings */
  if (gyro_started)
    mpu.getEvent(&a, &g, &temp);

  //* Impact detection runs here, not inside an LED mode. It used to be sampled from
  //* flame_steps() at the LED frame rate and only while that mode was selected,
  //! which is 20 Hz and no detection at all under any other mode. impact_poll()
  //* rate-limits itself to IMPACT_SAMPLE_US and latches its result for whichever
  //* mode is drawing.
  if (gyro_started)
    impact_poll();

  update_onboard_LED();

  update_strip(mode);

  webpage_loop();

  serial_outputs();

  if ((millis() > 5000) && (digitalRead(button_pin) == HIGH))
  {
    Serial.println("Button pressed!");
    onboard_led.setPixelColor(0, strip.Color(55, 0, 30));
    onboard_led.show();
    delay(1000);                     // Debounce delay
    digitalWrite(latch_enable, LOW); // Deactivate latch to shutdown
    delay(100);                      // Wait for latch to settle
    Serial.printf("Shutting down at %lu ms\n", millis());
    ESP.restart(); // Restart the ESP32-C3
  }
}
