#if !defined(SETUP_ESP32)
#define SETUP_ESP32

#include "shared/dependencies.h"
#include "components/ESP32/features/update_firmware_ota.h"

//! There is deliberately no ADC state here any more - see the note in
//! setup_ESP32() below and read_adc() in services/readings.h.

void setup_ESP32()
{
    Serial.println("[ESP32] SETUP STARTED!");

    //* Setup bluetooth
    btStop();

    //* Setup Watchdog
    // esp_task_wdt_init(180, true);
    // esp_task_wdt_add(NULL);

    WiFi.mode(WIFI_AP_STA);

    // esp_sleep_enable_ext0_wakeup(prog_switch, LOW);

    setup_OTA();

    //! Do NOT configure the ADC here. This used to run the legacy ESP-IDF driver:
    //!     adc1_config_width(ADC_WIDTH_BIT_12);
    //!     adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12);
    //!     esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_cal);
    //! and it hard-locked the board at boot, before a single LED was lit:
    //!     E (215) ADC: CONFLICT! driver_ng is not allowed to be used with the legacy driver
    //!     abort() was called at PC 0x42023589 on core 0   (boot loop)
    //!
    //! The two ADC drivers cannot coexist in one image. When the oneshot driver
    //! is linked in, the legacy entry points call check_adc_oneshot_driver_conflict()
    //! (esp_adc/adc_legacy.c), which logs that message and calls abort() outright -
    //! it is not a recoverable error code. The oneshot driver IS linked, because
    //! the Arduino core is built on it: analogRead() -> __analogInit() ->
    //! adc_oneshot_new_unit() (cores/esp32/esp32-hal-adc.c in the framework
    //! pinned by platformio.ini). So a single legacy call anywhere is fatal.
    //?
    //? The core owns the ADC now. read_adc() uses analogRead() for the raw count
    //? and analogReadMilliVolts() for the calibrated millivolts, which the core
    //? services through adc_oneshot_get_calibrated_result() against a
    //? curve-fitting calibration it creates itself. Its default attenuation is
    //? 11 dB, the same hardware setting the old ADC_ATTEN_DB_12 asked for, and
    //? the board's /4 divider keeps the rail well inside that range - so nothing
    //? here needs replacing, it needs leaving alone.

    Serial.println("[ESP32] SETUP FINISHED!");
}

#endif // SETUP_ESP32