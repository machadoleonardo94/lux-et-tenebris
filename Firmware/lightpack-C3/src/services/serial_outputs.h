#if !defined(SERVICE_SERIAL_READINGS)
#define SERVICE_SERIAL_READINGS

#include "shared/dependencies.h"

//* Set to 0 to drop the replayed boot report from the periodic telemetry.
//? It costs three lines per ten seconds. Keep it on any build that might be
//! asked "why did it restart", which on a battery-powered board is every build.
#define BOOT_REPORT_REPLAY 1

//* ---------------------- BOOT REPORT ----------------------
//? Why the previous run ended, and the rail's behaviour during the strip
//? self-test. Captured at boot (main.cpp) into the globals in shared/variables.h.
//!
//! REPLAYED in the periodic telemetry below, and that is not a convenience - it
//! is the only way these lines can be read at all. Two facts about this board's
//! console force it:
//!   1. Serial is the C3's USB Serial/JTAG (ARDUINO_USB_CDC_ON_BOOT=1 with
//!      ARDUINO_USB_MODE=1, so Serial is HWCDC, not a UART - see HWCDC.h:112).
//?      Every reset re-enumerates the USB device, so the host's COM port drops
//?      and comes back exactly while setup() is printing.
//!   2. HWCDC::write() with no host attached does not buffer the bytes, it
//*      DISCARDS them: the !isCDC_Connected() branch of write() runs
//*      flushTXBuffer() (cores/esp32/HWCDC.cpp:419). So the boot-time lines are
//!      not early, they are lost, and no amount of monitor reconnecting gets
//?      them back.
//* Replaying means the report can be read by opening the monitor at ANY time and
//? waiting one telemetry period. It survives the CDC re-enumeration because it
//* is printed again after it.
const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "power-on (cold boot)";
    case ESP_RST_EXT:
        return "external reset pin";
    case ESP_RST_SW:
        return "software restart (ESP.restart)";
    case ESP_RST_PANIC:
        return "PANIC - exception or abort()";
    case ESP_RST_INT_WDT:
        return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
        return "task watchdog";
    case ESP_RST_WDT:
        return "other watchdog";
    case ESP_RST_DEEPSLEEP:
        return "wake from deep sleep";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT - the 3.3V rail collapsed";
    case ESP_RST_SDIO:
        return "reset over SDIO";
    default:
        return "unknown";
    }
}

void boot_report_print()
{
    Serial.printf("[BOOT] reset reason: %s\n", reset_reason_name(boot_reset_reason));

    //! This is V_REG, the LED rail - NOT the rail that powers the module. The
    //? R16-R19 divider is tapped UPSTREAM of D1, and the module sits one diode
    //! drop below that. Nothing in the firmware can see that drop, so a healthy
    //* number here does NOT rule out a brown-out on the module's own supply, and
    //? reading this line as "the supply is fine" is exactly the mistake it is
    //* here to prevent.
    Serial.printf("[BOOT] V_REG (LED rail, upstream of D1): %lu mV\n",
                  (unsigned long)boot_rail_mv);

    //? The gap between these two numbers is the health metric: it is how far the
    //! strip's own current drags the rail the module's regulator is fed from
    //! (V_REG -> D1 -> the module's 5V pin). A sag of a few hundred mV is the
    //? signature of a supply path that cannot afford the LED load.
    //!
    //! A minimum at or above the baseline is NOT a small sag, and printing it as
    //! a negative one was misleading. It means the rail ROSE while the strip lit
    //? up, which the strip cannot cause: it means the module's own load fell by
    //? more than the LEDs added - the radio going quiet, or the CPU dropping out
    //* of setup()'s work into a delay(). The two readings were then taken at
    //* different loads and subtracting them measures nothing. Ambiguous beats
    //! wrong, so this says so instead.
    if (boot_rail_min_mv == 0)
        Serial.println("[BOOT]   lowest under the strip self-test: not measured");
    else if (boot_rail_min_mv >= boot_rail_mv)
        Serial.printf("[BOOT]   under the strip self-test: %lu mV - no sag against the\n"
                      "[BOOT]     baseline. The rail ROSE, so the readings were taken at\n"
                      "[BOOT]     different module loads; the difference means nothing.\n",
                      (unsigned long)boot_rail_min_mv);
    else
        Serial.printf("[BOOT]   lowest under the strip self-test: %lu mV (sag %ld mV)\n",
                      (unsigned long)boot_rail_min_mv,
                      (long)boot_rail_mv - (long)boot_rail_min_mv);

    //* A brown-out reset is the one line that always deserves spelling out,
    //? because the cause is never in the firmware and the fix is never a reflash.
    if (boot_reset_reason == ESP_RST_BROWNOUT)
        Serial.println("[BOOT]   -> BROWNOUT: the supply sagged below the detector threshold.");

    //* The way back in, printed with the report and therefore replayed with it.
    //? A user who has lost the page is exactly the user who cannot read this off
    //* the page, and the boot log alone is unreadable on this board's console.
    //* The whole input map, because GPIO9 is now the board's ONLY control - the
    //! GPIO3 touch input is hardware-damaged (see services/readings.h), so a user
    //* who cannot work this switch cannot work the device at all.
    Serial.println("[BOOT] GPIO9: 1 click next mode, 2 clicks previous mode, "
                   "5 clicks power off; 3 s hold radio on/off, "
                   "6 s hold config hotspot, 10 s hold clears the saved wifi "
                   "and the login.");
    Serial.println("[BOOT] GPIO9 while the config hotspot is up: 3 s hold skips it.");
}

void serial_outputs()
{
    loopCounter++;

    //? Previous detector sample count, so the report below can turn the counter into
    //? a rate over the same 10 s window the loop count uses.
    static uint32_t impact_samples_last = 0;

    if (millis() - loopTimer > 10000)
    {
        Serial.printf("\n\nTotal main loops per 10 seconds: %d \n", loopCounter);
        loopTimer = millis();
        loopCounter = 0;

#if BOOT_REPORT_REPLAY
        //* Replayed once per telemetry period, not just at boot - see the note
        //! over boot_report_print() above. Connect the monitor at any time, wait
        //* up to one period, and the last boot's report is there.
        boot_report_print();
#endif

        String tempIP = WiFi.localIP().toString();
        int8_t wifi_rssi = WiFi.RSSI();

        char macBuf[18];
        snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 macAdress[0], macAdress[1], macAdress[2],
                 macAdress[3], macAdress[4], macAdress[5]);
        String tempMac = String(macBuf);

        Serial.printf("\n\n");
        Serial.printf("->| HARDWARE | IP: %s | MAC: %s | RSSI: %d | \n", tempIP, tempMac.c_str(), wifi_rssi);
        Serial.printf("->| GYRO     | Accel_X: %.2f | Accel_Y: %.2f | Accel_Z: %.2f |\n",
                      a.acceleration.x, a.acceleration.y, a.acceleration.z);
        Serial.printf("->| GYRO     | Gyro_X: %.2f  | Gyro_Y: %.2f  | Gyro_Z: %.2f |\n",
                      g.gyro.x, g.gyro.y, g.gyro.z);

        //* Detector health. The rate the loop actually achieved is not something the
        //* firmware can assume: with WiFi, a web request or an OTA transfer in
        //* flight, loop() can run far slower than the 500 Hz the detector targets,
        //! and a rate below ~100 Hz is what makes impacts get missed or undersized.
        const uint32_t samples = impact_samples - impact_samples_last;
        impact_samples_last = impact_samples;
        if (impact_count == 0)
            Serial.printf("->| IMPACT   | %lu samples/s | none detected yet |\n",
                          (unsigned long)(samples / 10UL));
        else
            Serial.printf("->| IMPACT   | %lu samples/s | %lu total | last magnitude %u, %lu ms ago |\n",
                          (unsigned long)(samples / 10UL), (unsigned long)impact_count,
                          impact_last_magnitude, (unsigned long)(millis() - impact_last_ms));
        Serial.printf("\n\n");
    }

    //* Teleplot outputs
    /*
    Serial.printf(">acc_x: %.2f \n", a.acceleration.x);
    Serial.printf(">acc_y: %.2f \n", a.acceleration.y);
    Serial.printf(">acc_z: %.2f \n", a.acceleration.z);
    Serial.printf(">gyro_x: %.2f \n", g.gyro.x);
    Serial.printf(">gyro_y: %.2f \n", g.gyro.y);
    Serial.printf(">gyro_z: %.2f \n", g.gyro.z);
    */
}

#endif // SERVICE_SERIAL_READINGS
