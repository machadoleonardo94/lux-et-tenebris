//! Warning: GoHorse Develpment method was used to create this code.
//! It's presented as an "it just works" solution for Arduino users migrating to VSCode
//! and is not intended to be a best practice example of C++ programming.

//? Lux et Portabilis - Lightpack Let it Go firmware
//? Basic devboard firmware for smart LED strips, accelerometer, buttons and servo output.

//* Dependencies
#include "shared/dependencies.h"

//? strip: the NeoPixel object for controlling external LED strip
//? led_strip: the struct holding the current state of the LED strip (color, brightness, etc.)
//? onboard_led: the NeoPixel object for controlling the onboard status LED
//? status_led: the struct holding the current state of the onboard status LED

//* ---------------------- BOOT REPORT ----------------------
//? Why THIS boot happened, captured at the top of setup() before anything else
//? can cause another one.
//! The report is printed by boot_report_print() (services/serial_outputs.h) AND
//! replayed in the periodic telemetry, because this board's console is the C3's
//? USB Serial/JTAG: it re-enumerates on every reset and DISCARDS writes made
//* before the host is listening, so the boot log alone is unreadable here. See
//? the note over that function.
//? The captured values live in shared/variables.h so the replay can reach them.

void setup()
{
  //* First statement on purpose. esp_reset_reason() reports the cause of the
  //? PREVIOUS reset, so it has to be read before this boot does anything that
  //! could overwrite it.
  boot_reset_reason = esp_reset_reason();

  //* Arm both switches before the long bring-up below: setup_WIFI() can sit in the
  //? configuration portal for WIFI_PORTAL_TIMEOUT_S, and a press during that window
  //? is only remembered because the ISRs latch it. The pins, their pulls and their
  //* trigger edges are configured there too, so the two switches are declared in
  //* one place (services/readings.h) rather than one pin per service.
  setup_button_interrupt();
  // pinMode(servo_pin, OUTPUT);
  pinMode(latch_enable, OUTPUT);
  digitalWrite(latch_enable, HIGH); // Ensure latch is high at startup
  setup_ESP32();
  onboard_led.begin();
  onboard_led.setPixelColor(0, 50, 0, 0);
  onboard_led.show();
  Serial.println("Status LED initialized");
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\nESP32-C3 Lightpack Let it Go ");
  Serial.println("===========================");

  //* The two facts a board that misbehaves on one supply and not another is
  //? asked for first: why the last run ended, and what the rail looked like
  //? before any LED drew current. Read here, at rest, so the number below is
  //* the unloaded V_REG and the self-test's own readings have a baseline.
  boot_rail_mv = supply_rail_mv();
  boot_report_print();

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Serial.println("LED strip initialized");

#if STRIP_SELFTEST_ON_BOOT
  //? Runs before the network comes up, so it works even when WiFi fails.
  strip_channel_test();
#endif

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

  //* Restore the saved mode, palette and brightness before anything renders.
  //! This used to happen inside setup_webpage(), which only runs after a
  //! successful WiFi join. A board that could not reach WiFi therefore ran on
  //! compile-time defaults and silently ignored every setting the user had
  //! saved - including the palette. Configuration is not a network feature.
  webpage_config_load();
  strip_report_config();

  //* Network bring-up, then the device configuration webpage - except that the
  //? page is no longer started from here. setup_WIFI() reads the stored offline
  //? switch and the stored credentials and leaves the board in one of three
  //* states (NET_OFFLINE / NET_CONNECTING / NET_CONNECTED); the configuration
  //? page and the OTA listener come up on the connect transition, which for an
  //? online board whose router is slow may happen a minute after boot rather
  //* than here. wifi_loop() in loop() drives that.
  //? Set enable_network to false to keep the radio off and run fully offline.
  const bool enable_network = true;

  if (!enable_network)
  {
    Serial.println("[NET] Network disabled at compile time - running offline");
    WiFi.mode(WIFI_OFF);
  }
  else
  {
    setup_WIFI(true);
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

  //* User inputs, both polled here rather than from an ISR body: the interrupts
  //* only latch an edge, and the debounce, the classification, the mode change and
  //* the power-off are all loop-level work (services/readings.h).
  //! Touch button (GPIO3): 1 s press next LED mode, 5 s press cuts the latch.
  //! The GPIO3 input is HARDWARE DAMAGED on this board - the pad still measures a
  //? clean 3.3 V but the pin reads LOW and never latches an edge (see the polarity
  //! notes in services/readings.h). This path is kept for a repaired board only;
  //* every gesture below is what the device is actually driven with now.
  check_button();
  //? Boot button (GPIO9): 1 click next mode, 2 clicks previous mode, 5 clicks
  //? power off, 3 s hold toggles the radio, 6 s hold opens the config hotspot,
  //* 10 s hold wipes the saved networks and the page login. It is the only input
  //? this board still responds to, which is why it now carries the lot.
  check_boot_button();

  //* The network, after the inputs: a 1 s press that flips offline mode is acted
  //? on in the same pass rather than the next one, and check_boot_button() itself
  //* performs the transition, so this only carries the join and the retry.
  wifi_loop();

  update_strip(mode);

  webpage_loop();

  serial_outputs();
}
